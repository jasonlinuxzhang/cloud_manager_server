#include <stdio.h>
#include <libvirt/libvirt.h>
#include <stdlib.h>
#include <string.h>
#include "operation.h"
#include "../common.h"
#include "../log/log.h"
#include "../cJSON/cJSON.h"

OPERATION operation_group[] = {
    {"fetch", vm_list_fetch},
    {NULL, NULL}
};


const char *operation_code_to_string(int code)
{
    
}



cJSON *vm_list_fetch(cJSON *param)
{
    int active_vm_count = 0, inactive_vm_count = 0;
    int i = 0;
    virDomainPtr *active_domains = NULL, *inactive_domains = NULL;
    cJSON *return_object = NULL, *active_vm_list = NULL, *inactive_vm_list = NULL;
    
    const char **active_vm_name, **inactive_vm_name;
    const char *one_vm_name = NULL;

    active_vm_count = virConnectListAllDomains(g_conn, &active_domains, VIR_CONNECT_LIST_DOMAINS_ACTIVE);
    if(active_vm_count < 0)
    {
        log_error_message("list active domains fail");
        goto clean;
    }

    inactive_vm_count = virConnectListAllDomains(g_conn, &inactive_domains, VIR_CONNECT_LIST_DOMAINS_INACTIVE);
    if(inactive_vm_count < 0)
    {
        log_error_message("list inactive domains fail");
        goto clean;
    }

    active_vm_name = (const char **)malloc(sizeof(char *) * active_vm_count);
    for(i = 0; i < active_vm_count; i++)
    {
        one_vm_name = virDomainGetName(active_domains[i]); 
        active_vm_name[i] = malloc(strlen(one_vm_name) + 1); 
    }
    active_vm_list = cJSON_CreateStringArray(active_vm_name, active_vm_count);
    if(NULL == active_vm_list)
    {
        log_error_message("create cJSON object fail");
        goto clean;
    }

clean:

    return return_object;
}






