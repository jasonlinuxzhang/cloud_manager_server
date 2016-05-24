#ifndef _VM_MONITOR
#define _VM_MONITOR

#include <libvirt/libvirt.h>
#include <time.h>
#include "../cJSON/cJSON.h"


typedef struct _VM_INFO
{
    const char *name;
    virDomainInfo start_info;
    struct  timespec start_time;
    virDomainInfo end_info;
    struct  timespec end_time;
}VM_INFO;

VM_INFO vm_info_array[128];

int vm_monitor_init();
cJSON* get_vm_info_impl();
#endif
