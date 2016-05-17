#include <stdio.h> 
#include "network/network.h"
#include "log/log.h"
#include "common.h"
#include <errno.h>

int manager_init()
{
    g_exit = 0;
    if(-1 == socket_init())
    {
        return -1;
    }
}


int main()
{
    int res = 0, len = 0;
    struct sockaddr_in client_addr;
    res = manager_init();
    if(-1 == res)
    {
        return -1;
    }
    while(!g_exit)
    {
        memset(&client_addr, 0, sizeof(client_addr));
        res = accept(g_sockfd, (struct sockaddr *)(&client_addr), &len); 
        if(-1 == res)
        {
            if(EINTR == errno)
            {
                continue;
            }
            log_error_message("accept fail");
            break;
        }
        else if(0 == res)
        {
            printf("new conme");
        }
        
    }
    
    return 0;
}
