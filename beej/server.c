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

#define SERVER_PORT "8080"

#define BACKLOG 10

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}


void* get_in_addr(struct  sockaddr* sa){
    if(sa->sa_family==AF_INET){
        return &(((struct sockaddr_in*) sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

int main(int argc, char* argv[]){
    struct addrinfo hints, *res, *p;
    struct sockaddr_storage peerAddr;
    socklen_t peerAddrLen = sizeof(struct sockaddr_storage);
    struct sigaction sa;
    int status, sockfd, newfd;
    int yes=1;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_PASSIVE;

    if ((status=getaddrinfo(NULL,SERVER_PORT, &hints, &res))!=0){
        fprintf(stderr, "getaddrinfo: %s", gai_strerror(status));
        return 1;
    }

    for(p=res; p!=NULL; p=p->ai_next){
        if ((sockfd=socket(p->ai_family, p->ai_socktype,p->ai_protocol))==-1){
            perror("server: socket");
            continue;
        }
        
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
            perror("setsockopt");
            exit(1);
        }
        
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            close(sockfd);
            fprintf(stderr, "bind: %s", gai_strerror(errno));
            continue;
        }

        break;
    }

    freeaddrinfo(res);

    if (p==NULL){
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG)==-1){
        fprintf(stderr, "listed: %d",errno);
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1){ //main accept() loop
        newfd=accept(sockfd,(struct sockaddr *) &peerAddr, &peerAddrLen);
        if (newfd==-1){
            fprintf(stderr,"accept: %d\n", errno);
            continue;
        }
        inet_ntop(peerAddr.ss_family, get_in_addr( (struct sockaddr *) &peerAddr), s, sizeof(s));
        printf("server: got connection from %s\n", s);

        if (!fork()) { // this is the child process
            close(sockfd); //child doesn't need this
            if (send(newfd, "Hello, world!", 13, 0) == -1)
                fprintf(stderr,"send: %d\n", errno);
            close(newfd);
            exit(0);
        }
        close(newfd); // parent doesn't need this
    }   

    return 0;
}