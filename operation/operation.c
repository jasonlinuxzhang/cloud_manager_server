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
#include "../host/host.h"
#include "../vm_monitor/vm_monitor.h"

static cJSON *get_host_info();
static cJSON *get_vm_info();

OPERATION operation_group[] = {
    {"START", start_vm},
    {"FETCH", vm_list_fetch},
    {"DESTROY", destroy_vm},
    {"DEFINE", define_vm},
    {"DETAIL", detail_vm},
    {"UNDEFINE", undefine_vm},
    {"MONITOR", yun_monitor},
    {"CHANGE", change_vm_config},
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
        case 7: return "MONITOR";
        case 8: return "CHANGE";
        default: return NULL;
    }    
}

cJSON *change_vm_config(cJSON *param)
{
    char *buf = NULL;
    char *xml = NULL, *new_xml = NULL;
    cJSON *vm_name = NULL, *disk_size = NULL, *cpu_number = NULL, *memory_size = NULL, *cdrom_type = NULL;
    int status = 0;
    char memory_buff[10] = {0}, cpu_buff[10] = {0}, disk_buff[10] = {0};
    char cdrom_sourcefile[1024] = {0};
    cJSON *return_object = NULL;
    virDomainPtr domain;
    if(NULL == param)
    {
        return NULL;
    }
    
    vm_name = cJSON_GetObjectItem(param, "VmName");
    if(NULL == vm_name || cJSON_String != vm_name->type)
    {
        log_error_message("vm name object is null or type mismatch");
        goto clean;
    }

    disk_size = cJSON_GetObjectItem(param, "DiskSize");
    if(NULL != disk_size && cJSON_Number != disk_size->type)
    {
        log_error_message("disk size object is null or type mismatch");
        goto clean;
    }
    
    cpu_number = cJSON_GetObjectItem(param, "CpuNumber");
    if(NULL != cpu_number && cJSON_Number != cpu_number->type)
    {
        log_error_message("cpu number object is null or type mismatch");
        goto clean;
    }
    
    memory_size = cJSON_GetObjectItem(param, "MemorySize");
    if(NULL != memory_size && cJSON_Number != memory_size->type)
    {
        log_error_message("memory size object is null or type mismatch");
        goto clean;
    }

    cdrom_type = cJSON_GetObjectItem(param, "CdromType");
    if(NULL != cdrom_type && cJSON_String != cdrom_type->type)
    {
        log_error_message("cdrom type object is null or type mismatch");
        goto clean;
    }

    try_connect();
    
    domain = virDomainLookupByName(g_conn, vm_name->valuestring);
    if(NULL == domain)
    {
        log_error_message("can't get domain by %s", vm_name->valuestring);
        goto clean;
    } 

    xml = virDomainGetXMLDesc(domain, VIR_DOMAIN_XML_INACTIVE);
    if(NULL == xml)
    {
        log_error_message("can't get %s xml", vm_name->valuestring);
        goto clean;
    }
    
    new_xml = malloc(strlen(xml) + 1);
    if(new_xml == NULL)
    {
        log_error_message("alloc memory fail");
        xml = NULL;
        goto clean;
    }
    memset(new_xml, 0, strlen(xml) + 1);
    memcpy(new_xml, xml, strlen(xml));
    xml = new_xml;

    if(0 != virDomainGetState(domain, &status, NULL, 0))
    {
        log_error_message("can't get %s state", vm_name->valuestring);
        goto clean;
    }
    
    if(status == VIR_DOMAIN_SHUTOFF)   
    {
        if(NULL != memory_size)
        {
            sprintf(memory_buff, "%d", memory_size->valueint * 1024 * 512);
            new_xml = replace(xml, "<memory unit='KiB'>", "</memory>", memory_buff);
            if(NULL == new_xml)
            {
                goto clean;
            }
            xml = new_xml;

            new_xml  = replace(xml, "<currentMemory unit='KiB'>", "</currentMemory>", memory_buff);
            if(NULL == new_xml)
            {
                goto clean;
            }
            xml = new_xml;
        }
        if(NULL != cpu_number)
        {
            sprintf(cpu_buff, "%d", cpu_number->valueint);
            new_xml = replace(xml, "<vcpu placement='static'>", "</vcpu>", cpu_buff);
            if(NULL == new_xml)
            {
                goto clean;
            } 
            xml = new_xml;
        }
        if(0 != virDomainUndefine(domain))
        {
            get_libvirt_error();
            goto clean;
        }

        if(NULL == virDomainDefineXML(g_conn, xml))
        {
            get_libvirt_error();
            goto clean;
        }
    }
    else
    {
        if(NULL != cdrom_type)
        {
            sprintf(cdrom_sourcefile, "<disk type='file' device='cdrom'>\n<driver name='qemu' type='raw'/>\n<source file='%s%s'/>\n<target dev='hda' bus='ide'/>\n<readonly/>",  IMAGE_PATH, cdrom_type->valuestring);
        }
        if(0 != virDomainUpdateDeviceFlags(domain, cdrom_sourcefile, VIR_DOMAIN_DEVICE_MODIFY_FORCE))
        {
            log_error_message("update cdrom fail"); 
            goto clean;
        }

    }
    
    return_object = cJSON_CreateObject();
    if(NULL == return_object)
    {
        goto clean;
    } 
  
    cJSON_AddNumberToObject(return_object, "MessageType", 1);
    cJSON_AddNumberToObject(return_object, "RequestType", 8);
clean:   
    
    if(NULL != xml)
    {
        free(xml);
    }
    
    return NULL;
     
}


cJSON *yun_monitor(cJSON *param)
{
    cJSON *ret =get_vm_info();
    if(NULL == ret)
    {
        log_error_message("can't get vm info");
    }
    cJSON *operation_result_object = cJSON_CreateObject();
    if(NULL == operation_result_object)
    {
        log_error_message("create object fail");
        goto clean;
    }
    if(NULL != ret)
    {
        cJSON_AddItemToObject(operation_result_object, "VmInfo", ret);
    }
    cJSON *host_info_obejct = get_host_info(NULL);
    if(NULL == host_info_obejct)
    {
        log_error_message("cab't get host info");
    }
    
    cJSON_AddItemToObject(operation_result_object, "HostInfo", host_info_obejct);

    cJSON *response_object = cJSON_CreateObject();
    if(NULL == response_object)
    {
        log_error_message("create return object fail");
        goto clean;
    } 

    cJSON_AddNumberToObject(response_object, "MessageType", 1);
    cJSON_AddNumberToObject(response_object, "RequestType", 7);
    cJSON_AddItemToObject(response_object, "OperationResult", operation_result_object);

    return response_object; 

clean:
    if(NULL == ret)
    {
        cJSON_Delete(ret);
    }
    if(NULL == operation_result_object)
    {
        cJSON_Delete(operation_result_object);
    }
    return NULL;

}


static cJSON *get_vm_info()
{
    return get_vm_info_impl();
}

static cJSON *get_host_info()
{
    HOST_INFO *host_info = NULL;
    host_info = get_host_info_impl();    
    if(NULL == host_info)
    {
        return NULL;
    }

    cJSON *operation_result = cJSON_CreateObject();
    if(NULL == operation_result)
    {
        goto clean;
    }

    cJSON_AddNumberToObject(operation_result, "CpuRate", host_info->cpu_rate);
    cJSON_AddNumberToObject(operation_result, "MemoryTotal", host_info->memory_total);
    cJSON_AddNumberToObject(operation_result, "MemoryFree", host_info->memory_free);
    cJSON_AddNumberToObject(operation_result, "DiskTotal", host_info->disk_total);
    cJSON_AddNumberToObject(operation_result, "DiskFree", host_info->disk_free);
 
    return operation_result;
clean:
    if(NULL != operation_result)
    {
        cJSON_Delete(operation_result);
        operation_result = NULL;
    }
    if(NULL != host_info)
    {
        free(host_info);
    }
    return NULL;
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
    if(vmCount < 0)
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
    char rm_disk_cmd[128] = {0};
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
        
        memset(rm_disk_cmd, 0, sizeof(rm_disk_cmd));
        sprintf(rm_disk_cmd, "rm %s%s -f", IMAGE_PATH, vm_object->valuestring);
       
        system(rm_disk_cmd); 

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






