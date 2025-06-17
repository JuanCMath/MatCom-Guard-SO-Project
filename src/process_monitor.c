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


#define MAX_CPU_USAGE config.max_cpu_usage  // Umbral para alertas de CPU
#define MAX_MEM_USAGE config.max_ram_usage // Umbral para alertas de memoria


// Función para monitorear procesos
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

            if (strlen(info.name) > 0 && (info.cpu_usage > MAX_CPU_USAGE || info.mem_usage > MAX_MEM_USAGE)) {
                printf("[ALERTA] PID: %d, Nombre: %s, CPU: %.2f%%, Mem: %.2f%%\n",
                       pid, info.name, info.cpu_usage, info.mem_usage);
            }
        }
    }
    closedir(dir);
}


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


// Cálculo de uso de CPU
float cpu_usage(pid_t pid, unsigned long *prev_utime, unsigned long *prev_stime) {
    char stat_path[256];
    sprintf(stat_path, "/proc/%d/stat", pid);
    FILE *fp = fopen(stat_path, "r");
    if (!fp) return 0.0;

    unsigned long utime, stime;
    fscanf(fp, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", 
           &utime, &stime);
    fclose(fp);

    // Calcular diferencia
    unsigned long delta_utime = utime - *prev_utime;
    unsigned long delta_stime = stime - *prev_stime;
    *prev_utime = utime;
    *prev_stime = stime;

    // Obtener tiempo total del sistema
    struct sysinfo si;
    sysinfo(&si);
    unsigned long total_time = si.uptime * sysconf(_SC_CLK_TCK);

    return (delta_utime + delta_stime) * 100.0 / total_time;
}