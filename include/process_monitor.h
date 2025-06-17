#ifndef PROCESS_MONITOR_H  
#define PROCESS_MONITOR_H  

#include <pthread.h>

#define CONFIG_PATH "/etc/matcomguard.conf"

typedef struct {
    pid_t pid;
    char name[256];
    float cpu_usage;
    float cpu_time;
    float mem_usage;
    time_t inicio_alerta;
    int alerta_activa;
} ProcessInfo;

void monitor_processes();
ProcessInfo* get_process_info(pid_t pid);

typedef struct {
    float max_cpu_usage;
    float max_ram_usage;
    int check_interval;
    int alert_duration;
    char **white_list;
    int num_white_processes;
} Config;

void load_config();

#endif  