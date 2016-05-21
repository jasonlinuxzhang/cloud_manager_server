#ifndef _COMMON
#define _COMMON

#include <libvirt/libvirt.h>

#define XML_FILE_PATH "/home/virtual_machine/xml/vm.xml"
#define IMAGE_PATH "/home/virtual_machine/images/"
#define READ_FILE_MAX (1024*1024)

#define get_libvirt_error()                            \
{                                                    \
    virErrorPtr err = virGetLastError();             \
    if(NULL == err)                                  \
    {                                                   \
        log_error_message("libvirt api return NULL.");  \
    }                                                   \
    else                                                \
    {                                                   \
        log_error_message("libvirt error domian:%d, error code:%d, message:%d", err->domain, err->code, err->message);  \
    }   \
}   

typedef struct _VM_ATTRIBUTE
{
    char *name;
    char *mac_address;
    char *system_type;
    int disk_size;
    int memory_size;
    int cpu_number;
}VM_ATTRIBUTE;

int g_exit;
virConnectPtr g_conn;
int safewrite(int fd, void *buf, int count);
int saferead(int sockfd, char *buf, int count);
int try_connect();
int get_vm_port(char *vm_name);
char *get_vm_mac(char *name);
char *build_vm_xml(VM_ATTRIBUTE *attribute);
#endif
