#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <stdlib.h>

void print_binary(unsigned char byte);
void print_message(int sockfd, char* msg);
void printHex(unsigned char* arr);
void copyCharArr(char* src, char* dst, int len);
char* concat(char* first, int firstlen, char* second, int secondlen);
void add_to_pfds(struct pollfd pfds[], int newfd, int* fd_count, int* fd_size);
void del_from_pdfs(struct pollfd pdfs[], int index, int* fd_count);
