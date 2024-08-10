/*
** select.c -- a select() demo
*/

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define STDIN 0  // file descriptor for standard input

int main(){
    struct timeval tv;
    fd_set readfds;
    int num;

    tv.tv_sec = 2;
    tv.tv_usec = 500000;

    FD_ZERO(&readfds);
    FD_SET(STDIN, &readfds);

    if((num = select(STDIN+1, &readfds, NULL, NULL, &tv))==-1){
        perror("select");
        return 1;
    }else if(num==0){
        printf("2.5s passed and nothing happened\n");
        return 2;
    }else {
        if(FD_ISSET(STDIN, &readfds)){
            printf("Stdin ready for read\n");
        }
    }

    return 0;
}