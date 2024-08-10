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

#define LISTEN_PORT "3490"

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
    printf("Listener started\n");
    struct addrinfo hints, *res, *p;
    int status, sockfd, recvbytes;
    struct sockaddr from;
    int fromlen = sizeof(struct sockaddr);
    char buf[100];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, LISTEN_PORT, &hints, &res)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    for (p=res; p!=NULL; p=p->ai_next){
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("socket");
            continue;
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) != 0){
            close(sockfd);
            perror("bind");
            continue;
        }
        break;
    }

    if (p==NULL){
        fprintf(stderr, "Can't create socket\n");
        exit(1);
    }

    print_addrinfo(p);
    freeaddrinfo(res);

    printf("listener: waiting for recvfrom...\n");

    if ((recvbytes = recvfrom(sockfd, buf, sizeof(buf), 0, NULL, NULL)) == -1){
        perror("recvfrom");
        exit(1);
    }

    buf[recvbytes] = '\0';

    printf("Received: %s\n", buf);

    close(sockfd);
    return 0;
}