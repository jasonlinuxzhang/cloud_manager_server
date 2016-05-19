#ifndef _OPERATION
#define _OPERATIOn
#include "../cJSON/cJSON.h"
typedef struct _OPERATION
{
    char *operation_name;
    cJSON* (*handle)(cJSON *); 
}OPERATION;


cJSON *vm_list_fetch(cJSON *param);
const char *operation_code_to_string(int code);
cJSON *start_vm(cJSON *param);
cJSON *destroy_vm(cJSON *param);
cJSON *detail_vm(cJSON *param);
cJSON *define_vm(cJSON *param);

#endif 
