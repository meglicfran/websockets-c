#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <fcntl.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

#define PORT "6969"   // Port we're listening on
#define BACKLOG 10

struct http_header{
    char* method;
    char* url;
    char* protocol;
    int methodlen, protocollen, urllen;
    struct key_val* headers;
};

struct key_val{
    char* key;
    char *val;
    int keylen, vallen;
    struct key_val* next_key_val;
};

void print_message(int sockfd, char* msg){
    printf("Message received from socket %d:\n", sockfd);
    printf("--------------------------------------------\n");
    printf("%s\n", msg);
    printf("--------------------------------------------\n");
}

void printHex(unsigned char* arr){
    for(int i=0;i<strlen(arr);i++){
        printf("%02x ", arr[i]);
    }
    printf("\n");
}

void copyCharArr(char* src, char* dst, int len){
    for (int i=0; i<len; i++){
        dst[i]=src[i];
    }
}

//firstlen and secondlen should be strlen(first) and strlen(second)
char* concat(char* first, int firstlen, char* second, int secondlen){
    char* result = malloc(sizeof(char)*(firstlen+secondlen+1));
    copyCharArr(first, (char*)(&result[0]), firstlen);
    copyCharArr(second, (char*)(&result[firstlen]), secondlen);
    result[firstlen+secondlen]='\0';
    return result;
}

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

//add newfd to pfds[]  
void add_to_pfds(struct pollfd pfds[], int newfd, int* fd_count, int* fd_size){
    // if we don't have room, increase size of pfds
    if ((*fd_count) == (*fd_size)) { 
        (*fd_size) = (2>(*fd_size)*2) ? 2 : (*fd_size)*2; //if fd_size is 0 the set it to 2
        pfds = realloc(pfds, (sizeof(struct pollfd)) * (*fd_count) );
    }

    pfds[(*fd_count)].fd = newfd;
    pfds[(*fd_count)].events =  POLLIN;

    (*fd_count) ++;
}

//remove fd on index from pdfs[]
void del_from_pdfs(struct pollfd pdfs[], int index, int* fd_count){
    pdfs[index]=pdfs[(*fd_count)-1];//copy the last one to the index-th one
    (*fd_count)--; // decrease counter
}

void parse_headers(char* msg, int msglen, struct http_header* header){
    char buf[msglen];
    int lencounter=0;

    int i=0;

    //Parsing the method
    for(;i<msglen; i++){
        if(msg[i]!=' '){
            buf[lencounter]=msg[i];
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
    lencounter=0;

    //Parsing url
    for(; i<msglen; i++){
        if(msg[i]!=' '){
            buf[lencounter]=msg[i];
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
    lencounter=0;
    
    //Parsing protocol
    for(; i<msglen; i++){
        if(msg[i]!='\r'){
            buf[lencounter]=msg[i];
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
        for(; i<msglen; i++){
            if(msg[i]!=':'){
                buf[lencounter]=msg[i];
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
        for(; i<msglen; i++){
            if(msg[i]!='\r'){
                buf[lencounter]=msg[i];
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

        //end of msg
        if(i>=msglen || msg[i]=='\r'){
            break;
        }else{ // 
            struct key_val* new = malloc(sizeof(struct key_val));
            memset(new, 0, sizeof(struct key_val));
            new -> next_key_val=NULL;
            cur -> next_key_val = new;
            cur = new;
        }

    }
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
    printf("%s", header);
    printf("--------------------------------------------\n");

    if(send(sockfd, header, strlen(header),0) == -1){
        perror("send_handshake_response");
    }
}

void websocket_handshake(char* websocket_key, int sockfd){
    printf("Websocket key: %s \n", websocket_key);
    printf("--------------------------------------------\n");

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
    printf("Concatenated SHA1 (Base64): %s\n", base64);
    printf("--------------------------------------------\n");

    send_handshake_response(sockfd, base64, strlen(base64));
}

void handle_http_request(int sockfd, char* msg, int msglen, struct http_header* header){
    
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
        websocket_handshake(websocket_key, sockfd);
    }
}

void handle_message(int sockfd, char* msg, int msglen){
    
    struct http_header header;
    parse_headers(msg, msglen, &header);

    printf("method:%s\nurl:%s\nprotocol:%s\n", header.method, header.url, header.protocol);

    if(strcmp(header.method,"GET")==0 && strcmp(header.url, "/")==0 && strcmp(header.protocol, "HTTP/1.1")==0){
        printf("x\n");
        handle_http_request(sockfd, msg, msglen, &header);
    }
}


int main(int argc, char* argv[]){
    int fd_size=1, fd_count=0; //fd_size - size of unerlying array, fd_count - num of element currently in the array
    struct pollfd* pfds = malloc(sizeof(struct pollfd)*fd_size);
    struct sockaddr peeraddr;
    int listenerfd, num_events, peerlen, newfd, num_bytes;
    char msg[1000];
    
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
                    peerlen = sizeof(struct sockaddr);
                    if ((newfd = accept(listenerfd, &peeraddr, &peerlen)) == -1){
                        perror("accept");
                        continue;
                    }
                    
                    add_to_pfds(pfds, newfd, &fd_count, &fd_size);
                    printf("New connection:%d\n", newfd);
                    printf("--------------------------------------------\n");
                }else{ //handle the message
                    num_bytes = recv(pfds[i].fd, msg, sizeof(msg), 0);
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

                    msg[num_bytes]='\0'; //prevent funny bussines

                    print_message(pfds[i].fd, msg);
                    
                    handle_message(pfds[i].fd, (char *)&msg, num_bytes);
                }
            }
        }
    }
}