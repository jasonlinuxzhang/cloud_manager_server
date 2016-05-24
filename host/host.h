#ifndef _HOST
#define _HOST

typedef struct _HOST_INFO
{
    int cpu_rate;
    int memory_total;
    int memory_free;
    int disk_total;
    int disk_free;
}HOST_INFO;

HOST_INFO *get_host_info_impl();
int host_monitor_init();

#endif
