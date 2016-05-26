#ifndef _VM_MONITOR
#define _VM_MONITOR

#include <libvirt/libvirt.h>
#include <time.h>
#include "../cJSON/cJSON.h"


typedef struct _CPU_RATE
{
    char *name;
    int rate;
    struct timespec last_time;
    struct _CPU_RATE *next;
}CPU_RATE;

typedef struct _VM_MONITOR_STRUCT
{
    char *name;
    pthread_t vm_tid;
    int check_flag;
    struct _VM_MONITOR_STRUCT *next;
}VM_MONITOR_STRUCT;


int vm_monitor_init();
cJSON* get_vm_info_impl();
#endif
