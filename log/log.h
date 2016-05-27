#ifndef _LOG
#define _LOG

#include <pthread.h>

pthread_mutex_t log_thread_mutex;

int gLogFd;
int log_filter;

typedef enum _LOG_LEVEL
{
    DEBUG = 0,
    INFO,
    WARNING,
    ERROR 
}LOG_LEVEL;

void log_file_open();
void log_message(LOG_LEVEL log_evel, int line, const char *funcname, const char *fmt, ...);
void *log_monitor(void *argv);
int log_init();

#define LOG_FILE_PATH "/var/log/yun_manager.log"

#define log_error_int(line, funcname, ...)  \
    log_message(ERROR, line, funcname,  ##__VA_ARGS__)
#define log_info_int(line, funcname, ...)  \
    log_message(INFO, line, funcname, ##__VA_ARGS__)
#define log_warning_int(line, funcname, ...)  \
    log_message(WARING, line, funcname, ##__VA_ARGS__)
#define log_debug_int(line, funcname, ...)  \
    log_message(DEBUG, line, funcname, ##__VA_ARGS__)

#define log_error_message(...)  log_error_int(__LINE__, __func__, ##__VA_ARGS__)
#define log_info_message(...)  log_info_int(__LINE__, __func__,## __VA_ARGS__)
#define log_warning_message(...)  log_waring_int(__LINE__, __func__, ##__VA_ARGS__)
#define log_debug_message(...)  log_debug_int(__LINE__, __func__, ##__VA_ARGS__)

#endif
