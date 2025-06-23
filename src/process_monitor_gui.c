/**
 * @file monitoring_example.c
 * @brief Ejemplo de uso del sistema de monitoreo con hilos
 * 
 * Este archivo muestra cómo usar el sistema de monitoreo de procesos
 * con hilos de manera segura e integrable con una GUI.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "process_monitor.h"

// Variable global para controlar la ejecución
static volatile int running = 1;

// Manejador de señales para terminar limpiamente
void signal_handler(int sig) {
    (void)sig;
    printf("\n[INFO] Señal recibida, terminando...\n");
    running = 0;
}

// Callbacks para eventos de procesos
void on_new_process_callback(ProcessInfo *info) {
    printf("🆕 [GUI EVENT] Nuevo proceso detectado: %s (PID: %d)\n", 
           info->name, info->pid);
    // Aquí la GUI podría actualizar la lista de procesos
}

void on_process_terminated_callback(pid_t pid, const char *name) {
    printf("❌ [GUI EVENT] Proceso terminado: %s (PID: %d)\n", name, pid);
    // Aquí la GUI podría remover el proceso de la lista
}

void on_high_cpu_alert_callback(ProcessInfo *info) {
    printf("🔥 [GUI ALERT] Alto uso de CPU: %s (PID: %d) - %.2f%%\n", 
           info->name, info->pid, info->cpu_usage);
    // Aquí la GUI podría mostrar una notificación de alerta
}

void on_high_memory_alert_callback(ProcessInfo *info) {
    printf("💾 [GUI ALERT] Alto uso de memoria: %s (PID: %d) - %.2f%%\n", 
           info->name, info->pid, info->mem_usage);
    // Aquí la GUI podría mostrar una notificación de alerta
}

void on_alert_cleared_callback(ProcessInfo *info) {
    printf("✅ [GUI EVENT] Alerta despejada: %s (PID: %d)\n", 
           info->name, info->pid);
    // Aquí la GUI podría limpiar las notificaciones
}

// Función para mostrar estadísticas periódicamente
void show_periodic_stats() {
    MonitoringStats stats = get_monitoring_stats();
    
    printf("\n📊 === ESTADÍSTICAS ===\n");
    printf("   Estado: %s\n", stats.is_active ? "ACTIVO" : "INACTIVO");
    printf("   Procesos monitoreados: %d\n", stats.total_processes);
    printf("   Alertas de CPU: %d\n", stats.high_cpu_count);
    printf("   Alertas de memoria: %d\n", stats.high_memory_count);
    printf("   Alertas activas: %d\n", stats.active_alerts);
    printf("   Intervalo: %d segundos\n", stats.check_interval);
    printf("========================\n\n");
}

// Función para obtener y mostrar lista de procesos
void show_process_list() {
    int count;
    ProcessInfo *processes = get_process_list_copy(&count);
    
    if (processes == NULL || count == 0) {
        printf("📝 No hay procesos monitoreados\n\n");
        return;
    }
    
    printf("📝 === LISTA DE PROCESOS ===\n");
    for (int i = 0; i < count; i++) {
        ProcessInfo *p = &processes[i];
        printf("   PID: %d | %s | CPU: %.2f%% | MEM: %.2f%%\n",
               p->pid, p->name, p->cpu_usage, p->mem_usage);
    }
    printf("============================\n\n");
    
    free(processes);
}

int main() {
    printf("🚀 === EJEMPLO DE MONITOREO CON HILOS ===\n\n");
    
    // Configurar manejador de señales
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Cargar configuración
    load_config();
    printf("⚙️  Configuración cargada:\n");
    printf("   - Umbral CPU: %.2f%%\n", config.max_cpu_usage);
    printf("   - Umbral RAM: %.2f%%\n", config.max_ram_usage);
    printf("   - Intervalo: %d segundos\n", config.check_interval);
    printf("   - Duración alerta: %d segundos\n\n", config.alert_duration);
    
    // Configurar callbacks
    ProcessCallbacks callbacks = {
        .on_new_process = on_new_process_callback,
        .on_process_terminated = on_process_terminated_callback,
        .on_high_cpu_alert = on_high_cpu_alert_callback,
        .on_high_memory_alert = on_high_memory_alert_callback,
        .on_alert_cleared = on_alert_cleared_callback
    };
    set_process_callbacks(&callbacks);
    
    // Iniciar monitoreo
    printf("▶️  Iniciando monitoreo...\n");
    if (start_monitoring() != 0) {
        fprintf(stderr, "❌ Error al iniciar el monitoreo\n");
        return 1;
    }
    
    // Bucle principal (simula la GUI)
    int stats_counter = 0;
    while (running && is_monitoring_active()) {
        sleep(5); // Simula el ciclo de la GUI
        
        // Mostrar estadísticas cada 20 segundos
        stats_counter += 5;
        if (stats_counter >= 20) {
            show_periodic_stats();
            show_process_list();
            stats_counter = 0;
        }
        
        // Ejemplo: cambiar intervalo dinámicamente
        static int interval_changed = 0;
        if (!interval_changed && stats_counter == 10) {
            printf("🔄 Cambiando intervalo a 10 segundos...\n");
            set_monitoring_interval(10);
            interval_changed = 1;
        }
    }
    
    // Limpiar y terminar
    printf("\n🛑 Deteniendo monitoreo...\n");
    stop_monitoring();
    cleanup_monitoring();
    
    printf("✅ Programa terminado limpiamente\n");
    return 0;
}
