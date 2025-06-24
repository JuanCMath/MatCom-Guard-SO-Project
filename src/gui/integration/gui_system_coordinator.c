#include "gui_system_coordinator.h"
#include "gui_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

// ============================================================================
// ESTADO GLOBAL DEL SISTEMA COORDINADOR
// ============================================================================

// Esta es la variable más importante de todo nuestro sistema: mantiene el
// estado consolidado de MatCom Guard. Debe ser accedida de manera thread-safe
// desde múltiples hilos simultáneamente.
static SystemGlobalState global_state;

// Control del hilo coordinador principal
static pthread_t coordinator_thread;
static volatile int coordinator_active = 0;
static int update_interval_seconds = 5;  // Intervalo por defecto
static int security_evaluation_sensitivity = 5;  // Sensibilidad media por defecto

// ============================================================================
// FUNCIONES INTERNAS DEL COORDINADOR
// ============================================================================

/**
 * Esta función representa el "corazón" del sistema coordinador. Se ejecuta
 * en un hilo separado y es responsable de mantener sincronizado el estado
 * global del sistema. Piensa en ella como el director de una orquesta que
 * constantemente escucha a todos los músicos y ajusta el tempo general.
 */
static void* coordinator_thread_function(void* arg) {
    (void)arg; // Evitar warning de parámetro no usado
    
    gui_add_log_entry("SYSTEM_COORDINATOR", "INFO", 
                     "Hilo coordinador iniciado - sincronizando estado global");
    
    while (!global_state.shutdown_requested) {
        // Marcar el inicio de un ciclo de coordinación
        clock_t cycle_start = clock();
        
        pthread_mutex_lock(&global_state.state_mutex);
        
        // Paso 1: Actualizar estadísticas agregadas de todos los módulos
        // Esta operación recopila información fresca de cada subsistema
        if (update_aggregate_statistics() != 0) {
            gui_add_log_entry("SYSTEM_COORDINATOR", "WARNING", 
                             "Error al actualizar estadísticas agregadas");
        }
        
        // Paso 2: Evaluar el nivel de seguridad general del sistema
        // Esta es la función más crítica: determina si el sistema está seguro
        SystemSecurityLevel previous_level = global_state.security_level;
        global_state.security_level = evaluate_system_security_level();
        global_state.last_security_evaluation = time(NULL);
        
        // Si el nivel de seguridad cambió, es un evento significativo
        if (previous_level != global_state.security_level) {
            char level_change_msg[512];
            const char *level_names[] = {"SAFE", "MONITORING", "WARNING", "CRITICAL", "UNKNOWN"};
            snprintf(level_change_msg, sizeof(level_change_msg),
                     "Nivel de seguridad cambió de %s a %s",
                     level_names[previous_level], level_names[global_state.security_level]);
            
            const char *log_level = (global_state.security_level >= SECURITY_LEVEL_WARNING) ? "ALERT" : "INFO";
            gui_add_log_entry("SECURITY_EVALUATION", log_level, level_change_msg);
        }
        
        // Paso 3: Detectar correlaciones entre módulos
        // Esta análisis puede detectar ataques coordinados que no son obvios
        // cuando se miran los módulos individualmente
        int correlations = detect_cross_module_correlations();
        if (correlations > 0) {
            char correlation_msg[256];
            snprintf(correlation_msg, sizeof(correlation_msg),
                     "Detectadas %d correlaciones de seguridad entre módulos", correlations);
            gui_add_log_entry("CORRELATION_ANALYSIS", "WARNING", correlation_msg);
        }
        
        // Actualizar métricas de rendimiento del coordinador
        global_state.performance_metrics.coordination_cycles_completed++;
        
        pthread_mutex_unlock(&global_state.state_mutex);
        
        // Paso 4: Actualizar la interfaz de usuario (fuera del mutex para evitar bloqueos)
        // Esta operación puede tomar tiempo y no debe bloquear otros hilos
        if (update_gui_with_global_state() != 0) {
            gui_add_log_entry("SYSTEM_COORDINATOR", "WARNING", 
                             "Error al actualizar interfaz con estado global");
        }
        
        // Calcular tiempo de ciclo para métricas de rendimiento
        clock_t cycle_end = clock();
        float cycle_time_ms = ((float)(cycle_end - cycle_start) / CLOCKS_PER_SEC) * 1000.0f;
        
        pthread_mutex_lock(&global_state.state_mutex);
        // Actualizar promedio móvil del tiempo de ciclo
        if (global_state.performance_metrics.coordination_cycles_completed == 1) {
            global_state.performance_metrics.average_coordination_time_ms = cycle_time_ms;
        } else {
            // Promedio móvil simple para suavizar las variaciones
            global_state.performance_metrics.average_coordination_time_ms = 
                (global_state.performance_metrics.average_coordination_time_ms * 0.9f) + (cycle_time_ms * 0.1f);
        }
        pthread_mutex_unlock(&global_state.state_mutex);
        
        // Mostrar métricas de rendimiento cada 5 minutos aproximadamente
        if (global_state.performance_metrics.coordination_cycles_completed % 60 == 0) {  // Cada 5 minutos aprox
            char perf_msg[256];
            snprintf(perf_msg, sizeof(perf_msg),
                     "Métricas coordinador: %d ciclos, %.2fms promedio por ciclo",
                     global_state.performance_metrics.coordination_cycles_completed,
                     global_state.performance_metrics.average_coordination_time_ms);
            gui_add_log_entry("SYSTEM_COORDINATOR", "INFO", perf_msg);
        }
        
        // Esperar el intervalo configurado antes del próximo ciclo
        // Usamos sleeps cortos para poder responder rápidamente a shutdown_requested
        for (int i = 0; i < update_interval_seconds && !global_state.shutdown_requested; i++) {
            sleep(1);
        }
    }
    
    coordinator_active = 0;
    gui_add_log_entry("SYSTEM_COORDINATOR", "INFO", 
                     "Hilo coordinador terminado");
    
    return NULL;
}

// ============================================================================
// FUNCIONES DE INICIALIZACIÓN Y GESTIÓN DEL COORDINADOR
// ============================================================================

int init_system_coordinator(void) {
    // Inicializar el estado global con valores seguros
    memset(&global_state, 0, sizeof(SystemGlobalState));
    
    // Configuración inicial del estado de seguridad
    global_state.security_level = SECURITY_LEVEL_UNKNOWN;
    global_state.last_security_evaluation = 0;
    strcpy(global_state.security_description, "Sistema iniciando - evaluación pendiente");
    
    // Estado inicial de los módulos (se actualizará cuando los módulos reporten)
    global_state.process_module_status = MODULE_STATUS_INACTIVE;
    global_state.usb_module_status = MODULE_STATUS_INACTIVE;
    global_state.ports_module_status = MODULE_STATUS_INACTIVE;
    
    // Inicializar métricas de rendimiento
    global_state.performance_metrics.system_start_time = time(NULL);
    global_state.performance_metrics.coordination_cycles_completed = 0;
    global_state.performance_metrics.gui_updates_sent = 0;
    global_state.performance_metrics.average_coordination_time_ms = 0.0f;
    
    // Inicializar primitivas de sincronización
    if (pthread_mutex_init(&global_state.state_mutex, NULL) != 0) {
        gui_add_log_entry("SYSTEM_COORDINATOR", "ERROR", 
                         "Error al inicializar mutex del estado global");
        return -1;
    }
    
    if (pthread_cond_init(&global_state.state_changed_condition, NULL) != 0) {
        pthread_mutex_destroy(&global_state.state_mutex);
        gui_add_log_entry("SYSTEM_COORDINATOR", "ERROR", 
                         "Error al inicializar variable de condición");
        return -1;
    }
    
    global_state.shutdown_requested = 0;
    
    gui_add_log_entry("SYSTEM_COORDINATOR", "INFO", 
                     "Sistema coordinador inicializado exitosamente");
    
    return 0;
}

int start_system_coordinator(int update_interval_seconds_param) {
    if (coordinator_active) {
        gui_add_log_entry("SYSTEM_COORDINATOR", "WARNING", 
                         "Coordinador ya está activo");
        return 0;
    }
    
    // Validar y configurar intervalo de actualización
    if (update_interval_seconds_param < 1 || update_interval_seconds_param > 300) {
        gui_add_log_entry("SYSTEM_COORDINATOR", "WARNING", 
                         "Intervalo inválido, usando valor por defecto de 5 segundos");
        update_interval_seconds = 5;
    } else {
        update_interval_seconds = update_interval_seconds_param;
    }
    
    // Realizar evaluación inicial antes de iniciar el hilo
    pthread_mutex_lock(&global_state.state_mutex);
    update_aggregate_statistics();
    global_state.security_level = evaluate_system_security_level();
    global_state.last_security_evaluation = time(NULL);
    pthread_mutex_unlock(&global_state.state_mutex);
    
    // Crear el hilo coordinador
    int result = pthread_create(&coordinator_thread, NULL, coordinator_thread_function, NULL);
    if (result != 0) {
        gui_add_log_entry("SYSTEM_COORDINATOR", "ERROR", 
                         "Error al crear hilo coordinador");
        return -1;
    }
    
    coordinator_active = 1;
    
    char start_msg[256];
    snprintf(start_msg, sizeof(start_msg),
             "Coordinador del sistema iniciado (intervalo: %d segundos)",
             update_interval_seconds);
    gui_add_log_entry("SYSTEM_COORDINATOR", "INFO", start_msg);
    
    return 0;
}

int stop_system_coordinator(void) {
    if (!coordinator_active) {
        gui_add_log_entry("SYSTEM_COORDINATOR", "INFO", 
                         "Coordinador no está activo");
        return 0;
    }
    
    // Señalar al hilo que debe terminar
    pthread_mutex_lock(&global_state.state_mutex);
    global_state.shutdown_requested = 1;
    pthread_cond_signal(&global_state.state_changed_condition);
    pthread_mutex_unlock(&global_state.state_mutex);
    
    // Esperar a que el hilo termine
    int result = pthread_join(coordinator_thread, NULL);
    if (result != 0) {
        gui_add_log_entry("SYSTEM_COORDINATOR", "ERROR", 
                         "Error al esperar terminación del hilo coordinador");
        return -1;
    }
    
    gui_add_log_entry("SYSTEM_COORDINATOR", "INFO", 
                     "Coordinador del sistema detenido exitosamente");
    
    return 0;
}

void cleanup_system_coordinator(void) {
    // Detener el coordinador si está activo
    if (coordinator_active) {
        stop_system_coordinator();
    }
    
    // Limpiar primitivas de sincronización
    pthread_mutex_destroy(&global_state.state_mutex);
    pthread_cond_destroy(&global_state.state_changed_condition);
    
    // Limpiar el estado global
    memset(&global_state, 0, sizeof(SystemGlobalState));
    
    gui_add_log_entry("SYSTEM_COORDINATOR", "INFO", 
                     "Recursos del coordinador liberados");
}

// ============================================================================
// FUNCIONES DE EVALUACIÓN Y ACTUALIZACIÓN DE ESTADO
// ============================================================================

SystemSecurityLevel evaluate_system_security_level(void) {
    // Esta función implementa la lógica central de evaluación de seguridad
    // Es como un médico que examina todos los síntomas para hacer un diagnóstico
    
    int security_score = 0;  // Puntuación de 0 (seguro) a 100 (crítico)
    int total_threats = 0;
    
    // Evaluar amenazas de procesos
    if (global_state.aggregate_stats.suspicious_processes > 0) {
        security_score += global_state.aggregate_stats.suspicious_processes * 15;
        total_threats += global_state.aggregate_stats.suspicious_processes;
    }
    
    // Evaluar amenazas USB
    if (global_state.aggregate_stats.suspicious_usb_devices > 0) {
        security_score += global_state.aggregate_stats.suspicious_usb_devices * 25;
        total_threats += global_state.aggregate_stats.suspicious_usb_devices;
    }
    
    // Evaluar amenazas de puertos
    if (global_state.aggregate_stats.suspicious_ports > 0) {
        security_score += global_state.aggregate_stats.suspicious_ports * 20;
        total_threats += global_state.aggregate_stats.suspicious_ports;
    }
    
    // Evaluar procesos que exceden umbrales (menos crítico pero relevante)
    security_score += (global_state.aggregate_stats.processes_exceeding_cpu_threshold * 2);
    security_score += (global_state.aggregate_stats.processes_exceeding_memory_threshold * 2);
    
    // Evaluar estado de los módulos (si algún módulo tiene error, aumentar puntuación)
    if (global_state.process_module_status == MODULE_STATUS_ERROR) security_score += 10;
    if (global_state.usb_module_status == MODULE_STATUS_ERROR) security_score += 10;
    if (global_state.ports_module_status == MODULE_STATUS_ERROR) security_score += 10;
    
    // Aplicar sensibilidad configurada
    security_score = (security_score * security_evaluation_sensitivity) / 5;
    
    // Convertir puntuación a nivel de seguridad
    SystemSecurityLevel level;
    char description[512];
    
    if (security_score == 0) {
        level = SECURITY_LEVEL_SAFE;
        strcpy(description, "Sistema seguro - No se detectaron amenazas");
    } else if (security_score <= 10) {
        level = SECURITY_LEVEL_MONITORING;
        snprintf(description, sizeof(description),
                 "Monitoreo activo - Actividad menor detectada (puntuación: %d)", security_score);
    } else if (security_score <= 30) {
        level = SECURITY_LEVEL_WARNING;
        snprintf(description, sizeof(description),
                 "Advertencia - %d amenaza(s) detectada(s) (puntuación: %d)", 
                 total_threats, security_score);
    } else {
        level = SECURITY_LEVEL_CRITICAL;
        snprintf(description, sizeof(description),
                 "CRÍTICO - Múltiples amenazas activas detectadas (puntuación: %d)", security_score);
    }
    
    // Actualizar descripción en el estado global
    strncpy(global_state.security_description, description, sizeof(global_state.security_description) - 1);
    global_state.security_description[sizeof(global_state.security_description) - 1] = '\0';
    
    return level;
}

int update_aggregate_statistics(void) {
    // Esta función actúa como un "recolector de datos" que va a cada módulo
    // y pregunta por su estado actual, luego consolida toda esa información
    
    int success_count = 0;
    int total_modules = 3;
    
    // Recopilar estadísticas de procesos
    int processes_monitored = 0, high_cpu = 0, high_mem = 0;
    if (get_process_statistics_for_gui(&processes_monitored, &high_cpu, &high_mem) == 0) {
        global_state.aggregate_stats.total_processes_monitored = processes_monitored;
        global_state.aggregate_stats.processes_exceeding_cpu_threshold = high_cpu;
        global_state.aggregate_stats.processes_exceeding_memory_threshold = high_mem;
        global_state.aggregate_stats.last_process_scan = time(NULL);
        
        // Actualizar estado del módulo de procesos
        if (is_process_monitoring_active()) {
            global_state.process_module_status = MODULE_STATUS_ACTIVE;
        } else {
            global_state.process_module_status = MODULE_STATUS_INACTIVE;
        }
        
        success_count++;
    } else {
        global_state.process_module_status = MODULE_STATUS_ERROR;
    }
    
    // Recopilar estadísticas de USB
    int usb_devices = 0, suspicious_usb = 0, total_files = 0;
    if (get_usb_statistics_for_gui(&usb_devices, &suspicious_usb, &total_files) == 0) {
        global_state.aggregate_stats.total_usb_devices = usb_devices;
        global_state.aggregate_stats.suspicious_usb_devices = suspicious_usb;
        global_state.aggregate_stats.total_files_monitored = total_files;
        global_state.aggregate_stats.last_usb_scan = time(NULL);
        
        // Actualizar estado del módulo USB
        if (is_usb_monitoring_active()) {
            if (is_gui_usb_scan_in_progress()) {
                global_state.usb_module_status = MODULE_STATUS_SCANNING;
            } else {
                global_state.usb_module_status = MODULE_STATUS_ACTIVE;
            }
        } else {
            global_state.usb_module_status = MODULE_STATUS_INACTIVE;
        }
        
        success_count++;
    } else {
        global_state.usb_module_status = MODULE_STATUS_ERROR;
    }
    
    // Recopilar estadísticas de puertos
    int open_ports = 0, suspicious_ports = 0;
    time_t last_port_scan;
    if (get_port_statistics_for_gui(&open_ports, &suspicious_ports, &last_port_scan) == 0) {
        global_state.aggregate_stats.open_ports_found = open_ports;
        global_state.aggregate_stats.suspicious_ports = suspicious_ports;
        global_state.aggregate_stats.last_port_scan = last_port_scan;
        
        // Actualizar estado del módulo de puertos
        if (is_port_scan_active()) {
            global_state.ports_module_status = MODULE_STATUS_SCANNING;
        } else {
            global_state.ports_module_status = MODULE_STATUS_ACTIVE;
        }
        
        success_count++;
    } else {
        global_state.ports_module_status = MODULE_STATUS_ERROR;
    }
    
    // Actualizar timestamp de última actualización
    global_state.aggregate_stats.statistics_last_updated = time(NULL);
    
    // Determinar si la operación fue exitosa
    if (success_count == total_modules) {
        return 0;  // Todos los módulos respondieron correctamente
    } else if (success_count > 0) {
        char warning_msg[256];
        snprintf(warning_msg, sizeof(warning_msg),
                 "Solo %d de %d módulos respondieron correctamente", success_count, total_modules);
        gui_add_log_entry("SYSTEM_COORDINATOR", "WARNING", warning_msg);
        return 0;  // Éxito parcial
    } else {
        gui_add_log_entry("SYSTEM_COORDINATOR", "ERROR", 
                         "Ningún módulo respondió - posible fallo del sistema");
        return -1;  // Fallo completo
    }
}

int detect_cross_module_correlations(void) {
    // Esta función implementa análisis de correlación entre módulos para
    // detectar patrones de ataque que podrían no ser obvios individualmente
    
    int correlations_detected = 0;
    time_t current_time = time(NULL);
    
    // Correlación 1: Actividad sospechosa simultánea en procesos y USB
    // Esto podría indicar malware que se propaga a través de dispositivos USB
    if (global_state.aggregate_stats.suspicious_processes > 0 && 
        global_state.aggregate_stats.suspicious_usb_devices > 0) {
        
        // Verificar si ambos eventos ocurrieron en una ventana de tiempo cercana
        time_t time_diff = abs((int)(global_state.aggregate_stats.last_process_scan - 
                                   global_state.aggregate_stats.last_usb_scan));
        
        if (time_diff <= 300) {  // 5 minutos de ventana
            gui_add_log_entry("CORRELATION_ANALYSIS", "ALERT", 
                             "🔗 Correlación detectada: Actividad sospechosa simultánea en procesos y USB");
            correlations_detected++;
        }
    }
    
    // Correlación 2: Muchos puertos abiertos + procesos con alta CPU
    // Esto podría indicar actividad de red maliciosa o minería de criptomonedas
    if (global_state.aggregate_stats.open_ports_found > 20 && 
        global_state.aggregate_stats.processes_exceeding_cpu_threshold > 5) {
        
        gui_add_log_entry("CORRELATION_ANALYSIS", "WARNING", 
                         "🔗 Correlación detectada: Alta actividad de red con uso intensivo de CPU");
        correlations_detected++;
    }
    
    // Correlación 3: Puertos sospechosos + cambios en archivos USB
    // Esto podría indicar exfiltración de datos o comunicación con C&C
    if (global_state.aggregate_stats.suspicious_ports > 0 && 
        global_state.aggregate_stats.files_with_changes > 10) {
        
        gui_add_log_entry("CORRELATION_ANALYSIS", "ALERT", 
                         "🔗 Correlación detectada: Puertos sospechosos con modificación masiva de archivos");
        correlations_detected++;
    }
    
    // Correlación 4: Degradación general del sistema
    // Si múltiples módulos están reportando errores simultáneamente
    int modules_with_errors = 0;
    if (global_state.process_module_status == MODULE_STATUS_ERROR) modules_with_errors++;
    if (global_state.usb_module_status == MODULE_STATUS_ERROR) modules_with_errors++;
    if (global_state.ports_module_status == MODULE_STATUS_ERROR) modules_with_errors++;
    
    if (modules_with_errors >= 2) {
        gui_add_log_entry("CORRELATION_ANALYSIS", "CRITICAL", 
                         "🔗 Correlación detectada: Fallo múltiple de módulos - posible ataque al sistema");
        correlations_detected++;
    }
    
    return correlations_detected;
}

int update_gui_with_global_state(void) {
    // Esta función es responsable de actualizar todos los elementos de la GUI
    // con la información más reciente del estado global
    
    pthread_mutex_lock(&global_state.state_mutex);
    
    // Crear una copia local para minimizar el tiempo con el mutex bloqueado
    SystemGlobalState local_state = global_state;
    global_state.performance_metrics.gui_updates_sent++;
    
    pthread_mutex_unlock(&global_state.state_mutex);
    
    // Actualizar panel de estadísticas principales
    gui_update_statistics(
        local_state.aggregate_stats.total_usb_devices,
        local_state.aggregate_stats.total_processes_monitored,
        local_state.aggregate_stats.open_ports_found
    );
    
    // Actualizar estado del sistema basándose en el nivel de seguridad
    gboolean system_healthy = (local_state.security_level <= SECURITY_LEVEL_MONITORING);
    const char* status_descriptions[] = {
        "Sistema Seguro",           // SECURITY_LEVEL_SAFE
        "Monitoreo Activo",         // SECURITY_LEVEL_MONITORING  
        "Actividad Sospechosa",     // SECURITY_LEVEL_WARNING
        "Amenazas Críticas",        // SECURITY_LEVEL_CRITICAL
        "Estado Desconocido"        // SECURITY_LEVEL_UNKNOWN
    };
    
    gui_update_system_status(status_descriptions[local_state.security_level], system_healthy);
    
    // Si hay amenazas críticas, asegurar que el estado de escaneo refleje actividad
    if (local_state.security_level >= SECURITY_LEVEL_WARNING) {
        // Verificar si algún módulo está escaneando activamente
        gboolean any_scanning = (local_state.process_module_status == MODULE_STATUS_SCANNING ||
                                 local_state.usb_module_status == MODULE_STATUS_SCANNING ||
                                 local_state.ports_module_status == MODULE_STATUS_SCANNING);
        
        if (any_scanning) {
            gui_set_scanning_status(TRUE);
        } else {
            gui_set_scanning_status(FALSE);
        }
    }
    
    // Registrar actualización exitosa periódicamente
    static int update_count = 0;
    update_count++;
    if (update_count % 20 == 0) {  // Cada 20 actualizaciones
        char update_msg[256];
        snprintf(update_msg, sizeof(update_msg),
                 "GUI actualizada (#%d): Nivel seguridad=%d, USB=%d, Procesos=%d, Puertos=%d",
                 update_count, local_state.security_level,
                 local_state.aggregate_stats.total_usb_devices,
                 local_state.aggregate_stats.total_processes_monitored,
                 local_state.aggregate_stats.open_ports_found);
        gui_add_log_entry("SYSTEM_COORDINATOR", "INFO", update_msg);
    }
    
    return 0;
}

// ============================================================================
// FUNCIONES DE ACCESO AL ESTADO GLOBAL (THREAD-SAFE)
// ============================================================================

int get_global_state_copy(SystemGlobalState *state_copy) {
    if (!state_copy) {
        return -1;
    }
    
    pthread_mutex_lock(&global_state.state_mutex);
    *state_copy = global_state;  // Copia completa de la estructura
    pthread_mutex_unlock(&global_state.state_mutex);
    
    return 0;
}

SystemSecurityLevel get_current_security_level(void) {
    pthread_mutex_lock(&global_state.state_mutex);
    SystemSecurityLevel level = global_state.security_level;
    pthread_mutex_unlock(&global_state.state_mutex);
    
    return level;
}

int get_consolidated_statistics(int *total_devices, int *total_processes, 
                               int *total_open_ports, int *security_alerts) {
    if (!total_devices || !total_processes || !total_open_ports || !security_alerts) {
        return -1;
    }
    
    pthread_mutex_lock(&global_state.state_mutex);
    
    *total_devices = global_state.aggregate_stats.total_usb_devices;
    *total_processes = global_state.aggregate_stats.total_processes_monitored;
    *total_open_ports = global_state.aggregate_stats.open_ports_found;
    
    // Calcular total de alertas de seguridad activas
    *security_alerts = global_state.aggregate_stats.suspicious_processes +
                      global_state.aggregate_stats.suspicious_usb_devices +
                      global_state.aggregate_stats.suspicious_ports;
    
    pthread_mutex_unlock(&global_state.state_mutex);
    
    return 0;
}

// ============================================================================
// FUNCIONES DE NOTIFICACIÓN DE EVENTOS ENTRE MÓDULOS
// ============================================================================

int notify_security_event(const char *source_module, int event_severity, 
                         const char *event_description) {
    if (!source_module || !event_description) {
        return -1;
    }
    
    if (event_severity < 1 || event_severity > 10) {
        event_severity = 5;  // Valor por defecto para severidad inválida
    }
    
    char notification_msg[1024];
    snprintf(notification_msg, sizeof(notification_msg),
             "📢 Evento de seguridad reportado por %s (severidad %d/10): %s",
             source_module, event_severity, event_description);
    
    const char *log_level = (event_severity >= 8) ? "ALERT" : 
                           (event_severity >= 6) ? "WARNING" : "INFO";
    
    gui_add_log_entry("SECURITY_EVENTS", log_level, notification_msg);
    
    // Si el evento es de alta severidad, forzar reevaluación inmediata
    if (event_severity >= 7) {
        request_immediate_system_evaluation();
    }
    
    return 0;
}

int notify_module_status_change(const char *module_name, ModuleStatus new_status,
                               const char *status_description) {
    if (!module_name || !status_description) {
        return -1;
    }
    
    pthread_mutex_lock(&global_state.state_mutex);
    
    // Actualizar el estado del módulo correspondiente
    if (strcmp(module_name, "process") == 0) {
        global_state.process_module_status = new_status;
    } else if (strcmp(module_name, "usb") == 0) {
        global_state.usb_module_status = new_status;
    } else if (strcmp(module_name, "ports") == 0) {
        global_state.ports_module_status = new_status;
    }
    
    pthread_mutex_unlock(&global_state.state_mutex);
    
    const char *status_names[] = {"INACTIVE", "ACTIVE", "SCANNING", "ERROR", "MAINTENANCE"};
    char status_msg[512];
    snprintf(status_msg, sizeof(status_msg),
             "Módulo %s cambió estado a %s: %s",
             module_name, status_names[new_status], status_description);
    
    const char *log_level = (new_status == MODULE_STATUS_ERROR) ? "ERROR" : "INFO";
    gui_add_log_entry("MODULE_STATUS", log_level, status_msg);
    
    return 0;
}

// ============================================================================
// FUNCIONES DE CONFIGURACIÓN Y AJUSTES DINÁMICOS
// ============================================================================

int update_coordinator_configuration(int update_interval, int security_evaluation_sensitivity_param) {
    if (update_interval < 1 || update_interval > 300) {
        gui_add_log_entry("SYSTEM_COORDINATOR", "ERROR", 
                         "Intervalo de actualización inválido (debe estar entre 1 y 300 segundos)");
        return -1;
    }
    
    if (security_evaluation_sensitivity_param < 1 || security_evaluation_sensitivity_param > 10) {
        gui_add_log_entry("SYSTEM_COORDINATOR", "ERROR", 
                         "Sensibilidad de evaluación inválida (debe estar entre 1 y 10)");
        return -1;
    }
    
    update_interval_seconds = update_interval;
    security_evaluation_sensitivity = security_evaluation_sensitivity_param;
    
    char config_msg[256];
    snprintf(config_msg, sizeof(config_msg),
             "Configuración coordinador actualizada: intervalo=%ds, sensibilidad=%d/10",
             update_interval, security_evaluation_sensitivity_param);
    gui_add_log_entry("SYSTEM_COORDINATOR", "INFO", config_msg);
    
    return 0;
}

int request_immediate_system_evaluation(void) {
    if (!coordinator_active) {
        gui_add_log_entry("SYSTEM_COORDINATOR", "WARNING", 
                         "No se puede evaluar: coordinador no está activo");
        return -1;
    }
    
    // Señalar al hilo coordinador que realice una evaluación inmediata
    pthread_mutex_lock(&global_state.state_mutex);
    pthread_cond_signal(&global_state.state_changed_condition);
    pthread_mutex_unlock(&global_state.state_mutex);
    
    gui_add_log_entry("SYSTEM_COORDINATOR", "INFO", 
                     "Evaluación inmediata del sistema solicitada");
    
    return 0;
}