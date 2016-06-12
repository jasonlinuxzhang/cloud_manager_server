#include "common.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "log/log.h"
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
int try_connect()
{
    if(NULL == g_conn)
    {
        g_conn = virConnectOpen("qemu:///system");
        if(NULL == g_conn)
        {
            return -1;
        }
    }
    return 0;
}


int safewrite(int fd, void *buf, int count)
{
    int nwritten = 0;
    while (count > 0) {
        ssize_t r = write(fd, buf, count);

        if (r < 0 && errno == EINTR)
            continue;
        if (r < 0)
        {
            return r;
        }
        if (r == 0)
        {
            return nwritten;
        }
        buf = buf + r;
        count -= r;
        nwritten += r;
    }
    return nwritten;
}

int saferead(int sockfd, char *buf, int count)
{
    int read_count = 0;
    int real_count = 0;
    int count_left = count;
    while((read_count = read(sockfd, buf, count_left)))
    {
        if(read_count < 0)
        {
            if(EINTR == errno)
            {
                continue;
            }
            break;
        }
        real_count += read_count;
        if(count == real_count)
        {
            break;    
        }
    }
    return real_count;
}


int get_vm_port(char *vm_name)
{
    char cmd[128] = {0};
    char cmd_result[128] = {0};
    if(NULL == vm_name)
        return -1;
    snprintf(cmd, sizeof(cmd), "virsh domdisplay %s", vm_name);
    
    FILE *pfile = popen(cmd, "r"); 
    if(NULL == pfile)
    {
        log_error_message("%s execute fail", cmd);
        return -1;
    }
    
    int read_count = fread(cmd_result, sizeof(char), sizeof(cmd_result), pfile);
    if(read_count < 0)
    {
        log_error_message("read fail");
        goto clean;
    }
    
    char *pos = strrchr(cmd_result, ':'); 
    if(NULL == pos)
    {
        goto clean;
    }
    
    pos ++;
        
    int port;

    port = atoi(pos);    

    log_info_message("get %s port:%d", vm_name, port);

    return port;

clean:
    if(NULL != pfile)
    {
        pclose(pfile);
    }
}


char *get_vm_mac(char *name)
{
    char *mac_address = NULL;
    if(NULL == name)
    {
        return NULL;    
    }
    virDomainPtr domain = virDomainLookupByName(g_conn, name); 
    if(NULL == domain)
    {
        log_error_message("get %s domain fail", name);
        return NULL;
    }
    
    char *xml = virDomainGetXMLDesc(domain, 0);
    if(NULL == xml)
    {
        log_error_message("get %s xml fail", name);
        return NULL;
    }

    char *pos_start = strstr(xml, "mac address='");
    if(NULL == pos_start)
    {
        log_error_message("can't find mac location");
        return NULL;
    }

    pos_start += strlen("mac address='");
    
    char *pos_end = pos_start + strlen("xx:xx:xx:xx:xx:xx");
    
    mac_address = malloc(pos_end - pos_start + 1);
    if(NULL == mac_address)
    {
        log_error_message("alloc memory for mac fail");
        return NULL;
    }
    memset(mac_address, 0, pos_end - pos_start + 1);
    memcpy(mac_address, pos_start, pos_end - pos_start);
    
    return mac_address;
}

char *insert_string(char *source_string, const char *matching_string, const char *insert_string)
{
    if(NULL == source_string || NULL == matching_string || NULL == insert_string)
    {
        return NULL;
    }

    char *pos = strstr(source_string, matching_string);
    if(NULL == pos)
    {
        log_error_message("can't find %s", matching_string);
        return NULL;
    }
    pos += strlen(matching_string);
    
    int new_string_len = strlen(source_string) + strlen(insert_string) + 1;
    char *new_string = NULL, *temp_pos = NULL;
    temp_pos = malloc(new_string_len * sizeof(char));
    if(NULL == temp_pos)
    {
        log_error_message("alloc memory fail");
        return NULL;
    }
    memset(temp_pos, 0, new_string_len);
    new_string = temp_pos;

    memcpy(temp_pos, source_string, pos - source_string);
    temp_pos += (pos - source_string);

    memcpy(temp_pos, insert_string, strlen(insert_string));
    temp_pos += strlen(insert_string);

    memcpy(temp_pos, pos, strlen(source_string) - (pos - source_string));
    
    free(source_string);
    return new_string;
}


char *build_vm_xml(VM_ATTRIBUTE *attribute)
{
    char *xml_string = NULL, *xml_string_temp = NULL;
    int filefd = -1;
    char memory_buffer[10]= {0}, cpu_buffer[10] = {0}, disk_name_buffer[128] = {0};
    char system_type_buffer[128]  = {0};

    if(NULL == attribute)
    {
        return NULL;
    }

    filefd = open(XML_FILE_PATH, O_RDONLY);
    if(filefd < 0)
    {
        log_error_message("open file %s fail", XML_FILE_PATH);
        goto clean;
    }    
    
    xml_string = malloc(READ_FILE_MAX * sizeof(char));
    if(NULL == xml_string)
    {   
        log_error_message("alloc memory fail");
        goto clean;
    }
    memset(xml_string, 0, READ_FILE_MAX * sizeof(char));

    int read_count = saferead(filefd, xml_string, READ_FILE_MAX);
    if(read_count <= 0)
    {
        log_error_message("read file fail");
        goto clean;
    }
    xml_string[read_count] = '\0';

    xml_string_temp = insert_string(xml_string, "<name>", attribute->name); 
    if(NULL == xml_string_temp)
    {
        goto clean;
    }
    xml_string = xml_string_temp;

    sprintf(memory_buffer, "%d", attribute->memory_size * 1024 * 512);
    xml_string_temp = insert_string(xml_string, "<memory unit='KiB'>", memory_buffer);
    if(NULL == xml_string_temp)
    {
        goto clean;
    }
    xml_string = xml_string_temp;

    xml_string_temp = insert_string(xml_string, "<currentMemory unit='KiB'>", memory_buffer);
    if(NULL == xml_string_temp)
    {
        goto clean;
    }
    xml_string = xml_string_temp;
    
    sprintf(cpu_buffer, "%d", attribute->cpu_number);
    xml_string_temp = insert_string(xml_string, "<vcpu placement='static'>", cpu_buffer);
    if(NULL == xml_string_temp)
    {
        goto clean;
    }
    xml_string = xml_string_temp;

    sprintf(disk_name_buffer, "%s%s.qcow2", IMAGE_PATH, attribute->name);
    xml_string_temp = insert_string(xml_string, "type='qcow2'/>\n<source file='", disk_name_buffer);
    if(NULL == xml_string_temp)
    {
        goto clean;
    }
    xml_string = xml_string_temp;
    
    sprintf(system_type_buffer, "%s%s.iso", IMAGE_PATH, attribute->system_type);
    xml_string_temp = insert_string(xml_string, "device='cdrom'>\n<source file='", system_type_buffer);
    if(NULL == xml_string_temp)
    {
        goto clean;
    }

    xml_string = xml_string_temp;
    xml_string_temp = insert_string(xml_string, "<mac address='", attribute->mac_address);
    if(NULL == xml_string_temp)
    {
        goto clean;
    }
    xml_string = xml_string_temp;
    return xml_string;

clean: 

    if(NULL != xml_string)
    {
        free(xml_string);
    }
    return NULL;
}

char * replace(char *source_string, const char *first_match, const char *second_match, const char *dest_string)
{
    char *pos_start = NULL, *pos_end = NULL;
    char *new_string = NULL;
    int pos_offset = 0;
        

    if(NULL == source_string || NULL == first_match || NULL == second_match || NULL == dest_string)
    {
        log_error_message("param is null");
        return NULL;
    }

    pos_start = strstr(source_string, first_match);
    if(NULL == pos_start)
    {
        log_error_message("can't find %s", first_match); 
        return NULL;  
    }
    
    pos_end = strstr(source_string, second_match);
    if(NULL == pos_end)
    {
        log_error_message("can't find %s", second_match);
        return NULL;
    }

    new_string = malloc(strlen(source_string) + strlen(first_match) + strlen(second_match) + 1);
    if(NULL == new_string)
    {
        log_error_message("alloc memory fail");
        return NULL;
    } 
    memset(new_string, 0, strlen(source_string) + strlen(first_match) + strlen(second_match) + 1);

    pos_offset = 0;
    memcpy(new_string + pos_offset, source_string, pos_start + strlen(first_match)- source_string);
    
    pos_offset += (pos_start + strlen(first_match) - source_string);
    
    memcpy(new_string + pos_offset, dest_string, strlen(dest_string));
    pos_offset += strlen(dest_string);
        
    memcpy(new_string + pos_offset, pos_end, strlen(source_string) - (pos_end - source_string));

    free(source_string);
    
    return new_string;
}





