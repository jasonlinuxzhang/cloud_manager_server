#include "common.h"

int hostagent_safewrite(int fd, const void *buf, int count)
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
        buf = (const char *)buf + r;
        count -= r;
        nwritten += r;
    }
    return nwritten;
}

