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

typedef struct {
    unsigned long utime;
    unsigned long stime;
    unsigned long starttime;
} ProcStat;

int read_proc_stat(pid_t pid, ProcStat *stat);

// Estructura auxiliar para rastrear procesos activos
typedef struct {
    ProcessInfo info;
    int encontrado;  // Flag para marcar si fue encontrado en el ciclo actual
} ActiveProcess;


// Funciones auxiliares para manejo de procesos activos
int find_process(pid_t pid);
void add_process(ProcessInfo info);
void remove_process(pid_t pid);
void update_process(ProcessInfo info, int idx);
void clear_process_list();
void show_process_stats();

// Funciones para manejo de valores previos (si necesitas persistencia)
void read_prev_times(pid_t pid, unsigned long *prev_user_time, unsigned long *prev_sys_time);
void write_prev_times(pid_t pid, unsigned long prev_user_time, unsigned long prev_sys_time);

// Funciones de cálculo de CPU
float total_cpu_usage(pid_t pid, unsigned long *prev_user_time, unsigned long *prev_sys_time);
float interval_cpu_usage(pid_t pid);

// Variables globales externas
extern ActiveProcess *procesos_activos;
extern int num_procesos_activos;
extern Config config;

#endif