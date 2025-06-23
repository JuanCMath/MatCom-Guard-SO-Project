#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include "process_monitor.h"

static volatile int running = 1;

void signal_handler(int signum) {
    (void)signum;
    printf("\n[INFO] Recibida señal de interrupción. Deteniendo monitoreo...\n");
    running = 0;
}

// Callbacks para demostrar el funcionamiento del RF2
void on_new_process_callback(ProcessInfo *info) {
    printf("📍 [NUEVO PROCESO] PID: %d, Nombre: %s, CPU: %.2f%%, MEM: %.2f%%\n",
           info->pid, info->name, info->cpu_usage, info->mem_usage);
    if (info->is_whitelisted) {
        printf("   ✅ Proceso en whitelist - No se generarán alertas\n");
    }
}

void on_process_terminated_callback(pid_t pid, const char *name) {
    printf("🔴 [PROCESO TERMINADO] PID: %d, Nombre: %s\n", pid, name);
}

void on_high_cpu_alert_callback(ProcessInfo *info) {
    printf("🚨 [ALERTA CPU] PID: %d, Nombre: %s, CPU: %.2f%% (Umbral: %.2f%%)\n",
           info->pid, info->name, info->cpu_usage, config.max_cpu_usage);
}

void on_high_memory_alert_callback(ProcessInfo *info) {
    printf("🚨 [ALERTA MEMORIA] PID: %d, Nombre: %s, MEM: %.2f%% (Umbral: %.2f%%)\n",
           info->pid, info->name, info->mem_usage, config.max_ram_usage);
}

void on_alert_cleared_callback(ProcessInfo *info) {
    printf("✅ [ALERTA DESPEJADA] PID: %d, Nombre: %s volvió a valores normales\n",
           info->pid, info->name);
}

void print_rf2_status() {
    printf("\n=== ESTADO DEL SISTEMA RF2 ===\n");
    printf("Configuración actual:\n");
    printf("  - Umbral CPU: %.2f%%\n", config.max_cpu_usage);
    printf("  - Umbral RAM: %.2f%%\n", config.max_ram_usage);
    printf("  - Intervalo: %d seg\n", config.check_interval);
    printf("  - Duración alerta: %d seg\n", config.alert_duration);
    printf("  - Procesos en whitelist: %d\n", config.num_white_processes);
    
    if (config.num_white_processes > 0) {
        printf("  - Lista blanca: ");
        for (int i = 0; i < config.num_white_processes; i++) {
            printf("%s%s", config.white_list[i], 
                   i < config.num_white_processes - 1 ? ", " : "");
        }
        printf("\n");
    }
    
    MonitoringStats stats = get_monitoring_stats();
    printf("\nEstadísticas de monitoreo:\n");
    printf("  - Procesos activos: %d\n", stats.total_processes);
    printf("  - Procesos con CPU alta: %d\n", stats.high_cpu_count);
    printf("  - Procesos con memoria alta: %d\n", stats.high_memory_count);
    printf("  - Alertas activas: %d\n", stats.active_alerts);
    printf("  - Estado: %s\n", stats.is_active ? "ACTIVO" : "INACTIVO");
    printf("==============================\n\n");
}

int main() {
    printf("=== SISTEMA DE MONITOREO RF2 - MatCom Guard ===\n");
    printf("Implementación completa de los requisitos RF2\n\n");
    
    // Configurar manejador de señales
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Cargar configuración
    load_config();
    
    // Configurar callbacks
    ProcessCallbacks callbacks = {
        .on_new_process = on_new_process_callback,
        .on_process_terminated = on_process_terminated_callback,
        .on_high_cpu_alert = on_high_cpu_alert_callback,
        .on_high_memory_alert = on_high_memory_alert_callback,
        .on_alert_cleared = on_alert_cleared_callback
    };
    
    set_process_callbacks(&callbacks);
    
    // Mostrar configuración inicial
    print_rf2_status();
    
    printf("🚀 Iniciando monitoreo según especificaciones RF2...\n");
    printf("📋 Características implementadas:\n");
    printf("   ✅ Lectura de información desde /proc\n");
    printf("   ✅ Comparación entre iteraciones de monitoreo\n");
    printf("   ✅ Alertas basadas en duración configurable\n");
    printf("   ✅ Fórmula: alerta = (CPU > umbral) ∨ (RAM > umbral) por %d seg\n", config.alert_duration);
    printf("   ✅ Soporte para whitelist\n");
    printf("   ✅ Configuración desde /etc/matcomguard.conf\n");
    printf("   ✅ Sistema de callbacks thread-safe\n\n");
    
    // Iniciar monitoreo
    if (start_monitoring() != 0) {
        fprintf(stderr, "Error al iniciar el monitoreo\n");
        return 1;
    }
    
    printf("💡 El sistema está monitoreando...\n");
    printf("💡 Presiona Ctrl+C para detener\n");
    printf("💡 Las alertas se activarán después de %d segundos de exceder umbrales\n\n", config.alert_duration);
    
    // Bucle principal con estadísticas periódicas
    int cycle = 0;
    while (running) {
        sleep(15); // Estadísticas cada 15 segundos
        if (running) {
            cycle++;
            printf("\n--- Ciclo %d (cada 15 seg) ---\n", cycle);
            print_rf2_status();
        }
    }
    
    // Limpiar recursos
    printf("\n🛑 Deteniendo sistema de monitoreo...\n");
    cleanup_monitoring();
    
    printf("✅ Sistema RF2 finalizado correctamente\n");
    return 0;
}
