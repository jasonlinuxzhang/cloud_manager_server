#include <time.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <syslog.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "../common.h"
#include "log.h"

pthread_t log_thread_t;

int get_log_level_from_config();
char *log_level_string(LOG_LEVEL log_level);

int log_init()
{
    log_filter = get_log_level_from_config();   
    log_info_message("log filter is %s", log_level_string(log_filter));
    pthread_mutex_init(&log_thread_mutex, NULL);
    if(0 != pthread_create(&log_thread_t, NULL, log_monitor, NULL))
    {
        log_error_message("log monitor thread fail");
        pthread_mutex_destroy(&log_thread_mutex);
        return -1;
    }
    return 0;
}

int get_log_level_from_config()
{
    FILE *fp = NULL;
    char level[8] = {0};
    int nread = 0, log_level = -1;
    if((fp = fopen(LOG_CONFIG_FILE, "r")) == NULL)
    {
        log_error_message("fopen %s fail", LOG_CONFIG_FILE);
        return -1;
    }
    if((nread = fread(level, sizeof(char), sizeof(level), fp)) <= 0)
    {
        log_error_message("read date from %s fail", LOG_CONFIG_FILE);
        fclose(fp);
        return -1;
    }
    
    level[nread] = 0;
    sscanf(level, "%d", &log_level);
    return log_level;
    
}


void *log_monitor(void *argv)
{
    time_t now;
    struct tm *timenow = NULL;
    char cmd[128] = {0};
    snprintf(cmd, 128, "rm /var/log/yun_manager.log -f 2>>/dev/null");
    while(!g_exit)
    {
        sleep(1);
        pthread_mutex_lock(&log_thread_mutex);
        time(&now);
        timenow = localtime(&now);
        if(NULL == timenow)
        {
            printf("error");
            pthread_mutex_unlock(&log_thread_mutex);
            continue;
        }
        if(1 == timenow->tm_hour && 0 == timenow->tm_min && timenow->tm_sec < 2)
        {
            system(cmd);
        }
        pthread_mutex_unlock(&log_thread_mutex);
    } 
}

char *log_level_string(LOG_LEVEL log_level)
{
    switch (log_level)
    {
        case DEBUG: return "DEBUG"; break;
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

void log_message(LOG_LEVEL log_evel, int line, const char *funcname, const char *fmt, ...)
{
    struct timespec now;
    struct tm *p;
    char timep[128] = {0};
    char errorinfo[128] = {0};
    char str[1024] = {0};
    char msg[1024] = {0};
    va_list vargs;


    if(log_evel < log_filter)
    {
        return ;
    }
    
    pthread_mutex_lock(&log_thread_mutex);

    va_start(vargs, fmt); 

    vsnprintf(str, sizeof(str), fmt, vargs);
    /*get time string*/
    clock_gettime(CLOCK_REALTIME, &now);
    p = localtime(&now.tv_sec);
    snprintf(timep, sizeof(timep), "%04d-%02d-%02d %02d:%02d:%02d:%03d", (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec, (int)(now.tv_nsec/1000000));

    snprintf(msg, sizeof(msg), "%s: %s : %s:%d : %s\n", timep,  log_level_string(log_evel), funcname, line, str);
    if (NULL == msg)
        goto cleanup;

    log_file_open();

    if (gLogFd > 0)
    {
        if (safewrite(gLogFd, msg, strlen(msg)) < 0)
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
    va_end(vargs);
    pthread_mutex_unlock(&log_thread_mutex);
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


