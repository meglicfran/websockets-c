#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT_NUMBER "8080"

int main(int argc, char* argv[]){
    struct addrinfo hints, *res, *p;
    int status, sockfd, numbytes;
    char buf[100];

    memset(&hints, 0, sizeof hints);
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;

    if ((status=getaddrinfo(NULL, PORT_NUMBER, &hints, &res)) !=0 ){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    for (p=res; p!=NULL; p=p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))== -1){
            perror("socket");
            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            close(sockfd);
            perror("connect");
            continue;
        }
        break;
    }
    
    freeaddrinfo(res);

    if(p==NULL){
        perror("client: failed to connect");
        exit(1);
    }
    
    if ((numbytes = recv(sockfd, buf, sizeof(buf), 0)) == -1 ){
        perror("recv");
        exit(1);
    }
    buf[numbytes]='\0';
    
    printf("recv: %s\n", buf);
    
    
    close(sockfd);

    return 0;
}