#include <stdio.h>
#include <pthread.h>
#include <ctype.h>
#include <fcntl.h>
#include "../common.h"
#include "host.h"


#define CPU_MAX 128

pthread_t cpu_tid;
pthread_t mem_tid;

static int cpu_rate = 0;
static int mem_total = 0, mem_free = 0;


HOST_INFO *get_host_info()
{
    HOST_INFO *host_info = NULL;
    host_info = (HOST_INFO *)malloc(sizeof(HOST_INFO));
    if(NULL == host_info)
    {
        log_error_message("alloc memory fail");
        return NULL;
    }
    
    host_info->cpu_rate = cpu_rate;
    host_info->memory_total = mem_total;
    host_info->memory_free = mem_free;
}

int host_monitor_init()
{
    if(-1 == pthread_create(&cpu_tid, NULL, cpu_monitor, NULL))
    {
        log_error_message("create thread host monitor fail");
        return -1;
    }
    if(-1 == pthread_create(&mem_tid, NULL, memory_monitor, NULL))
    {
        log_error_message("create thread memory monitor fail");
        return -1;
    }
}

static char *skip_token(const char *p)
{
    while (isspace(*p)) p++;
    while (*p && !isspace(*p)) p++;
    return (char *)p;
}

void *cpu_monitor(void *argv)
{
    int read_count = 0, i;
    char buf[READ_FILE_MAX];
    long long cpu_old[10], cpu_new[10];
    long long old_cpu_time, new_cpu_time, idle_cpu_diff, total_cpu_diff;
    float cpu_usage = 0.0;
    char *p = NULL;

    int fd = open("/proc/stat", O_RDONLY);
    if(-1 == fd)
    {
        log_error_message("open /proc/stat fail");
        return NULL;
    }
    read_count = read(fd, buf, READ_FILE_MAX - 1); 
    if(read_count < 0)
    {
        log_error_message("read fail");
    }
    buf[read_count] = '\0'; 
    p = skip_token(buf);
    for(i = 0; i < 10; i++)
    {
        cpu_old[i] = strtoul(p, &p, 0);
    }
    close(fd);
    
    while(!g_exit)
    {
        sleep(1)
        fd = open("/proc/stat", O_RDONLY);
        if(-1 == fd)
        {
            log_error_message("open /proc/stat fail");
            return NULL;
        }
        read_count = read(fd, buf, READ_FILE_MAX - 1); 
        if(read_count < 0)
        {
            log_error_message("read fail");
        }
    
        buf[read_count] = '\0'; 
        for(i = 0; i < 10; i++)
        {
            cpu_new[i] = strtoul(p, &p, 0);
        }
        close(fd);
        
        old_cpu_time = 0;
        new_cpu_time = 0;

        for(i = 0; i < 10; i++)
        {
            old_cpu_time += cpu_old[i];
            new_cpu_time += cpu_new[i];
        }

        idle_cpu_diff = cpu_new[3] - cpu_old[3];
        total_cpu_diff = new_cpu_time - old_cpu_time;
        if(0 != total_cpu_diff)
        {
            cpu_usage = 100 - ((float)100 * idle_cpu_diff)/total_cpu_diff;    
        }
        else
        {
            cpu_usage = 0.0;
        }
        
        cpu_rate = cpu_usage; 
        for (i = 0; i < NCPUSTATES; i++)
            cpu_old[i] = cpu_new[i];
    }
}


void *memory_monitor(void *argv)
{
    int fd = -1;
    int read_count = 0; 
    char *buf = NULL, *p = NULL;
    
    if((buf = malloc(READ_FILE_MAX)) == NULL)
    {
        log_error_message("alloc memory fail");
        goto clean;
    }

    while(!g_exit)
    {
        fd = open("/proc/meminfo", O_RDONLY);
        if(fd < 0)
        {
            log_error_message("open /proc/meminfo fail");
            goto clean;
        }
    
        memset(buf, 0, READ_FILE_MAX);
        read_count = read(fd, buf, READ_FILE_MAX);
        if(read_count <= 0)
        {
            log_error_message("read /proc/meminfo fail or empty");
            goto clean;
        }
        
        buf[read_count] = '\0';
        p = buff;
        p = skip_token(p);
        mem_total = strtoul(p, &p, 10);
        
        p = strchr(p, '\n');
        p = skip_token(p);
        mem_free = strtoul(p, &p, 10);
        
        close(fd);
    }

clean:
    if(NULL != buf)
    {
        free(buf);
    }
    if(fd > 0)
    {
        close(fd);
    }
    return NULL;

}
