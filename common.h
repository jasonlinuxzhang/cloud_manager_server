#ifndef _COMMON
#define _COMMON
#include <libvirt/libvirt.h>
int g_exit;
virConnectPtr g_conn;
int safewrite(int fd, void *buf, int count);
int saferead(int sockfd, char *buf, int count);
int try_connect();
int get_vm_port(char *vm_name);
#endif
