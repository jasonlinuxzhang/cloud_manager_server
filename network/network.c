#include <arpa/inet.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include "../log/log.h"
#include "../common.h"
#include "network.h"

int socket_init()
{
    char *ip = NULL;
    g_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == g_sockfd)
    {
        log_error_message("socket fail"); 
        return -1;     
    } 
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);
    server_addr.sin_port = htons(SERVER_PORT);   
    
    ip = inet_ntoa(server_addr.sin_addr); 
    if(NULL == ip)
    {
        log_error_message("inet_ntoa fail");
        return -1;
    }
    
    if(-1 == bind(g_sockfd, (struct sockaddr *)(&server_addr), sizeof(struct sockaddr)))
    {
        log_error_message("bind fail");
        return -1;
    }

    if(-1 == listen(g_sockfd, 3))
    {
        log_error_message("listen fail");
        return -1;
    }
    log_info_message("server ip:%s listen in:%d", ip, ntohs(server_addr.sin_port));
    return g_sockfd;
}
