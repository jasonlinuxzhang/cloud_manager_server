#include <stdio.h>
#include <libvirt/libvirt.h>
#include <stdlib.h>
#include <string.h>
#include "operation.h"
#include "../common.h"
#include "../log/log.h"
#include "../cJSON/cJSON.h"

#define READ_FILE_MAX (1024*1024)

OPERATION operation_group[] = {
    {"START", start_vm},
    {"FETCH", vm_list_fetch},
    {"DESTROY", destroy_vm},
    {"DEFINE", define_vm},
    {"DETAIL", detail_vm},
    {NULL, NULL}
};

const char *operation_code_to_string(int code)
{
    switch(code)
    {
        case 0: return "START";
        case 2: return "FETCH";
        case 3: return "DESTROY";
        case 4: return "DEFINE";
        case 5: return "DETAIL";
        default: return NULL;
    }    
}


cJSON *detail_vm(cJSON *param)
{
    cJSON *name_object = NULL;
    cJSON *return_object = NULL, *operation_result_object = NULL;
    int vm_port = -1;
    virDomainPtr domain;
    int vm_state = 0;
    if(NULL == param || cJSON_Object != param->type)
    {
        log_error_message("JSON object is null orJSON object is not array");
        return NULL;
    }
    
    name_object = cJSON_GetObjectItem(param, "Name");
    if(NULL == name_object || cJSON_String != name_object->type)
    {
        log_error_message("name object is null or type is not string");
        return NULL;
    }

    try_connect(); 
    
    domain = virDomainLookupByName(g_conn, name_object->valuestring);
    if(NULL == domain)
    {
        log_error_message("can't find domain by %s", name_object->valuestring);
        return NULL;
    }
    
    char *xml = virDomainGetXMLDesc(domain, 0);
    if(NULL == xml)
    {
        log_error_message("can't get domain xml");
        return NULL;
    }

    if(virDomainGetState(domain, &vm_state, NULL, 0) != 0)
    {
        log_error_message("can't get domain state");
        return NULL;
    }

    if(vm_state == VIR_DOMAIN_RUNNING)
    {
        vm_port = get_vm_port(name_object->valuestring); 
    }

    return_object = cJSON_CreateObject();
    if(NULL == return_object)
    {
        goto clean;
    }
    cJSON_AddNumberToObject(return_object, "MessageType", 1);
    cJSON_AddNumberToObject(return_object, "RequestType", 5);
    operation_result_object = cJSON_CreateObject();
    if(NULL == operation_result_object)
    {
        goto clean;
    }
    
    cJSON_AddStringToObject(operation_result_object, "Xml", xml);
    cJSON_AddNumberToObject(operation_result_object, "Port", vm_port);
    cJSON_AddItemToObject(return_object, "Operation_Result", operation_result_object);
    

    return return_object;

clean:
    if(NULL != return_object)
    {
        cJSON_Delete(return_object);
    } 
    if(NULL != operation_result_object)
    {
        cJSON_Delete(operation_result_object);
    }
}


cJSON *define_vm(cJSON *param)
{
    cJSON *cpu_object = NULL, *disk_object = NULL, *memory_object = NULL, *name_object = NULL;
    if(NULL == param || cJSON_Object != param->type)
    {
        log_error_message("JSON object is null orJSON object is not array");
        return NULL;
    }
    
    name_object = cJSON_GetObjectItem(param, "Name");
    if(NULL == name_object || cJSON_String != name_object->type)
    {
        log_error_message("name object is null or type is not string");
        return NULL;
    }

    try_connect();

    if(NULL != virDomainLookupByName, name_object->valuestring)
    {
        log_error_message("this name is already use");
        return NULL;
    }
    
    
}

cJSON *destroy_vm(cJSON *param)
{
    int vm_count = 0;
    cJSON *vm_object = NULL;
    virDomainPtr domain;
    int res = 0, i = 0;
    if(NULL == param || cJSON_Array != param->type) 
    {
        log_error_message("JSON object is null orJSON object is not array");
        return NULL;
    }

    vm_count = cJSON_GetArraySize(param);
    if(vm_count <=0)
    {
        log_info_message("No vm");
    }    

    try_connect();    

    for(i = 0; i < vm_count; i++)
    {
        vm_object = cJSON_GetArrayItem(param, i);
        if(NULL == vm_object)
            continue;
        if(cJSON_String != vm_object->type)
        {
            continue;
        }
        domain = virDomainLookupByName(g_conn, vm_object->valuestring);
        if(NULL == domain)
        {
            log_error_message("can't get %s domain", vm_object->valuestring);
            continue;
        }
        
        res = virDomainDestroy(domain);
        if(0 != res)
        {
            log_error_message("%s destroy fail", vm_object->valuestring);
            continue;
        }
    }
    
    /* send list fetch result after start vms */
    return vm_list_fetch(NULL);
}

cJSON *start_vm(cJSON *param)
{
    int vm_count = 0;
    cJSON *vm_object = NULL;
    virDomainPtr domain;
    int res = 0, i = 0;
    if(NULL == param || cJSON_Array != param->type) 
    {
        log_error_message("JSON object is null orJSON object is not array");
        return NULL;
    }

    vm_count = cJSON_GetArraySize(param);
    if(vm_count <=0)
    {
        log_info_message("No vm");
    }    

    try_connect();    

    for(i = 0; i < vm_count; i++)
    {
        vm_object = cJSON_GetArrayItem(param, i);
        if(NULL == vm_object)
            continue;
        if(cJSON_String != vm_object->type)
        {
            continue;
        }
        domain = virDomainLookupByName(g_conn, vm_object->valuestring);
        if(NULL == domain)
        {
            log_error_message("can't get %s domain", vm_object->valuestring);
            continue;
        }
        
        res = virDomainCreate(domain);
        if(0 != res)
        {
            log_error_message("%s start fail", vm_object->valuestring);
            continue;
        }
    }
    
    /* send list fetch result after start vms */
    return vm_list_fetch(NULL);
}


cJSON *vm_list_fetch(cJSON *param)
{
    int active_vm_count = 0, inactive_vm_count = 0;
    int i = 0;
    virDomainPtr *active_domains = NULL, *inactive_domains = NULL;
    cJSON *return_object = NULL, *active_vm_list = NULL, *inactive_vm_list = NULL;
    cJSON *operation_result_object = NULL; 
    char **active_vm_name, **inactive_vm_name;
    char *one_vm_name = NULL;
    
    if(-1 == try_connect())
    {
        goto clean;
    }

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

    active_vm_name = (char **)malloc(sizeof(char *) * active_vm_count);
    for(i = 0; i < active_vm_count; i++)
    {
        one_vm_name = (char *)virDomainGetName(active_domains[i]); 
        active_vm_name[i] = malloc(strlen(one_vm_name) + 1); 
        memset((char *)active_vm_name[i], 0, strlen(one_vm_name) + 1);
        strcpy((char *)active_vm_name[i], one_vm_name);
    }
    active_vm_list = cJSON_CreateStringArray((const char **)active_vm_name, active_vm_count);
    if(NULL == active_vm_list)
    {
        log_error_message("create cJSON String Array fail");
        goto clean;
    }
    
    inactive_vm_name = (char **)malloc(sizeof(char *) * inactive_vm_count);
    for(i = 0; i < inactive_vm_count; i++)
    {
        one_vm_name = (char *)virDomainGetName(inactive_domains[i]); 
        inactive_vm_name[i] = malloc(strlen(one_vm_name) + 1); 
        memset((char *)inactive_vm_name[i], 0, strlen(one_vm_name) + 1);
        strcpy((char *)inactive_vm_name[i], one_vm_name);
    }
    inactive_vm_list = cJSON_CreateStringArray((const char **)inactive_vm_name, inactive_vm_count);
    if(NULL == inactive_vm_list)
    {
        log_error_message("create cJSON String Array fail");
        goto clean;
    }
    operation_result_object = cJSON_CreateObject();
    cJSON_AddItemToObject(operation_result_object, "Active_Vm", active_vm_list);
    cJSON_AddItemToObject(operation_result_object, "Inactive_Vm", inactive_vm_list);

    return_object = cJSON_CreateObject();
    cJSON_AddNumberToObject(return_object, "MessageType", 1);
    cJSON_AddNumberToObject(return_object, "RequestType", 2);
    cJSON_AddItemToObject(return_object, "Operation_Result", operation_result_object);
clean:

    return return_object;
}






