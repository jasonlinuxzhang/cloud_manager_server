#ifndef _NETWORK
#define _NETWORK

#include <stdio.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 6666

struct sockaddr_in server_addr;
int g_sockfd = 0;

int socket_init();

#endif
