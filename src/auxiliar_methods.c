#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/sysinfo.h>
#include "process_monitor.h"

// Variables globales para procesos activos (movidas desde process_monitor.c)
extern ActiveProcess *procesos_activos;
extern int num_procesos_activos;
extern Config config;

// ===== MÉTODOS AUXILIARES PARA MANEJO DE PROCESOS ACTIVOS =====

// Busca un proceso por pid, retorna índice o -1 si no está
int find_process(pid_t pid) {
    for (int i = 0; i < num_procesos_activos; i++) {
        if (procesos_activos[i].info.pid == pid) {
            return i;
        }
    }
    return -1;
}

// Agrega un proceso nuevo al array dinámico
void add_process(ProcessInfo info) {
    procesos_activos = realloc(procesos_activos, 
                              (num_procesos_activos + 1) * sizeof(ActiveProcess));
    
    if (procesos_activos == NULL) {
        fprintf(stderr, "Error: No se pudo asignar memoria para nuevo proceso\n");
        return;
    }
    
    procesos_activos[num_procesos_activos].info = info;
    procesos_activos[num_procesos_activos].encontrado = 1;
    num_procesos_activos++;
    
    printf("[NUEVO PROCESO] PID: %d, Nombre: %s\n", info.pid, info.name);
}

// Elimina un proceso por pid
void remove_process(pid_t pid) {
    int idx = find_process(pid);
    if (idx == -1) return;
    
    printf("[PROCESO TERMINADO] PID: %d, Nombre: %s\n", 
           procesos_activos[idx].info.pid, procesos_activos[idx].info.name);
    
    // Mover todos los elementos hacia la izquierda
    for (int i = idx; i < num_procesos_activos - 1; i++) {
        procesos_activos[i] = procesos_activos[i + 1];
    }
    
    num_procesos_activos--;
    
    if (num_procesos_activos > 0) {
        procesos_activos = realloc(procesos_activos, 
                                  num_procesos_activos * sizeof(ActiveProcess));
    } else {
        free(procesos_activos);
        procesos_activos = NULL;
    }
}

// Actualiza la información de un proceso existente
void update_process(ProcessInfo info, int idx) {
    if (idx < 0 || idx >= num_procesos_activos) return;
    
    procesos_activos[idx].info = info;
    procesos_activos[idx].encontrado = 1;
}

// Limpia toda la lista de procesos activos (al finalizar)
void clear_process_list() {
    if (procesos_activos != NULL) {
        free(procesos_activos);
        procesos_activos = NULL;
    }
    num_procesos_activos = 0;
    printf("[INFO] Lista de procesos activos limpiada\n");
}

// Función auxiliar para mostrar estadísticas de procesos activos
void show_process_stats() {
    printf("\n=== ESTADÍSTICAS DE PROCESOS ACTIVOS ===\n");
    printf("Total de procesos monitoreados: %d\n", num_procesos_activos);
    
    int alertas_cpu = 0, alertas_mem = 0;
    for (int i = 0; i < num_procesos_activos; i++) {
        ProcessInfo *p = &procesos_activos[i].info;
        if (p->cpu_usage > config.max_cpu_usage) alertas_cpu++;
        if (p->mem_usage > config.max_ram_usage) alertas_mem++;
    }
    
    printf("Procesos con alta CPU: %d\n", alertas_cpu);
    printf("Procesos con alta memoria: %d\n", alertas_mem);
    printf("=========================================\n\n");
}

// ===== FIN MÉTODOS AUXILIARES PARA MANEJO DE PROCESOS ACTIVOS=====

int read_proc_stat(pid_t pid, ProcStat *stat) {
    char stat_path[256];
    sprintf(stat_path, "/proc/%d/stat", pid);
    FILE *fp = fopen(stat_path, "r");
    if (!fp) return -1;

    // Leer los campos 14 (utime), 15 (stime), 22 (starttime)
    int scanned = fscanf(fp,
        "%*d "      // pid
        "%*s "      // comm
        "%*c "      // state
        "%*d %*d %*d %*d %*d "
        "%*u %*u %*u %*u %*u "
        "%lu %lu %*d %*d %*d %*d %*d %*d %*u %*u %*d %*u %lu",
        &stat->utime, &stat->stime, &stat->starttime);

    fclose(fp);
    return (scanned == 3) ? 0 : -1;
}

void get_stat_file_path(pid_t pid, char *path, size_t size) {
    snprintf(path, size, "/tmp/procstat_%d.dat", pid);
}

void read_prev_times(pid_t pid, unsigned long *prev_user_time, unsigned long *prev_sys_time) {
    char path[64];
    get_stat_file_path(pid, path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (f) {
        fscanf(f, "%lu %lu", prev_user_time, prev_sys_time);
        fclose(f);
    } else {
        *prev_user_time = 0;
        *prev_sys_time = 0;
    }
}

void write_prev_times(pid_t pid, unsigned long prev_user_time, unsigned long prev_sys_time) {
    char path[64];
    get_stat_file_path(pid, path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%lu %lu\n", prev_user_time, prev_sys_time);
        fclose(f);
    }
}

