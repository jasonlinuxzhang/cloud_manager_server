#ifndef _OPERATION
#define _OPERATIOn
#include "../cJSON/cJSON.h"
typedef struct _OPERATION
{
    char *operation_name;
    cJSON* (*handle)(cJSON *); 
}OPERATION;


cJSON *vm_list_fetch(cJSON *param);
#endif 
