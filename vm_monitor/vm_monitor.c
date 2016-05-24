#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include "../log/log.h"
#include "../common.h"
#include "vm_monitor.h"

pthread_t vm_tid;

static int activeCount = 0;
pthread_mutex_t vm_monitor_mutex;

void *vm_monitor(void *argv);

int vm_monitor_init()
{
    int ret = 0;
    pthread_mutex_init(&vm_monitor_mutex, NULL);
    ret = pthread_create(&vm_tid, NULL, vm_monitor, NULL);
    if(ret < 0)
    {
        log_error_message("create thread vm monitor faill");
        pthread_mutex_destroy(&vm_monitor_mutex);
        return -1;
    }
}

cJSON* get_vm_info_impl()
{
    int i = 0;
    cJSON *return_array = NULL;
    cJSON *one_info = NULL;
    if(activeCount <= 0)
    {
        return NULL;
    }    

    return_array = cJSON_CreateArray();
    if(NULL == return_array)
    {
        log_error_message("create array object fail");
        return NULL;
    }

    pthread_mutex_lock(&vm_monitor_mutex);
  
    for(i = 0; i < activeCount; i++)
    {
        one_info = cJSON_CreateObject();
        if(NULL == one_info)
        {
            continue;
        }
        cJSON_AddStringToObject(one_info, "Name", vm_info_array[i].name);
        cJSON_AddNumberToObject(one_info, "MaxMemory", vm_info_array[i].start_info.maxMem); 
        cJSON_AddNumberToObject(one_info, "UsedMemory", vm_info_array[i].start_info.memory);  
        cJSON_AddItemToArray(return_array, one_info);
    }
    pthread_mutex_unlock(&vm_monitor_mutex);
    
    return return_array;
}

void *vm_monitor(void *argv)
{
    virDomainPtr *domains;
    int active_count = 0, i = 0;
    int ret = 0;
    while(!g_exit)
    {
        sleep(1);
        try_connect();
        active_count = virConnectListAllDomains(g_conn, &domains, VIR_CONNECT_LIST_DOMAINS_ACTIVE);
        if(active_count <= 0)
        {
            log_error_message("list active domains fail or no active domains");
            continue;
        }
        pthread_mutex_lock(&vm_monitor_mutex);
        for(i = 0; i < active_count; i++)
        {   
            ret = virDomainGetInfo(domains[i], &vm_info_array[i].start_info); 
            clock_gettime(CLOCK_REALTIME, &vm_info_array[i].start_time);
            vm_info_array[i].name = virDomainGetName(domains[i]); 
        }
        log_info_message("get %d vms info", active_count);
        pthread_mutex_unlock(&vm_monitor_mutex);
        activeCount = active_count;
    }
    return NULL;
}



