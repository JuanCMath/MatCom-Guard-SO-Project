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


// Variables globales
Config config;
ProcessInfo *procesos = NULL;
int num_procesos = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
FILE *log_file = NULL;

// Variables globales para procesos activos
ActiveProcess *procesos_activos = NULL;
int num_procesos_activos = 0;

#define MAX_CPU_USAGE config.max_cpu_usage  // Umbral para alertas de CPU
#define MAX_MEM_USAGE config.max_ram_usage // Umbral para alertas de memoria


// Función para cargar la configuración
void load_config() {

    // Valores predeterminados
    config.max_cpu_usage = 90.0;
    config.max_ram_usage = 80.0;
    config.check_interval = 30;
    config.alert_duration = 10;
    config.num_white_processes = 0;
    config.white_list = NULL;

    // Cargar desde archivo
    FILE *conf = fopen(CONFIG_PATH, "r");
    if (!conf) return;

    char line[256];
    while (fgets(line, sizeof(line), conf)) {

        //Actualiza el umbral para alertas de uso de CPU
        if (strstr(line, "UMBRAL_CPU=")) {
            sscanf(line, "UMBRAL_CPU=%f", &config.max_cpu_usage);
        } 

        //Actualiza el umbral para alertas de uso de RAM
        else if (strstr(line, "UMBRAL_RAM=")) {
            sscanf(line, "UMBRAL_RAM=%f", &config.max_ram_usage);
        } 

        //Actualiza el intervalo de realizacion de chequeos
        else if (strstr(line, "INTERVALO=")) {
            sscanf(line, "INTERVALO=%d", &config.check_interval);
        } 

        //Actualiza la duracion del estado de alerta
        else if (strstr(line, "DURACION_ALERTA=")) {
            sscanf(line, "DURACION_ALERTA=%d", &config.alert_duration);
        } 

        //Actualiza lo procesos a tener en cuenta en la lista blanca
        else if (strstr(line, "WHITELIST=")) {
            char *list = strchr(line, '=') + 1;
            list[strcspn(list, "\n")] = 0;
            
            // Tokenizar la lista de procesos
            char *token = strtok(list, ",");
            while (token) {
                config.white_list = realloc(config.white_list, 
                                            (config.num_white_processes + 1) * sizeof(char*));
                config.white_list[config.num_white_processes] = strdup(token);
                config.num_white_processes++;
                token = strtok(NULL, ",");
            }
        }
    }

    fclose(conf);
}


// Función para monitorear procesos
void monitor_processes() {
    DIR *dir = opendir("/proc");
    if (!dir) {
        perror("Error al abrir /proc");
        return;
    }

    // 1. Marcar todos los procesos actuales como "no encontrados"
    for (int i = 0; i < num_procesos_activos; i++) {
        procesos_activos[i].encontrado = 0;
    }

    // 2. Recorrer /proc y procesar cada PID
    struct dirent *entry;
    printf("=== CICLO DE MONITOREO ===\n");
    
    while ((entry = readdir(dir)) != NULL) {
        // Verificar si el nombre del directorio es un número (PID)
        char *endptr;
        long lpid = strtol(entry->d_name, &endptr, 10);
        if (*endptr == '\0') {
            pid_t pid = (pid_t)lpid;
            
            // Obtener información del proceso
            ProcessInfo *info_ptr = get_process_info(pid);
            if (!info_ptr || strlen(info_ptr->name) == 0) {
                if (info_ptr) free(info_ptr);
                continue;
            }
            
            int idx = find_process(pid);
            if (idx == -1) {
                // Proceso nuevo - agregarlo
                add_process(*info_ptr);
            } else {
                // Proceso existente - actualizar información
                update_process(*info_ptr, idx);
            }
            
            // Verificar si el proceso excede umbrales y generar alertas
            if (info_ptr->cpu_usage > MAX_CPU_USAGE || info_ptr->mem_usage > MAX_MEM_USAGE) {
                printf("[ALERTA] PID: %d, Nombre: %s, CPU: %.2f%%, Mem: %.2f%%\n",
                       pid, info_ptr->name, info_ptr->cpu_usage, info_ptr->mem_usage);
            }
            
            free(info_ptr);
        }
    }
    closedir(dir);

    // 3. Eliminar procesos que no fueron encontrados (terminados)
    for (int i = num_procesos_activos - 1; i >= 0; i--) {
        if (!procesos_activos[i].encontrado) {
            remove_process(procesos_activos[i].info.pid);
        }
    }
    
    // 4. Mostrar estadísticas opcionales
    show_process_stats();
}


// Cálculo de uso total de CPU
float total_cpu_usage(pid_t pid, unsigned long *prev_user_time, unsigned long *prev_sys_time) {

    ProcStat stat;
    if (read_proc_stat(pid, &stat) != 0) return 0.0;

    // Inicializar los tiempos previos si es la primera vez
    if (*prev_sys_time == 0 && *prev_user_time == 0) {
        *prev_sys_time = stat.stime;
        *prev_user_time = stat.utime;
        return 0.0;
    } else {
        *prev_sys_time = stat.stime;
        *prev_user_time = stat.utime;
    }
    
    FILE *uptime_fp = fopen("/proc/uptime", "r");
    if (!uptime_fp) return 0.0;
    double uptime = 0.0;
    fscanf(uptime_fp, "%lf", &uptime);
    fclose(uptime_fp);

    long clk_tck = sysconf(_SC_CLK_TCK);
    double seconds = uptime - (stat.starttime / (double)clk_tck);
    if (seconds <= 0) return 0.0;
    
    return (float)(100.0 * ((stat.utime + stat.stime) / ((double)clk_tck * seconds)));
}

// Calculo de uso del CPU en el ultimo intervalo de tiempo
float interval_cpu_usage(pid_t pid) {
    ProcStat stat;
    if (read_proc_stat(pid, &stat) != 0) return 0.0;

    unsigned long prev_user_time = 0, prev_sys_time = 0;
    read_prev_times(pid, &prev_user_time, &prev_sys_time);

    unsigned long delta_user = stat.utime - prev_user_time;
    unsigned long delta_sys = stat.stime - prev_sys_time;
    unsigned long delta_total = delta_user + delta_sys;
    long clk_tck = sysconf(_SC_CLK_TCK);

    write_prev_times(pid, stat.utime, stat.stime);

    double cpu_percent = 100.0 * (delta_total / ((double)clk_tck * config.check_interval));    
    return (float)(100.0 * (delta_total / ((double)clk_tck * config.check_interval)));
}