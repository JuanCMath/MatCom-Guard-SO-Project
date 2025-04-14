#ifndef PROCESS_MONITOR_H  
#define PROCESS_MONITOR_H  

#define MAX_CPU_USAGE 90.0  // Umbral para alertas de CPU
#define MAX_MEM_USAGE 80.0  // Umbral para alertas de memoria

typedef struct {
    pid_t pid;
    char name[100];
    float cpu_usage;
    float mem_usage;
} ProcessInfo;

void monitor_processes();
ProcessInfo* get_process_info(pid_t pid);

#endif  