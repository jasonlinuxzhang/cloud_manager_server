#include <network.h>
#include "common.h"

int socket_init()
{
    g_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == g_sockfd)
    {
        log_error_message("socket fail"); 
        return -1;     
    } 
    server_addr.sin_family = AF_INET;
    server_addr.s_addr = inet_addr(SERVER_ADDR);
    server_addr.sin_port = htons(SERVER_PORT);   
    
    log_info_message("");
    
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
    return g_sockfd;
}
