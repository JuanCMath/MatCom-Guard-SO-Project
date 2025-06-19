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

// Funciones auxiliares para información de procesos
unsigned long get_total_system_memory();
float get_process_memory_usage(pid_t pid);
void get_process_name(pid_t pid, char *name, size_t size);
int process_exists(pid_t pid);

// Variables globales externas
extern ActiveProcess *procesos_activos;
extern int num_procesos_activos;
extern Config config;

// Estructura para callbacks de eventos 
//(nota de desarrollador: son como los delegados en c#, pueden ser pasados a funciones para que se ejecuten)
typedef struct {
    /*
    Cuándo: Cuando find_process(pid) retorna -1 (no existe en la lista) 
    Dónde: En monitor_processes() → bloque de proceso nuevo 
    Propósito: Notificar que hay un proceso nuevo para agregar a la GUI
    */
    void (*on_new_process)(ProcessInfo *info);

    /*
    Cuándo: Cuando un proceso estaba en la lista pero ya no está en proc 
    Dónde: En el bucle de eliminación de procesos terminados 
    Propósito: Notificar que un proceso terminó para removerlo de la GUI
    */
    void (*on_process_terminated)(pid_t pid, const char *name);

    /*
    Cuándo: Cuando info->cpu_usage > config.max_cpu_usage 
    Dónde: En la verificación de umbrales 
    Propósito: Mostrar alerta visual/sonora de alto uso de CPU
    */
    void (*on_high_cpu_alert)(ProcessInfo *info);

    /*
    Cuándo: Cuando info->mem_usage > config.max_ram_usage 
    Dónde: En la verificación de umbrales 
    Propósito: Mostrar alerta visual/sonora de alto uso de memoria
    */
    void (*on_high_memory_alert)(ProcessInfo *info);

    /*
    Cuándo: Cuando una alerta que estaba activa se despeja 
    Dónde: En la gestión de duración de alertas
    Propósito: Limpiar alertas visuales cuando el proceso vuelve a valores normales
    */
    void (*on_alert_cleared)(ProcessInfo *info);
    //   ^       ^              ^
    //   |       |              |
    //   |       |              └─ Parámetros que recibe
    //   |       └─ Nombre del callback
    //   └─ Tipo de retorno

} ProcessCallbacks;
// La misma función de monitoreo puede trabajar con:
// - GUI GTK
// - GUI Qt
// - Aplicación de consola
// - Web API
// - Sistema de logs
// Solo cambiando los callbacks


// Estructura para estadísticas de monitoreo
typedef struct {
    int total_processes;
    int high_cpu_count;
    int high_memory_count;
    int active_alerts;
    int is_active;
    int check_interval;
} MonitoringStats;

// Funciones de control de hilos de monitoreo
int start_monitoring();
int stop_monitoring();
int is_monitoring_active();
void set_monitoring_interval(int seconds);
void set_process_callbacks(ProcessCallbacks *callbacks);

// Funciones thread-safe para acceder a datos
MonitoringStats get_monitoring_stats();
ProcessInfo* get_process_list_copy(int *count);
void cleanup_monitoring();

#endif