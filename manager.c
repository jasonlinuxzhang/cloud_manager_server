#include <stdio.h> 
#include "network/network.h"
#include "log/log.h"
#include "common.h"
#include <errno.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include "cJSON/cJSON.h"
#include "operation/operation.h"
#include "host/host.h"
#include "vm_monitor/vm_monitor.h"
#include<sys/param.h>
#include<sys/types.h>
#include<sys/stat.h>

void *handle_operation(void *arg);
int search_operation_func(int operation_code);
cJSON * handle_cJSON_and_search_operation(char *json_string);

extern OPERATION operation_group[];

void init_daemon()
{
    int pid, i;
    pid = fork();
    if(pid < 0)
    {
        log_error_message("first fork fail");     
        exit(1);
    }
    else if(pid > 0)
    {
        exit(0);
    }

    setsid(); 
    pid=fork();
    if(pid>0)
    {
        exit(0);
    }
    else if(pid<0)    
    {
        exit(1);
    }

    for(i=0;i<NOFILE;i++)
        close(i);
    chdir("/home"); 
    umask(0);
    log_info_message("now this is daemon");
}

int redirect_std_to_file()
{
    FILE *fp = NULL;
    if((fp = fopen(LOG_FILE_PATH, "a+")) == NULL)
    {
        log_error_message("open file fail");
        return -1;
    }
    if(dup2(fileno(fp), STDOUT_FILENO) == -1)
    {
        log_error_message("redirect stdout fail");
        return -1;
    }
    if(dup2(fileno(fp), STDERR_FILENO) == -1)
    {
        log_error_message("redirect stderr fail");
        return -1;
    }
    fclose(fp);
    log_info_message("redirect stdout and stderr to log file success");
}

int manager_init()
{
    g_exit = 0;
    log_init();
    if(-1 == redirect_std_to_file() || -1 == socket_init() || -1 == host_monitor_init() || -1 == vm_monitor_init())
    {
        log_error_message("some init fail");
        return -1;
    }
}

int main()
{
    int res = 0, len = 0;
    char client_ip[128] = {0};
    struct sockaddr_in client_addr;
    int client_fd = 0, client_fd_bak = 0;
    pthread_t tid;

//    init_daemon();

    res = manager_init();
    if(-1 == res)
    {
        log_error_message("manage init fail");
        return -1;
    }
    log_info_message("start listen");
    while(!g_exit)
    {
        memset(&client_addr, 0, sizeof(client_addr));
        client_fd = accept(g_sockfd, (struct sockaddr *)(&client_addr), &len); 
        if(-1 == client_fd)
        {
            if(EINTR == errno)
            {
                continue;
            }
            log_error_message("accept fail");
            break;
        }
        else
        {
            memset(client_ip, 0, sizeof(client_ip));
            inet_ntop(AF_INET, (void *)&client_addr.sin_addr, client_ip, sizeof(client_ip));
            log_debug_message("new connect ip:%s", client_ip);
            client_fd_bak = client_fd;
            pthread_create(&tid, NULL, handle_operation, (void *)&client_fd_bak);
        }
    }
    
    return 0;
}



void *handle_operation(void *arg)
{
    int client_fd = *(int *)arg;
    char *buf = NULL, *operation_result_string = NULL;
    char *response_string = NULL;
    int json_len = 0, count = 0, read_count = 0, write_count = 0;
    char json_len_string[1024] = {0};
    cJSON *operation_object = NULL;
    
    /*read len character, until meer '{'*/
    while(read(client_fd, json_len_string + count, sizeof(char)) == 1)
    {
        if('{' == json_len_string[count])
        {
            json_len_string[count] = '\0';
            break;
        }
        count ++;
    }
    log_debug_message("read json length(head):%s, count:%d", json_len_string, count);
    
    sscanf(json_len_string, "%d", &json_len);
    
    if(json_len <= 0)
    {
        log_error_message("json length string to number fail, read count: %d", count);
        goto clean;
    }
    else if(json_len < 1024 * 1024)
    {
        buf = malloc(json_len * sizeof(char) + 1);
        if(NULL == malloc)
        {
            goto clean;
        }
        memset(buf, 0, json_len * sizeof(char) + 1);
        buf[0] = '{';
        read_count = saferead(client_fd, buf + 1, json_len - 1);
        if(read_count != json_len - 1)
        {
            log_error_message("expect read count:%d real read count:%d not equal", json_len - 1, read_count);
            goto clean;
        }
        /* search handle*/
        operation_object = handle_cJSON_and_search_operation(buf);
        if(NULL == operation_object)
        {
            log_error_message("handle_cJSON_and_search_operation fail");
            goto clean;
        }
        operation_result_string = cJSON_PrintUnformatted(operation_object);
        if(NULL == operation_result_string)
        {
            goto clean;
        }
    }
clean:
    if(NULL != buf)
    {
        free(buf);
    }    
    if(NULL != operation_object)
    {
        cJSON_Delete(operation_object);
    }
    if(NULL != operation_result_string)
    {
        write_count = safewrite(client_fd, operation_result_string, strlen(operation_result_string));
        if(write_count != strlen(operation_result_string))
        {
            log_error_message("write response fail, expect write count %d real write count %d not equal", strlen(operation_result_string), write_count);
        }
        else
        {
            log_debug_message("write response success:%s", operation_result_string);
        }
        
    }
    close(client_fd);
}


cJSON * handle_cJSON_and_search_operation(char *json_string)
{
    cJSON *object = NULL, *message_type = NULL, *request_type = NULL;
    cJSON *operation_result = NULL;
    int index = 0;
    object = cJSON_Parse(json_string);
    if(NULL == object)
    {
        log_error_message("parse JSON fail");
        return NULL;
    }
    
    message_type = cJSON_GetObjectItem(object, "MessageType");
    if(NULL == message_type || message_type->type != cJSON_Number)
    {
        log_error_message("can't parse message type, or message type is not number type");
        goto clean;
    }
    
    request_type = cJSON_GetObjectItem(object, "RequestType");
    if(NULL == request_type || request_type->type != cJSON_Number) 
    {
        log_error_message("can't parse request type, or request type is not number type");
    }    

    index = search_operation_func(request_type->valueint); 
    if(-1 != index)
    {
        operation_result = operation_group[index].handle(cJSON_GetObjectItem(object, "Param"));
    }
    else
    {
        log_error_message("can't find operation funcation");
    }
    
clean:
    if(NULL != object)
    {
        cJSON_Delete(object);
    }
    return operation_result;
}

int search_operation_func(int operation_code)
{
    int i = 0;
    const char *operation_name = operation_code_to_string(operation_code); 
    if(NULL == operation_name)
    {
        return -1;
    }
    while(NULL != operation_group[i].operation_name)    
    {
        if(strcmp(operation_group[i].operation_name, operation_name) == 0)
        {
            break;   
        }
        i ++;
    }
    if(NULL != operation_group[i].operation_name)
    {
        return i;
    }
    else
    {
        return -1;
    }
}
