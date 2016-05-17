#ifndef _NETWORK
#define _NETWORK


#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 6666

struct sockaddr_in server_addr;
int g_sockfd;

int socket_init();

#endif
