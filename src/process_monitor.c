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


// Variables para control de hilos de monitoreo
static pthread_t monitoring_thread;
static volatile int monitoring_active = 0;
static volatile int should_stop = 0;

// Callbacks opcionales para eventos
static ProcessCallbacks *event_callbacks = NULL;


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


// ===== FUNCIÓN PRINCIPAL PARA OBTENER INFORMACIÓN DE PROCESO =====

ProcessInfo* get_process_info(pid_t pid) {
    // Verificar que el proceso existe
    if (!process_exists(pid)) {
        return NULL;
    }
    
    ProcessInfo *info = malloc(sizeof(ProcessInfo));
    if (!info) {
        fprintf(stderr, "[ERROR] No se pudo asignar memoria para ProcessInfo\n");
        return NULL;
    }
    
    // Inicializar estructura
    memset(info, 0, sizeof(ProcessInfo));
    info->pid = pid;
    
    // Obtener nombre del proceso
    get_process_name(pid, info->name, sizeof(info->name));
    
    // Si no se pudo obtener el nombre, el proceso probablemente terminó
    if (strlen(info->name) == 0 || strstr(info->name, "unknown_")) {
        free(info);
        return NULL;
    }
    
    // Obtener uso de CPU usando función existente
    info->cpu_usage = interval_cpu_usage(pid);
    
    // Obtener uso de memoria
    info->mem_usage = get_process_memory_usage(pid);
    
    // Inicializar campos de alerta
    info->alerta_activa = 0;
    info->inicio_alerta = 0;
    info->cpu_time = 0.0; // Por ahora, se puede implementar después
    
    return info;
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
                // Callback para proceso nuevo
                if (event_callbacks && event_callbacks->on_new_process) {
                    event_callbacks->on_new_process(info_ptr);
                }
            } else {
                // Proceso existente - actualizar información
                update_process(*info_ptr, idx);
            }
            
            // Verificar si el proceso excede umbrales y generar alertas con callbacks
            if (info_ptr->cpu_usage > MAX_CPU_USAGE) {
                printf("[ALERTA CPU] PID: %d, Nombre: %s, CPU: %.2f%%\n",
                       pid, info_ptr->name, info_ptr->cpu_usage);
                if (event_callbacks && event_callbacks->on_high_cpu_alert) {
                    event_callbacks->on_high_cpu_alert(info_ptr);
                }
            }
            
            if (info_ptr->mem_usage > MAX_MEM_USAGE) {
                printf("[ALERTA MEM] PID: %d, Nombre: %s, Mem: %.2f%%\n",
                       pid, info_ptr->name, info_ptr->mem_usage);
                if (event_callbacks && event_callbacks->on_high_memory_alert) {
                    event_callbacks->on_high_memory_alert(info_ptr);
                }
            }
            
            free(info_ptr);
        }
    }
    closedir(dir);

    // 3. Eliminar procesos que no fueron encontrados (terminados)
    for (int i = num_procesos_activos - 1; i >= 0; i--) {
        if (!procesos_activos[i].encontrado) {
            pid_t terminated_pid = procesos_activos[i].info.pid;
            char terminated_name[256];
            strncpy(terminated_name, procesos_activos[i].info.name, sizeof(terminated_name) - 1);
            terminated_name[255] = '\0';
            
            // Callback para proceso terminado
            if (event_callbacks && event_callbacks->on_process_terminated) {
                event_callbacks->on_process_terminated(terminated_pid, terminated_name);
            }
            
            remove_process(terminated_pid);
        }
    }
    
    // 4. Mostrar estadísticas opcionales    show_process_stats();
}

// ===== FUNCIONES DE MANEJO DE HILOS PARA MONITOREO =====
// Función para establecer callbacks de eventos
void set_process_callbacks(ProcessCallbacks *callbacks) {
    pthread_mutex_lock(&mutex);
    event_callbacks = callbacks;
    pthread_mutex_unlock(&mutex);
}

// Función del hilo de monitoreo
void* monitoring_thread_function(void* arg) {
    (void)arg; // Evitar warning de parámetro no usado
    
    while (!should_stop) {
        // Ejecutar ciclo de monitoreo
        pthread_mutex_lock(&mutex);
        monitor_processes();
        pthread_mutex_unlock(&mutex);
        
        // Esperar el intervalo configurado
        for (int i = 0; i < config.check_interval && !should_stop; i++) {
            sleep(1);
        }
    }
    
    monitoring_active = 0;
    pthread_exit(NULL);
    return NULL;
}

// Iniciar el monitoreo en hilo separado
int start_monitoring() {
    pthread_mutex_lock(&mutex);
    
    if (monitoring_active) {
        pthread_mutex_unlock(&mutex);
        printf("[INFO] El monitoreo ya está activo\n");
        return 1; // Ya está activo
    }
    
    should_stop = 0;
    monitoring_active = 1;
    
    int result = pthread_create(&monitoring_thread, NULL, monitoring_thread_function, NULL);
    if (result != 0) {
        monitoring_active = 0;
        pthread_mutex_unlock(&mutex);
        fprintf(stderr, "[ERROR] No se pudo crear el hilo de monitoreo: %d\n", result);
        return -1;
    }
    
    pthread_mutex_unlock(&mutex);
    printf("[INFO] Monitoreo iniciado con intervalo de %d segundos\n", config.check_interval);
    return 0;
}

// Detener el monitoreo
int stop_monitoring() {
    pthread_mutex_lock(&mutex);
    
    if (!monitoring_active) {
        pthread_mutex_unlock(&mutex);
        printf("[INFO] El monitoreo no está activo\n");
        return 1; // No estaba activo
    }
    
    should_stop = 1;
    pthread_mutex_unlock(&mutex);
    
    // Esperar a que termine el hilo
    int result = pthread_join(monitoring_thread, NULL);
    if (result != 0) {
        fprintf(stderr, "[ERROR] Error al esperar el hilo de monitoreo: %d\n", result);
        return -1;
    }
    
    printf("[INFO] Monitoreo detenido\n");
    return 0;
}

// Verificar si el monitoreo está activo
int is_monitoring_active() {
    pthread_mutex_lock(&mutex);
    int active = monitoring_active;
    pthread_mutex_unlock(&mutex);
    return active;
}

// Cambiar intervalo de monitoreo dinámicamente
void set_monitoring_interval(int seconds) {
    if (seconds < 1) seconds = 1;
    if (seconds > 3600) seconds = 3600; // Max 1 hora
    
    pthread_mutex_lock(&mutex);
    config.check_interval = seconds;
    pthread_mutex_unlock(&mutex);
    
    printf("[INFO] Intervalo de monitoreo cambiado a %d segundos\n", seconds);
}

// Obtener estadísticas thread-safe
MonitoringStats get_monitoring_stats() {
    MonitoringStats stats = {0};
    
    pthread_mutex_lock(&mutex);
    
    stats.total_processes = num_procesos_activos;
    stats.is_active = monitoring_active;
    stats.check_interval = config.check_interval;
    
    // Contar alertas activas
    for (int i = 0; i < num_procesos_activos; i++) {
        ProcessInfo *p = &procesos_activos[i].info;
        if (p->cpu_usage > config.max_cpu_usage) stats.high_cpu_count++;
        if (p->mem_usage > config.max_ram_usage) stats.high_memory_count++;
        if (p->alerta_activa) stats.active_alerts++;
    }
    
    pthread_mutex_unlock(&mutex);
    
    return stats;
}

// Obtener copia thread-safe de la lista de procesos
ProcessInfo* get_process_list_copy(int *count) {
    pthread_mutex_lock(&mutex);
    
    *count = num_procesos_activos;
    if (num_procesos_activos == 0) {
        pthread_mutex_unlock(&mutex);
        return NULL;
    }
    
    ProcessInfo *copy = malloc(num_procesos_activos * sizeof(ProcessInfo));
    if (copy) {
        for (int i = 0; i < num_procesos_activos; i++) {
            copy[i] = procesos_activos[i].info;
        }
    }
    
    pthread_mutex_unlock(&mutex);
    return copy;
}

// Limpiar recursos al finalizar
void cleanup_monitoring() {
    stop_monitoring();
    
    pthread_mutex_lock(&mutex);
    clear_process_list();
    event_callbacks = NULL;
    pthread_mutex_unlock(&mutex);
    
    pthread_mutex_destroy(&mutex);
    printf("[INFO] Recursos de monitoreo liberados\n");
}