#include "utils.h"

void print_binary(unsigned char byte){
    for(int i=0;i<8;i++){
        unsigned char mask = 1<<(8-1-i);
        printf("%d", byte&mask?1:0);
    }
    printf("\n");
}

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