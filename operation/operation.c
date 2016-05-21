#include <stdio.h>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "operation.h"
#include "../common.h"
#include "../log/log.h"
#include "../cJSON/cJSON.h"


OPERATION operation_group[] = {
    {"START", start_vm},
    {"FETCH", vm_list_fetch},
    {"DESTROY", destroy_vm},
    {"DEFINE", define_vm},
    {"DETAIL", detail_vm},
    {"UNDEFINE", undefine_vm},
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
        case 6: return "UNDEFINE";
        default: return NULL;
    }    
}

cJSON *undefine_vm(cJSON *param)
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
            get_libvirt_error();
            continue;
        }
        
        res = virDomainUndefine(domain);
        if(0 != res)
        {
            get_libvirt_error();
            continue;
        }
    }
    
    /* send list fetch result after start vms */
    return vm_list_fetch(NULL);
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
        get_libvirt_error();
        return NULL;
    }
    
    char *xml = virDomainGetXMLDesc(domain, 0);
    if(NULL == xml)
    {
        get_libvirt_error();
        return NULL;
    }

    if(virDomainGetState(domain, &vm_state, NULL, 0) != 0)
    {
        get_libvirt_error();
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
    cJSON *mac_object = NULL, *system_object = NULL;
    virDomainPtr *domains;
    int vmCount  = 0, i = 0;
    char *mac_temp = NULL, *xml_string = NULL;
    char create_img[1024] = {0};
    
    VM_ATTRIBUTE attribute;

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

    if(NULL != virDomainLookupByName(g_conn, name_object->valuestring))
    {
        get_libvirt_error();
        return NULL;
    }

    memory_object = cJSON_GetObjectItem(param, "MemorySize"); 
    if(NULL == memory_object || cJSON_Number != memory_object->type)
    {
        log_error_message("memory object is null or type is not number");
        return NULL;
    }
    
    disk_object = cJSON_GetObjectItem(param, "DiskSize");
    if(NULL == disk_object || cJSON_Number != disk_object->type)
    {
        log_error_message("disk object is null or type is not number");
        return NULL;
    }
    
    cpu_object = cJSON_GetObjectItem(param, "CpuNumber");
    if(NULL == cpu_object || cJSON_Number != cpu_object->type)
    {
        log_error_message("cpu object is null or type is not number");
        return NULL;
    }

    mac_object = cJSON_GetObjectItem(param, "Mac");
    if(NULL == mac_object || cJSON_String != mac_object->type)
    {
        log_error_message("mac object is null or type is not number");
        return NULL;
    }

    system_object = cJSON_GetObjectItem(param, "System");
    if(NULL == system_object || cJSON_String != system_object->type)
    {
        log_error_message("system object is null or type is not number");
        return NULL;
    }

    vmCount = virConnectListAllDomains(g_conn, &domains, 0);
    if(vmCount <= 0)
    {
        get_libvirt_error();
        return NULL;
    }

    for(i = 0; i < vmCount; i++)
    {
        mac_temp =  get_vm_mac((char *)virDomainGetName(domains[i])); 
        if(NULL == mac_temp)
        {
            continue;
        }
        if(strcmp(mac_temp, mac_object->valuestring) == 0)
        {
            log_info_message("this mac %s is already use by %s", mac_temp, virDomainGetName(domains[i]));
            free(mac_temp);
            break;
        }
        free(mac_temp);
    }
    
    if(i != vmCount)
    {
        return NULL;
    }

    memset(&attribute, 0, sizeof(attribute));
    attribute.name = (char *)name_object->valuestring;
    attribute.mac_address = (char *)mac_object->valuestring;
    attribute.system_type = (char *)system_object->valuestring;
    attribute.memory_size = memory_object->valueint;
    attribute.cpu_number = cpu_object->valueint; 

    xml_string = build_vm_xml(&attribute);    
    if(NULL == xml_string)
    {
        goto clean;
    }

    sprintf(create_img, "qemu-img create -f qcow2 %s%s.qcow2 %dG 0>/dev/null 1>/dev/null 2>/dev/null", IMAGE_PATH, name_object->valuestring, disk_object->valueint); 
    if(0 !=system(create_img))  
    {
        log_error_message("create img fail for %s", disk_object->valueint);
        goto clean;
    } 
    
    if(NULL == virDomainDefineXML(g_conn, xml_string))
    {
        get_libvirt_error();
        goto clean;
    } 
    
    free(xml_string);
    return vm_list_fetch(NULL); 

clean:
    if(NULL != xml_string)
    {
        free(xml_string);
    } 
    return NULL;
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
            get_libvirt_error();
            continue;
        }
        
        res = virDomainDestroy(domain);
        if(0 != res)
        {
            get_libvirt_error();
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
            get_libvirt_error();
            continue;
        }
        
        res = virDomainCreate(domain);
        if(0 != res)
        {
            get_libvirt_error();
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
        get_libvirt_error();
        goto clean;
    }

    inactive_vm_count = virConnectListAllDomains(g_conn, &inactive_domains, VIR_CONNECT_LIST_DOMAINS_INACTIVE);
    if(inactive_vm_count < 0)
    {
        get_libvirt_error();
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






