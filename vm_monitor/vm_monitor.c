#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "../log/log.h"
#include "../common.h"
#include "vm_monitor.h"

pthread_t vm_tid;

static VM_MONITOR_STRUCT *vm_monitor_tid_head = NULL;       /*monitor thread */
static CPU_RATE *vm_cpu_rate_head = NULL;                   /*cpu rate*/

pthread_mutex_t cpu_rate_mutex;


void *vm_monitor(void *argv);
void *one_vm_monitor(void *argv);

int vm_monitor_init()
{
    int ret = 0;
    pthread_mutex_init(&cpu_rate_mutex, NULL);
    ret = pthread_create(&vm_tid, NULL, vm_monitor, NULL);
    if(ret < 0)
    {
        log_error_message("create thread vm monitor faill");
        pthread_mutex_destroy(&cpu_rate_mutex);
        return -1;
    }
}


void detach_and_free_cpu_node(CPU_RATE *temp)
{
    CPU_RATE *head = vm_cpu_rate_head, *pre = NULL;
    if(NULL == temp)
    {
        return ;
    }

    log_info_message("enter delete %s cpu node", temp->name);

    if(vm_cpu_rate_head == temp)
    {
        vm_cpu_rate_head = vm_cpu_rate_head->next;
    }
    else
    {
        while(NULL != head)
        {
            head = head->next;
            if(head == temp)
            {
                pre->next = head->next;
                break;
            }
            head = head->next;
        }
    }
    
    free(temp->name);
    free(temp);
    log_info_message("exit delete cpu node");
}


cJSON* get_vm_info_impl()
{
    int i = 0;
    cJSON *return_array = NULL;
    cJSON *one_info = NULL;
    CPU_RATE *temp = NULL, *temp_2 = NULL;
    struct timespec now_time;
    return_array = cJSON_CreateArray();
    if(NULL == return_array)
    {
        log_error_message("create array object fail");
        return NULL;
    }
    pthread_mutex_lock(&cpu_rate_mutex);
    
    clock_gettime(CLOCK_REALTIME, &now_time);
    temp = vm_cpu_rate_head;
    while(NULL != temp)
    {
        if((now_time.tv_sec - temp->last_time.tv_sec)*1000 + (now_time.tv_nsec - temp->last_time.tv_nsec)/1000000 > 1500) 
        {
            temp_2 = temp;
            temp =temp->next;
            detach_and_free_cpu_node(temp_2);
            continue;        
        }
        one_info = cJSON_CreateObject();
        if(NULL == one_info) 
        {
            break;
        }
        cJSON_AddNumberToObject(one_info, temp->name, temp->rate);
        cJSON_AddItemToArray(return_array, one_info);
        temp = temp->next;
    }
    pthread_mutex_unlock(&cpu_rate_mutex);
    
    return return_array;
}

void insert_to_list(VM_MONITOR_STRUCT **head, VM_MONITOR_STRUCT *onenode)
{
    VM_MONITOR_STRUCT *local_head = *head, *pre = NULL;
    
    onenode->next = NULL;
    if(NULL == *head)
    {
        *head = onenode; 
        return ;
    }
    
    while(NULL != local_head->next)
    {
        local_head = local_head->next;
    }

    local_head->next = onenode;
}


void detach_and_free_node(VM_MONITOR_STRUCT *temp)
{
    VM_MONITOR_STRUCT *head = vm_monitor_tid_head, *pre = NULL;
    
    log_info_message("detach %s node\n", temp->name);
    
    if(head == temp)
    {
        vm_monitor_tid_head =head->next;
        goto clean;
    }
    
    while(head != NULL)
    {
        if(head == temp)
        {
            pre->next = head->next;
            break;
        }
        pre = head;
        head = head->next;
    }

clean: 
    free(temp->name);
    free(temp);
}

void *vm_monitor(void *argv)
{
    virDomainPtr *domains;
    int active_count = 0, i = 0, j = 0, index = 0;
    int ret = 0;
    const char *vm_name = NULL;
    VM_MONITOR_STRUCT *temp = NULL, *head = NULL;

    while(!g_exit)
    {
        sleep(1);
        try_connect();
        active_count = virConnectListAllDomains(g_conn, &domains, VIR_CONNECT_LIST_DOMAINS_ACTIVE);
        if(active_count < 0)
        {
            log_error_message("list active domains fail or no active domains");
            continue;
        }

        
        head = vm_monitor_tid_head;
        while(NULL != head)
        {
            head->check_flag = 0;
            head = head->next;
        }
        for(i = 0; i < active_count; i++)
        {   
            vm_name = virDomainGetName(domains[i]);
            if(NULL == vm_name)
            {
                log_error_message("can't get name");
                continue;
            }
            for(head = vm_monitor_tid_head; NULL != head; head =head->next) 
            {
                if(strcmp(head->name, vm_name) == 0)
                {
                    head->check_flag = 1;
                    break;
                }
            }
            if(NULL == head)
            {
                temp = (VM_MONITOR_STRUCT *)malloc(sizeof(VM_MONITOR_STRUCT));
                if(NULL == temp)
                {
                    log_error_message("alloc memory fail");
                    continue;
                }
                temp->name = malloc(strlen(vm_name) + 1);
                memset(temp->name, 0, strlen(vm_name) + 1);
                strcpy(temp->name, vm_name);
                temp->check_flag = 1;
                
                if(0 != pthread_create(&temp->vm_tid, NULL, one_vm_monitor, temp->name))
                {
                    log_error_message("thread create fail");
                    free(temp->name);
                    free(temp);
                }
                else
                {
                    log_info_message("new running vm:%s start an thread success", temp->name);
                    insert_to_list(&vm_monitor_tid_head, temp);
                }
                temp = NULL;
            }
        }

        head = vm_monitor_tid_head;
        while(NULL != head)
        {
            if(0 == head->check_flag)
            {
                temp = head;
                head = head->next;
                detach_and_free_node(temp);
                continue;
            }
            head = head->next;
        }
        
    }
    return NULL;
}

void *one_vm_monitor(void *argv)
{
    char vm_name[16] = {0};
    virDomainInfo domain_info;
    virDomainPtr domain;
    long long cpu_start, cpu_end;
    int interval = 1;
    struct timespec starttime, endtime;
    double cputime = 0, realtime = 0;
    int cpu_usage = 0;
    CPU_RATE *temp, *pre;    
    int state = 0;

    pthread_detach(pthread_self());
    strncpy(vm_name, (char *)argv, 16);

    domain = virDomainLookupByName(g_conn, vm_name);   
    if(NULL == domain)
    {
        //get_libvirt_error();
        return NULL;
    } 

    while(!g_exit)
    {
        if(0 != virDomainGetState(domain, &state, NULL, 0))
        {
            log_error_message("get vm state fail");
            break;
        }
        if(state == VIR_DOMAIN_SHUTOFF)
        {
            log_error_message("%s is not running, now thread exit", vm_name);
            break;
        }
        if(virDomainGetInfo(domain, &domain_info) != 0)
        {
            break;
        }
        cpu_start = domain_info.cpuTime;

        clock_gettime(CLOCK_REALTIME, &starttime);
       
        sleep(interval);

        if(virDomainGetInfo(domain, &domain_info) != 0)
        {
            break;
        }
        cpu_end = domain_info.cpuTime;
        clock_gettime(CLOCK_REALTIME, &endtime);

        cputime = (cpu_end - cpu_start) * 1.0/1000;
        realtime = 1000000 * (endtime.tv_sec - starttime.tv_sec) + (endtime.tv_nsec - starttime.tv_nsec) * 1.0/1000;
        cpu_usage = (cputime * 10000) / realtime;
        
        pthread_mutex_lock(&cpu_rate_mutex);
        temp = vm_cpu_rate_head;
        while(NULL != temp)
        {
            if(strcmp(temp->name, vm_name) == 0)
            {
                temp->rate = cpu_usage;
                clock_gettime(CLOCK_REALTIME, &temp->last_time);
                break;
            }
            
            if(NULL == temp->next)
            {
                temp->next = malloc(sizeof(CPU_RATE));
                temp->next->name = malloc(strlen(vm_name) + 1);
                memset(temp->next->name, 0, strlen(vm_name) + 1);
                temp->next->next = NULL;    
                temp->next->rate = cpu_usage;
                clock_gettime(CLOCK_REALTIME, &temp->next->last_time);
                strcpy(temp->next->name, vm_name);
                break;
            }

            temp = temp->next;
        }
       
        if(NULL == vm_cpu_rate_head)
        {
            vm_cpu_rate_head = malloc(sizeof(CPU_RATE));
            vm_cpu_rate_head->name = malloc(strlen(vm_name) + 1);
            memset(vm_cpu_rate_head->name, 0, strlen(vm_name) + 1);
            strcpy(vm_cpu_rate_head->name, vm_name);
            vm_cpu_rate_head->next = NULL;    
            vm_cpu_rate_head->rate = cpu_usage;
            clock_gettime(CLOCK_REALTIME, &vm_cpu_rate_head->last_time);
        }
        pthread_mutex_unlock(&cpu_rate_mutex);
    
    }

}


