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

#define SEND_PORT "3490"

void print_addrinfo(struct addrinfo* p){
    char buf[INET6_ADDRSTRLEN];
    if (inet_ntop(p->ai_family, &((struct sockaddr_in *)(p->ai_addr))->sin_addr, buf, sizeof(buf)) == NULL){
        perror("inet_ntop");
        return;
    }else{
        printf("%s\n", buf);
    }
}

int main(int argc, char* argv[]){
    struct addrinfo hints, *res, *p;
    struct sockaddr_in to;
    int status, sockfd, numbytes;
    char buf[100];

    if (argc != 2){
        printf("usage: sender msg\n");
        return 1;
    }
    if (sizeof(argv[1])>100){
        printf("message too long!\n");
        return 1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((status = getaddrinfo(NULL, SEND_PORT, &hints, &res))==-1){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    for (p=res; p!=NULL; p=p->ai_next){
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("socket");
            continue;;
        }
        break;
    }
    
    print_addrinfo(p);

    if (p==NULL){
        fprintf(stderr, "Can't create socket\n");
        exit(1);
    }


    if ((numbytes = sendto(sockfd, argv[1], strlen(argv[1]), 0, p->ai_addr, p->ai_addrlen)) == -1 ){
        perror("sendto");
        exit(1);
    }

    freeaddrinfo(res);

    printf("talker: sent %d bytes\n", numbytes);
    close(sockfd);

    return 0;
}