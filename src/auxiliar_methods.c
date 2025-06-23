/**
 * @file auxiliar_methods.c
 * @brief Funciones auxiliares para el sistema de monitoreo de procesos
 * 
 * Este archivo contiene todas las funciones auxiliares organizadas por categorías:
 * - Lectura y acceso a /proc
 * - Información de procesos (nombre, memoria, existencia)
 * - Cálculo de uso de CPU
 * - Gestión de lista de procesos activos
 * - Persistencia de datos de procesos
 */

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

// Variables globales externas
extern ActiveProcess *procesos_activos;
extern int num_procesos_activos;
extern Config config;

// Variables globales externas
extern ActiveProcess *procesos_activos;
extern int num_procesos_activos;
extern Config config;

// ===== FUNCIONES DE ACCESO A /proc =====

/**
 * @brief Lee los campos relevantes de /proc/[pid]/stat
 * @param pid PID del proceso
 * @param stat Estructura donde almacenar los datos
 * @return 0 si éxito, -1 si error
 */
int read_proc_stat(pid_t pid, ProcStat *stat) {
    char stat_path[256];
    sprintf(stat_path, "/proc/%d/stat", pid);
    FILE *fp = fopen(stat_path, "r");
    if (!fp) return -1;    
    
    // Leer los campos 14 (utime), 15 (stime), 22 (starttime)
    int scanned = fscanf(fp,
        "%*d "      // pid (campo 1)
        "%*s "      // comm (campo 2)
        "%*c "      // state (campo 3)
        "%*d %*d %*d %*d %*d "  // campos 4-8
        "%*u %*u %*u %*u %*u "  // campos 9-13
        "%lu %lu %*ld %*ld %*ld %*ld %*ld %*ld %lu",  // campos 14, 15, 16-21, 22
        &stat->utime, &stat->stime, &stat->starttime);

    fclose(fp);
    return (scanned == 3) ? 0 : -1;
}

/**
 * @brief Obtiene la memoria total del sistema en kB
 * @return Memoria total en kB, 0 si error
 */
unsigned long get_total_system_memory() {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) return 0;
    
    char line[256];
    unsigned long total_mem = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "MemTotal:")) {
            sscanf(line, "MemTotal: %lu kB", &total_mem);
            break;
        }
    }
    fclose(fp);
    return total_mem;
}

// ===== FUNCIONES DE INFORMACIÓN DE PROCESOS =====

/**
 * @brief Verifica si un proceso existe
 * @param pid PID del proceso
 * @return 1 si existe, 0 si no
 */
int process_exists(pid_t pid) {
    char stat_path[256];
    sprintf(stat_path, "/proc/%d/stat", pid);
    FILE *fp = fopen(stat_path, "r");
    if (fp) {
        fclose(fp);
        return 1;
    }
    return 0;
}

/**
 * @brief Obtiene el nombre del proceso
 * @param pid PID del proceso
 * @param name Buffer donde almacenar el nombre
 * @param size Tamaño del buffer
 */
void get_process_name(pid_t pid, char *name, size_t size) {
    // Intentar primero /proc/[pid]/comm (más confiable)
    char comm_path[256];
    sprintf(comm_path, "/proc/%d/comm", pid);
    FILE *fp = fopen(comm_path, "r");
    
    if (fp) {
        if (fgets(name, size, fp)) {
            // Remover \n al final
            name[strcspn(name, "\n")] = '\0';
            fclose(fp);
            return;
        }
        fclose(fp);
    }
    
    // Si comm falla, intentar desde /proc/[pid]/stat
    char stat_path[256];
    sprintf(stat_path, "/proc/%d/stat", pid);
    fp = fopen(stat_path, "r");
    
    if (fp) {
        char comm_field[256];
        if (fscanf(fp, "%*d %255s", comm_field) == 1) {
            // Remover paréntesis del nombre
            if (comm_field[0] == '(' && comm_field[strlen(comm_field)-1] == ')') {
                comm_field[strlen(comm_field)-1] = '\0';
                strncpy(name, comm_field + 1, size - 1);
                name[size - 1] = '\0';
            } else {
                strncpy(name, comm_field, size - 1);
                name[size - 1] = '\0';
            }
            fclose(fp);
            return;
        }
        fclose(fp);
    }
    
    // Si todo falla, nombre por defecto
    snprintf(name, size, "unknown_%d", pid);
}

// ===== FUNCIONES DE PERSISTENCIA DE DATOS =====

/**
 * @brief Genera la ruta del archivo de estadísticas de un proceso
 * @param pid PID del proceso
 * @param path Buffer donde almacenar la ruta
 * @param size Tamaño del buffer
 */
void get_stat_file_path(pid_t pid, char *path, size_t size) {
    snprintf(path, size, "/tmp/procstat_%d.dat", pid);
}

/**
 * @brief Lee los tiempos previos de CPU de un proceso
 * @param pid PID del proceso
 * @param prev_user_time Puntero donde almacenar tiempo de usuario previo
 * @param prev_sys_time Puntero donde almacenar tiempo de sistema previo
 */
void read_prev_times(pid_t pid, unsigned long *prev_user_time, unsigned long *prev_sys_time) {
    char path[64];
    get_stat_file_path(pid, path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (f) {
        int scanned = fscanf(f, "%lu %lu", prev_user_time, prev_sys_time);
        fclose(f);
        
        // Si no se pudieron leer ambos valores, inicializar a 0
        if (scanned != 2) {
            *prev_user_time = 0;
            *prev_sys_time = 0;
        }
    } else {
        *prev_user_time = 0;
        *prev_sys_time = 0;
    }
}

/**
 * @brief Escribe los tiempos previos de CPU de un proceso
 * @param pid PID del proceso
 * @param prev_user_time Tiempo de usuario a guardar
 * @param prev_sys_time Tiempo de sistema a guardar
 */
void write_prev_times(pid_t pid, unsigned long prev_user_time, unsigned long prev_sys_time) {
    char path[64];
    get_stat_file_path(pid, path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%lu %lu\n", prev_user_time, prev_sys_time);
        fclose(f);
    }
}

// ===== FUNCIONES DE CÁLCULO DE CPU Y MEMORIA =====

/**
 * @brief Calcula el uso total de CPU de un proceso desde su inicio
 * @param pid PID del proceso
 * @param prev_user_time Puntero a tiempo de usuario previo
 * @param prev_sys_time Puntero a tiempo de sistema previo
 * @return Porcentaje de uso de CPU
 */
float total_cpu_usage(pid_t pid) {
    ProcStat stat;
    if (read_proc_stat(pid, &stat) != 0) return 0.0;
    
    // Obtener uptime del sistema en segundos
    FILE *uptime_fp = fopen("/proc/uptime", "r");
    if (!uptime_fp) return 0.0;
    double system_uptime_seconds = 0.0;
    fscanf(uptime_fp, "%lf", &system_uptime_seconds);
    fclose(uptime_fp);

    long clk_tck = sysconf(_SC_CLK_TCK);
    
    // Calcular tiempo que el proceso ha estado vivo en segundos
    double process_start_time_seconds = stat.starttime / (double)clk_tck;
    double process_uptime_seconds = system_uptime_seconds - process_start_time_seconds;
    
    if (process_uptime_seconds <= 0) return 0.0;
    
    // Convertir tiempo de CPU usado a segundos
    double cpu_time_seconds = (stat.utime + stat.stime) / (double)clk_tck;
    
    // Calcular porcentaje: (tiempo_cpu_usado / tiempo_vida_proceso) * 100
    // NOTA: En sistemas multi-core, esto puede ser > 100% para procesos multihilo
    return (float)(100.0 * (cpu_time_seconds / process_uptime_seconds));
}

/**
 * @brief Calcula el uso de CPU de un proceso en el último intervalo
 * @param pid PID del proceso
 * @return Porcentaje de uso de CPU en el intervalo
 */
float interval_cpu_usage(pid_t pid) {
    ProcStat stat;
    if (read_proc_stat(pid, &stat) != 0) return 0.0;

    unsigned long prev_user_time = 0, prev_sys_time = 0;
    read_prev_times(pid, &prev_user_time, &prev_sys_time);

    // Inicializar y Verificar consistencia de datos (proceso reiniciado, overflow, etc.)
    if ((prev_user_time == 0 && prev_sys_time == 0) ||               // Si es la primera medición (no hay datos previos)
        (stat.utime < prev_user_time || stat.stime <  prev_sys_time) // Proceso posiblemente reiniciado o datos inconsistentes
    )
    {
        write_prev_times(pid, stat.utime, stat.stime);
        return total_cpu_usage(pid);  // No hay delta válido, calcula el uso del cpu desde que comenzo el proceso.
    }    
    
    unsigned long delta_user = stat.utime - prev_user_time;
    unsigned long delta_sys = stat.stime - prev_sys_time;
    unsigned long delta_total = delta_user + delta_sys;
    long clk_tck = sysconf(_SC_CLK_TCK);

    // Guardar tiempos actuales para próxima medición
    write_prev_times(pid, stat.utime, stat.stime);

    // Calcular porcentaje de CPU
    // IMPORTANTE: En sistemas multi-core, >100% es NORMAL para procesos multihilo
    // Un proceso con N hilos puede usar hasta N*100% CPU
    double cpu_percentage = 100.0 * (delta_total / ((double)clk_tck * config.check_interval));
    
    // Detectar valores extremadamente altos que indican problemas de datos
    long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    double max_theoretical_cpu = num_cores * 100.0;
    
    if (cpu_percentage > max_theoretical_cpu) {
        // Valor superior al 100% del máximo teórico, probablemente error
        fprintf(stderr, "[WARNING] PID %d: CPU calculation suspicious: %.2f%% "
                "(max theoretical: %.2f%% for %ld cores)\n", 
                pid, cpu_percentage, max_theoretical_cpu, num_cores);
        return total_cpu_usage(pid);
    }
    
    return (float)cpu_percentage;
}

/**
 * @brief Obtiene el uso de memoria del proceso en porcentaje
 * @param pid PID del proceso
 * @return Porcentaje de uso de memoria
 */
float get_process_memory_usage(pid_t pid) {
    char status_path[256];
    sprintf(status_path, "/proc/%d/status", pid);
    FILE *fp = fopen(status_path, "r");
    if (!fp) return 0.0;
    
    char line[256];
    unsigned long vmrss = 0; // Memoria física usada
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "VmRSS:")) {
            sscanf(line, "VmRSS: %lu kB", &vmrss);
            break;
        }
    }
    fclose(fp);
    
    if (vmrss == 0) return 0.0;
    
    unsigned long total_mem = get_total_system_memory();
    if (total_mem == 0) return 0.0;
    
    float mem_percentage = (float)(100.0 * vmrss / total_mem);
    
    // Validación: la memoria nunca debería exceder 100%
    if (mem_percentage > 100.0) {
        return 100.0; // Valor sospechoso, retornar 100 para lanzar la alerta
    }
    
    return mem_percentage;
}

// ===== FUNCIONES DE GESTIÓN DE LISTA DE PROCESOS ACTIVOS =====

/**
 * @brief Busca un proceso por PID en la lista de procesos activos
 * @param pid PID del proceso
 * @return Índice en la lista, -1 si no se encuentra
 */
/**
 * @brief Busca un proceso por PID en la lista de procesos activos
 * @param pid PID del proceso
 * @return Índice en la lista, -1 si no se encuentra
 */
int find_process(pid_t pid) {
    for (int i = 0; i < num_procesos_activos; i++) {
        if (procesos_activos[i].info.pid == pid) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Agrega un proceso nuevo al array dinámico de procesos activos
 * @param info Información del proceso a agregar
 */
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

/**
 * @brief Elimina un proceso de la lista de procesos activos
 * @param pid PID del proceso a eliminar
 */
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

/**
 * @brief Actualiza la información de un proceso existente en la lista
 * @param info Nueva información del proceso
 * @param idx Índice del proceso en la lista
 */
void update_process(ProcessInfo info, int idx) {
    if (idx < 0 || idx >= num_procesos_activos) return;
    
    procesos_activos[idx].info = info;
    procesos_activos[idx].encontrado = 1;
}

/**
 * @brief Limpia toda la lista de procesos activos
 */
void clear_process_list() {
    if (procesos_activos != NULL) {
        free(procesos_activos);
        procesos_activos = NULL;
    }
    num_procesos_activos = 0;
    printf("[INFO] Lista de procesos activos limpiada\n");
}

/**
 * @brief Muestra estadísticas de los procesos activos
 */
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

/**
 * @brief Limpia archivos temporales de estadísticas de procesos
 * Se ejecuta al finalizar el monitoreo para evitar datos corruptos
 */
void cleanup_temp_files() {
    printf("[INFO] Limpiando archivos temporales de estadísticas...\n");
    
    // Eliminar archivos temporales del directorio /tmp
    int result = system("rm -f /tmp/procstat_*.dat 2>/dev/null");
    if (result == 0) {
        printf("[INFO] Archivos temporales limpiados exitosamente\n");
    } else {
        printf("[WARNING] No se pudieron limpiar algunos archivos temporales\n");
    }
}

/**
 * @brief Limpia archivos temporales antiguos (más de 1 día)
 * Útil para mantenimiento automático del sistema
 */
void cleanup_stale_temp_files() {
    printf("[INFO] Limpiando archivos temporales antiguos...\n");

    if (system("find /tmp -name 'procstat_*.dat' -mtime +1 -delete 2>/dev/null") == 0) {
        printf("[INFO] Archivos temporales limpiados exitosamente\n");
    } else {
        printf("[WARNING] No se pudieron limpiar algunos archivos temporales viejos\n");
    }
}

// ===== FIN MÉTODOS AUXILIARES PARA MANEJO DE PROCESOS ACTIVOS=====

