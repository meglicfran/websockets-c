#include "server.h"
#include "utils.h"


int start_listener(){
    struct addrinfo hints, *res, *p;
    int status, listenerfd=-1;
    int yes=1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, PORT, &hints, &res)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return -1;
    }

    for (p=res; p!=NULL; p=p->ai_next){
        //Create a socket
        if ((listenerfd=socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("listener");
            continue;
        }

        //Lose the "address already in use" error msg.
        setsockopt(listenerfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        
        //Bind the socet to the addres/port
        if (bind(listenerfd, p->ai_addr, p->ai_addrlen) == -1){
            perror("bind");
            continue;
        }

        //Start listener
        if (listen(listenerfd, BACKLOG) == -1){
            perror("listener");
            continue;
        }

        break;
    }
    if(p==NULL){
        fprintf(stderr, "Can't make listener\n");
        return -1;
    }

    freeaddrinfo(res);

    return listenerfd;
}

//returns -1 on error, 0 otherwise
int parse_http_headers(char* data, int datalen, struct http_header* header){
    char buf[datalen];
    int lencounter=0;

    int i=0;

    //Parsing the method
    for(;i<datalen; i++){
        if(data[i]!=' '){
            buf[lencounter]=data[i];
            lencounter++;
        }else{
            buf[lencounter]='\0';
            header->methodlen = lencounter+1;
            header->method = malloc(sizeof(char)*(lencounter+1));
            copyCharArr(buf, (header->method), lencounter+1);
            i++;
            break;
        }
    }
    if(i>=datalen || buf[lencounter] != '\0' || strcmp(buf,"GET")!=0){
        return -1;
    }
    lencounter=0;

    //Parsing url
    for(; i<datalen; i++){
        if(data[i]!=' '){
            buf[lencounter]=data[i];
            lencounter++;
        }else{
            buf[lencounter]='\0';
            header->urllen = lencounter+1;
            header->url = malloc(sizeof(char)*(lencounter+1));
            copyCharArr(buf, (header->url), lencounter+1);
            i++;
            break;
        }
    }
    if(i>=datalen || buf[lencounter] != '\0'){
        return -1;
    }
    lencounter=0;
    
    //Parsing protocol
    for(; i<datalen; i++){
        if(data[i]!='\r'){
            buf[lencounter]=data[i];
            lencounter++;
        }else{
            buf[lencounter]='\0';
            header->protocollen = lencounter+1;
            header->protocol = malloc(sizeof(char)*(lencounter+1));
            copyCharArr(buf, (header->protocol), lencounter+1);
            i+=2;
            break;
        }
    }
    lencounter=0;

    header->headers = malloc(sizeof(struct key_val));
    memset(header->headers, 0, sizeof(struct key_val));
    header->headers->next_key_val=NULL;
    struct key_val* cur = header->headers;
    //Parsing key-vals
    while(1){
        //parsing key
        for(; i<datalen; i++){
            if(data[i]!=':'){
                buf[lencounter]=data[i];
                lencounter++;
            }else{
                buf[lencounter]='\0';
                cur->keylen = lencounter+1;
                cur->key = malloc(sizeof(char)*(lencounter+1));
                copyCharArr(buf, (cur->key), lencounter+1);
                i+=2;
                break;
            }
        }
        lencounter=0;

        //parsing val
        for(; i<datalen; i++){
            if(data[i]!='\r'){
                buf[lencounter]=data[i];
                lencounter++;
            }else{
                buf[lencounter]='\0';
                cur->vallen = lencounter+1;
                cur->val = malloc(sizeof(char)*(lencounter+1));
                copyCharArr(buf, (cur->val), lencounter+1);
                i+=2;
                break;
            }
        }
        lencounter=0;

        //end of data
        if(i>=datalen || data[i]=='\r'){
            break;
        }else{ // 
            struct key_val* new = malloc(sizeof(struct key_val));
            memset(new, 0, sizeof(struct key_val));
            new -> next_key_val=NULL;
            cur -> next_key_val = new;
            cur = new;
        }
    }
    return 0;
}

void send_handshake_response(int sockfd, unsigned char* base64, int base64len){
    char* first = "HTTP/1.1 101 Switching Protocols\r\n";
    char* second = "Connection: Upgrade\r\n";
    char* third = "Sec-WebSocket-Accept:";
    char* fourth = "Upgrade: websocket\r\n";
    char* end ="\r\n";

    char* header ="";
    header = concat(header, strlen(header), first, strlen(first));
    header = concat(header, strlen(header), second, strlen(second));
    header = concat(header, strlen(header), third, strlen(third));
    header = concat(header, strlen(header), base64, strlen(base64));
    header = concat(header, strlen(header), end, strlen(end));    
    header = concat(header, strlen(header), fourth, strlen(fourth));
    header = concat(header, strlen(header), end, strlen(end));
    printf("Handshake response:\n"); 
    printf("--------------------------------------------\n");
    printf("%s", header);
    printf("--------------------------------------------\n");

    if(send(sockfd, header, strlen(header),0) == -1){
        perror("send_handshake_response");
    }
}

void websocket_handshake(char* websocket_key, int sockfd){
    //concatenating key with magic string
    char magic_string[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"; //magic string
    char* concatenated = concat(websocket_key, strlen(websocket_key), magic_string, strlen(magic_string));

    //calculating sha1 of concatenated string
    char sha1[SHA_DIGEST_LENGTH+1];
    SHA1(concatenated, strlen(concatenated),sha1);
    sha1[SHA_DIGEST_LENGTH]='\0';

    //base64 encoding the sha1 hash
    int output_length = 4 * ((strlen(sha1) + 2) / 3); //Base64 output size calculation
    unsigned char* base64 = malloc(output_length);
    EVP_EncodeBlock(base64, sha1, strlen(sha1));


    send_handshake_response(sockfd, base64, strlen(base64));
}

void handle_http_request(int pfds_index, struct http_header* header){
    char* websocket_key;
    int handshake=0;
    struct key_val* p;
    for(p=header->headers; p!=NULL; p=p->next_key_val){
        if(strcmp(p->key,"Sec-WebSocket-Key")==0){
            websocket_key=p->val;
            handshake=1;
            break;
        }
    }

    if(handshake){
        websocket_handshake(websocket_key, pfds[pfds_index].fd);
    }else{
        printf("[Sec-WebSocket-Key] not found.\n");
        printf("--------------------------------------------\n");
    }
}

//returns -1 on error, 0 otherwise
int handle_ws_dataframe(int pfds_index, char* dataframe, int dataframe_len){
    printf("Handle ws dataframe:\n");
    if(dataframe_len<2){
        printf("--------------------------------------------\n");
        return -1;
    }
    
    //parsing first byte
    int cur_byte = 0;
    unsigned char first_byte = dataframe[cur_byte];
    cur_byte++;
    int fin = first_byte&(0b10000000)?1:0;
    unsigned char reserved = first_byte&(0b01110000);
    unsigned char opcode = first_byte&(0b00001111);
    printf("Fin: %d\n", fin);
    printf("Reserved: 0x%x\n", reserved);
    printf("Opcode: %d\n", opcode);

    //parsing second byte
    unsigned char second_byte = dataframe[cur_byte];
    cur_byte++;
    int mask = second_byte&(0b10000000)?1:0;
    unsigned long long payload_length = second_byte&(0b01111111);
    if(payload_length==126){
        memcpy(&payload_length, &(dataframe[cur_byte]), 2);
        cur_byte+=2;
        payload_length = ntohs(payload_length);
    }else if(payload_length==127){
        unsigned int bigend, littleend;
        memcpy(&bigend, &(dataframe[cur_byte]), 2);
        cur_byte+=2;
        memcpy(&littleend, &(dataframe[cur_byte]), 2);
        cur_byte+=2;
        bigend = ntohl(bigend);
        littleend = ntohl(littleend);    
        payload_length = bigend*65536+littleend;
    }

    printf("Mask: %d\n", fin);
    printf("Payload length: %lld (dataframe_len=%d)\n", payload_length, dataframe_len);
    if(payload_length>dataframe_len){
        while(payload_length>dataframe_len){
            char dataframe_extension[1000];
            int num_bytes = recv(pfds[pfds_index].fd, dataframe_extension, sizeof(dataframe_extension), 0);
            dataframe = concat(dataframe, strlen(dataframe), dataframe_extension, num_bytes);
            dataframe_len = strlen(dataframe);
            printf("\tfetching(%lld/%d)...\n", payload_length, dataframe_len);
        }
    }
    if(mask!=1){
        printf("Messages from the client must be masked. Disconnecting socket %d\n", pfds[pfds_index].fd);
        printf("--------------------------------------------\n");
        close(pfds[pfds_index].fd);
        del_from_pdfs(pfds, pfds_index, &fd_count);
        return -1;
    }

    //masking-key
    unsigned char masking_key[4];
    memcpy(&masking_key, &(dataframe[cur_byte]), sizeof(masking_key));
    cur_byte+=sizeof(masking_key);
    printf("Masking-key: %02x %02x %02x %02x\n", masking_key[0], masking_key[1], masking_key[2], masking_key[3]);

    //encoded bytes
    unsigned char encoded[payload_length+1];
    memcpy(&encoded, &(dataframe[cur_byte]), sizeof(encoded));
    cur_byte+=sizeof(encoded);
    encoded[payload_length]='\0';

    //decoding message
    char decoded[payload_length+1];
    for(int i=0; i<payload_length; i++){
        decoded[i]=encoded[i]^masking_key[i%4];
    }
    decoded[payload_length] = '\0';

    printf("Decoded msg: %s\n", decoded);
    printf("--------------------------------------------\n");
}

//Determine how data is interpreted and handled
void handle_data(int pdfs_index, char* data, int datalen){
    print_data(pfds[pdfs_index].fd, data);
    struct http_header header;
    if(parse_http_headers(data, datalen, &header) == 0){
        handle_http_request(pdfs_index, &header);
    }else{
        handle_ws_dataframe(pdfs_index, data, datalen);
    }
}

int main(int argc, char* argv[]){
    pfds = malloc(sizeof(struct pollfd)*fd_size);
    int listenerfd, num_events, newfd, num_bytes;
    char data[1000];
    
    listenerfd = start_listener();
    if(listenerfd == -1){
        return 1;
    }
    printf("Listening on %s...\n", PORT);
    printf("--------------------------------------------\n");
    add_to_pfds(pfds, listenerfd, &fd_count, &fd_size);

    while(1){ //main loop
        num_events=poll(pfds, fd_count, -1);
        if(num_events == -1){
            perror("poll");
            exit(1);
        }
        for(int i=0; i<fd_count; i++){
            if (pfds[i].revents & POLLIN){ //fd is ready to read
                if(pfds[i].fd == listenerfd){ // if it's listener accept connection
                    if ((newfd = accept(listenerfd, NULL, NULL)) == -1){
                        perror("accept");
                        continue;
                    }
                    
                    add_to_pfds(pfds, newfd, &fd_count, &fd_size);
                    printf("New connection:%d\n", newfd);
                    printf("--------------------------------------------\n");
                }else{ //handle the message
                    num_bytes = recv(pfds[i].fd, data, sizeof(data), 0);
                    if(num_bytes == -1){ //error
                        perror("recv");
                        continue;
                    }

                    if(num_bytes == 0){ //Connection closed
                        printf("Connection closed: %d\n", pfds[i].fd);
                        printf("--------------------------------------------\n");
                        close(pfds[i].fd);
                        del_from_pdfs(pfds, i, &fd_count);
                        continue;
                    }

                    data[num_bytes]='\0'; //prevent funny bussines

                    handle_data(i, (char *)&data, num_bytes);
                }
            }
        }
    }
}