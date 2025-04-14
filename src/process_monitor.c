#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include "process_monitor.h"

void monitor_processes() {
    DIR *dir = opendir("/proc");
    if (!dir) {
        perror("Error al abrir /proc");
        return;
    }

    struct dirent *entry;
    printf("Procesos con alto uso de CPU/MEM:\n");
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && atoi(entry->d_name) != 0) {
            pid_t pid = atoi(entry->d_name);
            ProcessInfo info = get_process_info(pid);
            if (info.cpu_usage > MAX_CPU_USAGE || info.mem_usage > MAX_MEM_USAGE) {
                printf("[ALERTA] PID: %d, Nombre: %s, CPU: %.2f%%, Mem: %.2f%%\n",
                       pid, info.name, info.cpu_usage, info.mem_usage);
            }
        }
    }
    closedir(dir);
}