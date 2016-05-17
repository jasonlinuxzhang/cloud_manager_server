#include "log.h"
#include <time.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

char *log_level_string(LOG_LEVEL log_level)
{
    switch (log_level)
    {
        case INFO: return "INFO"; break;
        case WARNING: return "WARNING"; break;
        case ERROR: return "ERROR"; break;
        default: return "NOLEVEL";
    }
}

void print_log_to_syslog(const char *buf)
{
    openlog("yun_manager", LOG_PID, LOG_USER);
    syslog(LOG_ERR, "%s", buf);
    closelog();
}

void log_message(LOG_LEVEL log_evel, int line, char *funcname, const char *fmt, va_list vargs)
{
    struct timespec now;
    struct tm *p;
    char timep[128] = {0};
    char errorinfo[128] = {0};
    char str[1024] = {0};
    char msg[1024] = {0};
    

    snprintf(str, sizeof(str), fmt, vargs);
    /*get time string*/
    clock_gettime(CLOCK_REALTIME, &now);
    p = localtime(&now.tv_sec);
    snprintf(timep, sizeof(timep), "%04d-%02d-%02d %02d:%02d:%02d:%03d", (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec, (int)(now.tv_nsec/1000000));

    snprintf(msg, sizeof(msg), "%s: %u: %s : %s:%d : %s\n", timep, (unsigned int)gettid(), log_level_string(log_evel), funcname, line, str);
    if (NULL == msg)
        goto cleanup;

    log_file_open();

    if (gLogFd > 0)
    {
        if (safe_write(gLogFd, msg, strlen(msg)) < 0)
        {
            print_log_to_syslog(msg);  
        }
        close(gLogFd);
    }
    else
    {
       print_log_to_syslog(msg); 
    } 
cleanup:
    if(NULL != msg)
    {
        free(msg);
    }
    if(NULL != str)
    {
        free(str);
    }
    
}

void log_file_open()
{
    if(NULL == LOG_FILE_PATH)    
    {
        return;
    }
    gLogFd = open(LOG_FILE_PATH, O_WRONLY | O_APPEND | O_CREAT);
    if(-1 == gLogFd)
    {
        print_log_to_syslog("open log file fail");
    }
}


