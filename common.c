#include "common.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "log/log.h"

int try_connect()
{
    if(NULL == g_conn)
    {
        g_conn = virConnectOpen("qemu:///system");
        if(NULL == g_conn)
        {
            return -1;
        }
    }
    return 0;
}


int safewrite(int fd, void *buf, int count)
{
    int nwritten = 0;
    while (count > 0) {
        ssize_t r = write(fd, buf, count);

        if (r < 0 && errno == EINTR)
            continue;
        if (r < 0)
        {
            return r;
        }
        if (r == 0)
        {
            return nwritten;
        }
        buf = buf + r;
        count -= r;
        nwritten += r;
    }
    return nwritten;
}

int saferead(int sockfd, char *buf, int count)
{
    int read_count = 0;
    int real_count = 0;
    int count_left = count;
    while((read_count = read(sockfd, buf, count_left)))
    {
        log_info_message("read %d", read_count);
        if(read_count < 0)
        {
            if(EINTR == errno)
            {
                continue;
            }
            break;
        }
        real_count += read_count;
        if(count == real_count)
        {
            break;    
        }
    }
    return real_count;
}
