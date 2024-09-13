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

int fd_size=1, fd_count=0;//fd_size - size of unerlying array, fd_count - num of element currently in the array
struct pollfd* pfds;