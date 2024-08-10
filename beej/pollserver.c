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

#define PORT "9034"   // Port we're listening on
#define BACKLOG 10

void * get_in_addr(struct sockaddr_storage* sa){
    if (sa->ss_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr);
    } 
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void add_to_pfds(struct pollfd pfds[], int newfd, int* fd_count, int* fd_size){
    
    // if we don't have room, increase size of pfds
    if ((*fd_count) == (*fd_size)) { 
        (*fd_size) *= 2;
        pfds = realloc(pfds, (sizeof(struct pollfd)) * (*fd_count) );
    }

    pfds[(*fd_count)].fd = newfd;
    pfds[(*fd_count)].events =  POLLIN;

    (*fd_count) ++;
}

//remove index from set
void del_from_pdfs(struct pollfd pdfs[], int index, int* fd_count){
    pdfs[index]=pdfs[(*fd_count)-1];//copy the last one to the index-th one
    (*fd_count)--; // decrease counter
}

int listener_setup(){
    struct addrinfo hints, *res, *p;
    int status;
    int listenerfd;
    int yes=1;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET; //IPv4
    hints.ai_socktype = SOCK_STREAM; //tcp

    if ((status = getaddrinfo(NULL, PORT, &hints, &res)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(1);
    }

    for(p=res; p!=NULL; p=p->ai_next){

        //make a socket
        if ((listenerfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("socket");
            continue;
        }
        
        //Lose the "address already in use" error msg.
        setsockopt(listenerfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        //bind the socket to the port
        if (bind(listenerfd, p->ai_addr, p->ai_addrlen) != 0){
            perror("bind");
            continue;
        }

        //start listening
        if (listen(listenerfd, BACKLOG) != 0){
            perror("listen");
            continue;
        }

        break;
    }

    if(p==NULL){
        fprintf(stderr, "Can't make listener\n");
        exit(1);
    }

    freeaddrinfo(res);

    //
    fcntl(listenerfd, F_SETFL, O_NONBLOCK); // ?!?!?!?!
    //

    return listenerfd;
}

void broadcast(struct pollfd pfds[], int senderfd, int listenerfd, char* msg_buf, int fd_count){
    int numbytes;
    for(int i=0; i<fd_count; i++){
        if(pfds[i].fd == senderfd || pfds[i].fd == listenerfd){
            continue;
        }
        if ((numbytes=send(pfds[i].fd, msg_buf, sizeof(msg_buf), 0)) == -1){ //???
            perror("send");
            continue;
        }
    }
}

int main(){
    struct pollfd* pfds;
    struct sockaddr_storage peeraddr;
    int fd_size=5, fd_count=0;
    int listenerfd;
    int num_events;
    int peerlen;
    char remoteIP[INET6_ADDRSTRLEN];
    char msg_buf[256];
    int numbytes;
    pfds = malloc(sizeof(struct pollfd)*fd_size);

    listenerfd = listener_setup();
    add_to_pfds(pfds, listenerfd, &fd_count, &fd_size);

    printf("Listener started on port:%s\n", PORT);

    while(1){ //main loop
        num_events = poll(pfds, fd_count, -1);

        if(num_events == -1){ // poll() error
            perror("poll");
            exit(1);
        }

        //run trough all fd-s, check whose events is triggered and react appropriately
        for(int i=0; i<fd_count; i++){
            if(pfds[i].revents & POLLIN){ //event is triggered
                //listener is triggered
                if(pfds[i].fd == listenerfd) {
                    int newfd = accept(listenerfd, (struct sockaddr*) &peeraddr, &peerlen);
                    if (newfd==-1){
                        perror("accept");
                        continue;
                    }
                    add_to_pfds(pfds, newfd, &fd_count, &fd_size);
                    
                    inet_ntop(peeraddr.ss_family, get_in_addr(&peeraddr), remoteIP, sizeof(remoteIP));
                    
                    printf("Accepted connection from %s socket:%d.\n Number of connected peers: %d\n", remoteIP, newfd, fd_count);
                    continue;
                }else{ // peer is triggered
                    if ((numbytes = recv(pfds[i].fd, msg_buf, sizeof(msg_buf), 0)) == -1){
                        perror("recv");
                        close(pfds[i].fd);
                        del_from_pdfs(pfds, i, &fd_count);
                        continue;
                    }else if (numbytes == 0){ //connection closed
                        close(pfds[i].fd);
                        del_from_pdfs(pfds, i, &fd_count);
                        printf("Connection on socket %d closed. Connected peers: %d\n", pfds[i].fd, fd_count);
                        continue;
                    }else{ // boradcast the message to all except sender and listener
                        msg_buf[numbytes]='\0';
                        broadcast(pfds, pfds[i].fd, listenerfd, msg_buf, fd_count);
                    }
                }
            }
        }
    }

    
}