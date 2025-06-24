#include "gui_process_integration.h"
#include "gui_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// Declaraciones de funciones internas
static gboolean complete_process_scan_simulation(gpointer user_data);

// ============================================================================
// VARIABLES DE ESTADO DE LA INTEGRACIÓN
// ============================================================================

// Esta estructura mantiene el estado de sincronización entre GUI y backend
static struct {
    int initialized;           // ¿Está inicializada la integración?
    int monitoring_requested;  // ¿La GUI ha solicitado monitoreo?
    int last_process_count;    // Último conteo de procesos para detectar cambios
    pthread_mutex_t state_mutex; // Protege el acceso concurrente al estado
} integration_state = { 0, 0, 0, PTHREAD_MUTEX_INITIALIZER };

// Callbacks configurados para conectar con el backend
static ProcessCallbacks backend_callbacks;

// ============================================================================
// GESTIÓN DEL CICLO DE VIDA DEL MONITOR DE PROCESOS
// ============================================================================

int init_process_integration(void) {
    pthread_mutex_lock(&integration_state.state_mutex);
    
    if (integration_state.initialized) {
        pthread_mutex_unlock(&integration_state.state_mutex);
        return 0; // Ya está inicializado
    }
    
    // Paso 1: Cargar configuración del backend
    // Esta llamada lee el archivo /etc/matcomguard.conf y establece
    // los umbrales y parámetros de monitoreo
    load_config();
    
    // Paso 2: Configurar callbacks que conectan eventos del backend con la GUI
    // Estos callbacks actúan como "traductores" que convierten eventos
    // orientados a datos del backend en actualizaciones de la interfaz gráfica
    backend_callbacks.on_new_process = on_gui_process_new;
    backend_callbacks.on_process_terminated = on_gui_process_terminated;
    backend_callbacks.on_high_cpu_alert = on_gui_high_cpu_alert;
    backend_callbacks.on_high_memory_alert = on_gui_high_memory_alert;
    backend_callbacks.on_alert_cleared = on_gui_alert_cleared;
    
    // Paso 3: Registrar los callbacks en el backend
    // Esta llamada le dice al sistema de monitoreo qué funciones debe
    // llamar cuando detecte eventos específicos
    set_process_callbacks(&backend_callbacks);
    
    integration_state.initialized = 1;
    integration_state.last_process_count = 0;
    
    pthread_mutex_unlock(&integration_state.state_mutex);
    
    // Registrar mensaje de inicialización en la GUI
    gui_add_log_entry("PROCESS_INTEGRATION", "INFO", 
                     "Integración de monitoreo de procesos inicializada");
    
    return 0;
}

int start_process_monitoring(void) {
    pthread_mutex_lock(&integration_state.state_mutex);
    
    // Verificar que la integración esté inicializada
    if (!integration_state.initialized) {
        pthread_mutex_unlock(&integration_state.state_mutex);
        gui_add_log_entry("PROCESS_INTEGRATION", "ERROR", 
                         "Integración no inicializada. Llame a init_process_integration() primero");
        return -1;
    }
    
    integration_state.monitoring_requested = 1;
    pthread_mutex_unlock(&integration_state.state_mutex);
    
    // Verificar si el monitoreo ya está activo en el backend
    if (is_monitoring_active()) {
        gui_add_log_entry("PROCESS_INTEGRATION", "INFO", 
                         "Monitoreo de procesos ya está activo");
        return 1;
    }
    
    // Iniciar el monitoreo en el backend
    // Esta llamada crea un hilo separado que ejecuta el ciclo de monitoreo
    int result = start_monitoring();
    
    if (result == 0) {
        gui_add_log_entry("PROCESS_INTEGRATION", "INFO", 
                         "Monitoreo de procesos iniciado exitosamente");
        
        // Actualizar el estado de la GUI para reflejar que está escaneando
        gui_set_scanning_status(TRUE);
        
        // Realizar una sincronización inicial para poblar la GUI
        // con cualquier proceso que ya esté siendo monitoreado
        sync_gui_with_backend_processes();
        
    } else {
        gui_add_log_entry("PROCESS_INTEGRATION", "ERROR", 
                         "Error al iniciar monitoreo de procesos");
    }
    
    return result;
}

int stop_process_monitoring(void) {
    pthread_mutex_lock(&integration_state.state_mutex);
    integration_state.monitoring_requested = 0;
    pthread_mutex_unlock(&integration_state.state_mutex);
    
    if (!is_monitoring_active()) {
        gui_add_log_entry("PROCESS_INTEGRATION", "INFO", 
                         "Monitoreo de procesos no está activo");
        return 1;
    }
    
    int result = stop_monitoring();
    
    if (result == 0) {
        gui_add_log_entry("PROCESS_INTEGRATION", "INFO", 
                         "Monitoreo de procesos detenido");
        gui_set_scanning_status(FALSE);
    } else {
        gui_add_log_entry("PROCESS_INTEGRATION", "ERROR", 
                         "Error al detener monitoreo de procesos");
    }
    
    return result;
}

int is_process_monitoring_active(void) {
    // Delegamos al backend la respuesta sobre el estado del monitoreo
    return is_monitoring_active();
}

/**
 * @brief Limpia todos los recursos de la integración de procesos
 * 
 * MODIFICACIÓN: Mejorada para realizar limpieza más robusta y evitar
 * colgamientos durante el cierre de la aplicación.
 */
void cleanup_process_integration(void) {
    pthread_mutex_lock(&integration_state.state_mutex);
    
    if (!integration_state.initialized) {
        pthread_mutex_unlock(&integration_state.state_mutex);
        return;
    }
    
    gui_add_log_entry("PROCESS_INTEGRATION", "INFO", 
                     "Iniciando limpieza de integración de procesos...");
    
    // CAMBIO: La función stop_monitoring() ahora tiene timeout de seguridad
    if (is_monitoring_active()) {
        gui_add_log_entry("PROCESS_INTEGRATION", "INFO", 
                         "Deteniendo monitoreo activo...");
        stop_monitoring();
    }
    
    // CAMBIO: La función cleanup_monitoring() fue mejorada con timeouts
    cleanup_monitoring();
    
    integration_state.initialized = 0;
    integration_state.monitoring_requested = 0;
    
    pthread_mutex_unlock(&integration_state.state_mutex);
    
    gui_add_log_entry("PROCESS_INTEGRATION", "INFO", 
                     "✅ Integración de procesos finalizada y recursos liberados");
}

// ============================================================================
// INTERFAZ DE CALLBACKS PARA LA GUI
// ============================================================================

void on_gui_process_new(ProcessInfo *info) {
    if (!info) return;
    
    // Paso 1: Convertir la estructura rica del backend a la estructura simple de la GUI
    GUIProcess gui_process;
    if (adapt_process_info_to_gui(info, &gui_process) != 0) {
        gui_add_log_entry("PROCESS_INTEGRATION", "ERROR", 
                         "Error al adaptar información de proceso nuevo");
        return;
    }
    
    // Paso 2: Actualizar la interfaz gráfica con el nuevo proceso
    gui_update_process(&gui_process);
    
    // Paso 3: Registrar el evento para auditoría
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), 
             "Nuevo proceso detectado: %s (PID: %d) - CPU: %.1f%%, MEM: %.1f%%", 
             info->name, info->pid, info->cpu_usage, info->mem_usage);
    gui_add_log_entry("PROCESS_MONITOR", "INFO", log_msg);
}

void on_gui_process_terminated(pid_t pid, const char *name) {
    // Cuando un proceso termina, necesitamos removerlo de la vista de la GUI
    // Por ahora, registramos el evento. En una implementación más avanzada,
    // podríamos tener una función gui_remove_process()
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), 
             "Proceso terminado: %s (PID: %d)", 
             name ? name : "desconocido", pid);
    gui_add_log_entry("PROCESS_MONITOR", "INFO", log_msg);
    
    // Actualizar estadísticas después de la terminación del proceso
    int total, high_cpu, high_mem;
    if (get_process_statistics_for_gui(&total, &high_cpu, &high_mem) == 0) {
        // Como no tenemos el número de puertos disponible aquí, 
        // usamos el último valor conocido (0 por simplicidad)
        gui_update_statistics(0, total, 0);
    }
}

void on_gui_high_cpu_alert(ProcessInfo *info) {
    if (!info) return;
    
    // ⚠️ VERIFICACIÓN ADICIONAL DE SEGURIDAD: Doble chequeo de whitelist
    if (info->is_whitelisted) {
        char debug_msg[256];
        snprintf(debug_msg, sizeof(debug_msg), 
                 "⚠️ ADVERTENCIA: Intento de alerta para proceso whitelisted '%s' (PID: %d)", 
                 info->name, info->pid);
        gui_add_log_entry("PROCESS_MONITOR", "WARNING", debug_msg);
        return; // NO generar alerta para procesos whitelisted
    }
    
    // Convertir para actualización de GUI
    GUIProcess gui_process;
    if (adapt_process_info_to_gui(info, &gui_process) != 0) {
        return;
    }
    
    // Actualizar la GUI para mostrar el estado de alerta
    gui_update_process(&gui_process);
    
    // Registrar alerta en el log con nivel ALERT para destacarla
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), 
             "� ALERTA CPU: Proceso '%s' (PID: %d) usando %.1f%% de CPU", 
             info->name, info->pid, info->cpu_usage);
    gui_add_log_entry("PROCESS_MONITOR", "ALERT", log_msg);
    
    // Si las notificaciones están habilitadas, podríamos disparar
    // una notificación del sistema aquí usando is_notifications_enabled()
}

void on_gui_high_memory_alert(ProcessInfo *info) {
    if (!info) return;
    
    // ⚠️ VERIFICACIÓN ADICIONAL DE SEGURIDAD: Doble chequeo de whitelist
    if (info->is_whitelisted) {
        char debug_msg[256];
        snprintf(debug_msg, sizeof(debug_msg), 
                 "⚠️ ADVERTENCIA: Intento de alerta RAM para proceso whitelisted '%s' (PID: %d)", 
                 info->name, info->pid);
        gui_add_log_entry("PROCESS_MONITOR", "WARNING", debug_msg);
        return; // NO generar alerta para procesos whitelisted
    }
    
    GUIProcess gui_process;
    if (adapt_process_info_to_gui(info, &gui_process) != 0) {
        return;
    }
    
    gui_update_process(&gui_process);
    
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), 
             "🚨 ALERTA MEMORIA: Proceso '%s' (PID: %d) usando %.1f%% de RAM", 
             info->name, info->pid, info->mem_usage);
    gui_add_log_entry("PROCESS_MONITOR", "ALERT", log_msg);
}

void on_gui_alert_cleared(ProcessInfo *info) {
    if (!info) return;
    
    GUIProcess gui_process;
    if (adapt_process_info_to_gui(info, &gui_process) != 0) {
        return;
    }
    
    gui_update_process(&gui_process);
    
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), 
             "✅ Alerta despejada: Proceso '%s' (PID: %d) volvió a valores normales", 
             info->name, info->pid);
    gui_add_log_entry("PROCESS_MONITOR", "INFO", log_msg);
}

// ============================================================================
// FUNCIONES DE SINCRONIZACIÓN Y ESTADO
// ============================================================================

int sync_gui_with_backend_processes(void) {
    // Obtener una copia thread-safe de todos los procesos monitoreados
    int process_count = 0;
    ProcessInfo *process_list = get_process_list_copy(&process_count);
    
    if (!process_list) {
        // No hay procesos o error - esto es normal si el monitoreo acaba de iniciar
        gui_add_log_entry("PROCESS_INTEGRATION", "INFO", 
                         "No hay procesos monitoreados para sincronizar");
        return 0;
    }
    
    gui_add_log_entry("PROCESS_INTEGRATION", "INFO", 
                     "Sincronizando GUI con backend - procesos encontrados");
    
    // Convertir y actualizar cada proceso en la GUI
    for (int i = 0; i < process_count; i++) {
        GUIProcess gui_process;
        if (adapt_process_info_to_gui(&process_list[i], &gui_process) == 0) {
            gui_update_process(&gui_process);
        }
    }
    
    // Liberar la memoria del array copiado
    free(process_list);
    
    pthread_mutex_lock(&integration_state.state_mutex);
    integration_state.last_process_count = process_count;
    pthread_mutex_unlock(&integration_state.state_mutex);
    
    return process_count;
}

int get_process_statistics_for_gui(int *total_processes, int *high_cpu_count, int *high_memory_count) {
    if (!total_processes || !high_cpu_count || !high_memory_count) {
        return -1;
    }
    
    // Obtener estadísticas del backend usando su interfaz thread-safe
    MonitoringStats stats = get_monitoring_stats();
    
    *total_processes = stats.total_processes;
    *high_cpu_count = stats.high_cpu_count;
    *high_memory_count = stats.high_memory_count;
    
    return 0;
}

int update_process_monitoring_config(float cpu_threshold, float memory_threshold, int check_interval) {
    // Verificar parámetros válidos
    if (cpu_threshold <= 0 || cpu_threshold > 100 || 
        memory_threshold <= 0 || memory_threshold > 100 ||
        check_interval < 1 || check_interval > 3600) {
        gui_add_log_entry("PROCESS_INTEGRATION", "ERROR", 
                         "Parámetros de configuración inválidos");
        return -1;
    }
    
    // Actualizar intervalo dinámicamente (el backend soporta esto)
    set_monitoring_interval(check_interval);
    
    // Los umbrales requieren actualizar la configuración global
    // Por ahora registramos que se necesita actualizar la configuración
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), 
             "Configuración actualizada: CPU=%.1f%%, MEM=%.1f%%, Intervalo=%ds",
             cpu_threshold, memory_threshold, check_interval);
    gui_add_log_entry("PROCESS_INTEGRATION", "INFO", log_msg);
    
    return 0;
}

// ============================================================================
// FUNCIONES DE COMPATIBILIDAD CON GUI EXISTENTE
// ============================================================================

void gui_compatible_scan_processes(void) {
    // Esta función actúa como puente entre el sistema de callbacks
    // simple de la GUI existente y el sistema más sofisticado del backend
    
    pthread_mutex_lock(&integration_state.state_mutex);
    
    if (!integration_state.initialized) {
        pthread_mutex_unlock(&integration_state.state_mutex);
        
        // Si no está inicializado, inicializar automáticamente
        if (init_process_integration() != 0) {
            gui_add_log_entry("PROCESS_INTEGRATION", "ERROR", 
                             "Error al inicializar integración de procesos");
            return;
        }
    }
    
    pthread_mutex_unlock(&integration_state.state_mutex);
    
    // Si el monitoreo no está activo, iniciarlo
    if (!is_monitoring_active()) {
        int result = start_process_monitoring();
        if (result != 0 && result != 1) { // 1 significa que ya estaba activo
            gui_add_log_entry("PROCESS_INTEGRATION", "ERROR", 
                             "Error al iniciar monitoreo de procesos");
            return;
        }
        
        // IMPORTANTE: Simular finalización de "escaneo" después de 3 segundos
        // Esto permite que los botones se desbloqueen mientras el monitoreo continúa
        g_timeout_add_seconds(3, (GSourceFunc)complete_process_scan_simulation, NULL);
        
    } else {
        // Si ya está activo, solo hacer una sincronización para refrescar la vista
        gui_add_log_entry("PROCESS_INTEGRATION", "INFO", 
                         "Actualizando vista de procesos...");
        sync_gui_with_backend_processes();
        
        // También simular que el "escaneo" terminó para desbloquear botones
        g_timeout_add_seconds(1, (GSourceFunc)complete_process_scan_simulation, NULL);
    }
    
    // Actualizar estadísticas en la GUI
    int total, high_cpu, high_mem;
    if (get_process_statistics_for_gui(&total, &high_cpu, &high_mem) == 0) {
        gui_update_statistics(0, total, 0); // 0 para USB y puertos por ahora
    }
}

int is_gui_process_scan_in_progress(void) {
    // El backend de procesos siempre está "escaneando" una vez iniciado,
    // pero para efectos de la GUI, consideramos que hay escaneo en progreso
    // si el monitoreo fue solicitado recientemente y está activo
    pthread_mutex_lock(&integration_state.state_mutex);
    int requested = integration_state.monitoring_requested;
    pthread_mutex_unlock(&integration_state.state_mutex);
    
    return requested && is_monitoring_active();
}

// ============================================================================
// FUNCIONES DE SIMULACIÓN PARA COMPATIBILIDAD CON GUI
// ============================================================================

/**
 * @brief Simula la finalización de un "escaneo" de procesos para la GUI
 * 
 * Los procesos funcionan con monitoreo continuo, no escaneos puntuales como 
 * los puertos. Esta función simula que un escaneo terminó para que los botones
 * de la GUI se desbloqueen, mientras el monitoreo continúa en segundo plano.
 */
static gboolean complete_process_scan_simulation(gpointer user_data) {
    (void)user_data; // Suprimir warning
    
    pthread_mutex_lock(&integration_state.state_mutex);
    
    // Limpiar el flag que indica que se solicitó monitoreo
    // Esto permite que los botones se desbloqueen
    integration_state.monitoring_requested = 0;
    
    pthread_mutex_unlock(&integration_state.state_mutex);
    
    // Actualizar estado de la GUI para mostrar que el "escaneo" terminó
    gui_set_scanning_status(FALSE);
    
    gui_add_log_entry("PROCESS_INTEGRATION", "INFO", 
                     "✅ Escaneo inicial de procesos completado - monitoreo continúa activo");
    
    // Retornar FALSE para que se ejecute solo una vez
    return FALSE;
}