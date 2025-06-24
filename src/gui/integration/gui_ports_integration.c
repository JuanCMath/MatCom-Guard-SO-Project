#include "gui_ports_integration.h"
#include "gui_internal.h"
#include "port_scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

// ============================================================================
// ESTRUCTURAS DE ESTADO PARA ESCANEO DE PUERTOS
// ============================================================================

// Esta estructura representa el estado más complejo que hemos manejado hasta ahora
// porque debe coordinar escaneos de larga duración con feedback en tiempo real
typedef struct {
    int initialized;                    // ¿Está inicializado el sistema?
    int scan_active;                    // ¿Hay un escaneo actualmente en progreso?
    int scan_cancelled;                 // ¿El usuario canceló el escaneo actual?
    PortScanConfig current_config;      // Configuración del escaneo actual
    
    // Información de progreso para feedback de usuario en tiempo real
    int total_ports_to_scan;            // Total de puertos en el escaneo actual
    int ports_completed;                // Puertos ya escaneados
    time_t scan_start_time;             // Momento en que comenzó el escaneo
    float last_progress_percentage;     // Último porcentaje reportado
    
    // Resultados del último escaneo completado
    PortInfo *last_results;             // Array con resultados del último escaneo
    int last_results_count;             // Número de puertos en los resultados
    time_t last_scan_completion_time;   // Cuándo se completó el último escaneo
    
    // Threading y sincronización
    pthread_t scan_thread;              // Hilo donde se ejecuta el escaneo
    pthread_mutex_t state_mutex;        // Protege el acceso concurrente al estado
    volatile int should_stop_scan;      // Señal para cancelar escaneo en progreso
    
} PortsIntegrationState;

static PortsIntegrationState ports_state = {
    .initialized = 0,
    .scan_active = 0,
    .scan_cancelled = 0,
    .total_ports_to_scan = 0,
    .ports_completed = 0,
    .scan_start_time = 0,
    .last_progress_percentage = 0.0,
    .last_results = NULL,
    .last_results_count = 0,
    .last_scan_completion_time = 0,
    .should_stop_scan = 0,
    .state_mutex = PTHREAD_MUTEX_INITIALIZER
};

// ============================================================================
// FUNCIONES INTERNAS DEL HILO DE ESCANEO
// ============================================================================

// Declaraciones de funciones internas
static void* port_scanning_thread_function(void* arg);
static gboolean cleanup_scan_thread_callback(gpointer user_data);

/**
 * Esta función representa el núcleo del sistema de escaneo de puertos.
 * Se ejecuta en un hilo separado para no bloquear la interfaz de usuario,
 * y debe manejar la complejidad de coordinar con el backend mientras
 * proporciona feedback continuo a la GUI.
 * 
 * La función implementa un patrón de "escaneo progresivo" donde actualiza
 * el estado de la GUI después de cada puerto escaneado, permitiendo que
 * el usuario vea el progreso en tiempo real incluso en escaneos de horas.
 */
static void* port_scanning_thread_function(void* arg) {
    PortScanConfig *pconfig = (PortScanConfig*)arg;
    
    if (!pconfig) {
        gui_add_log_entry("PORT_INTEGRATION", "ERROR", 
                         "Configuración de escaneo nula en hilo de escaneo");
        return NULL;
    }
    
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), 
             "Iniciando escaneo de puertos %d-%d en hilo separado", 
             pconfig->start_port, pconfig->end_port);
    gui_add_log_entry("PORT_SCANNER", "INFO", log_msg);
    
    // Configurar estado de escaneo
    pthread_mutex_lock(&ports_state.state_mutex);
    
    // Liberar resultados anteriores
    if (ports_state.last_results) {
        free(ports_state.last_results);
        ports_state.last_results = NULL;
    }
    
    ports_state.total_ports_to_scan = pconfig->end_port - pconfig->start_port + 1;
    ports_state.ports_completed = 0;
    ports_state.scan_start_time = time(NULL);
    ports_state.last_progress_percentage = 0.0;
    ports_state.should_stop_scan = 0;
    
    pthread_mutex_unlock(&ports_state.state_mutex);
    
    // Crear array para almacenar resultados
    int total_ports = pconfig->end_port - pconfig->start_port + 1;
    PortInfo *scan_results = malloc(total_ports * sizeof(PortInfo));
    if (!scan_results) {
        gui_add_log_entry("PORT_INTEGRATION", "ERROR", 
                         "Error de memoria al inicializar escaneo");
        free(pconfig);
        return NULL;
    }
    
    int results_count = 0;
    int open_ports = 0;
    int suspicious_ports = 0;
    
    // ESCANEO REAL PUERTO POR PUERTO
    for (int port = pconfig->start_port; port <= pconfig->end_port; port++) {
        // Verificar si se debe cancelar
        pthread_mutex_lock(&ports_state.state_mutex);
        int should_stop = ports_state.should_stop_scan;
        pthread_mutex_unlock(&ports_state.state_mutex);        if (should_stop) {
            gui_add_log_entry("PORT_SCANNER", "INFO", "Escaneo cancelado por usuario");
            // Llamar al callback para notificar cancelación
            on_port_scan_completed(NULL, 0, 1);  // 1 = cancelado
            // Limpiar memoria antes de salir
            free(scan_results);
            free(pconfig);
            pthread_mutex_lock(&ports_state.state_mutex);
            ports_state.scan_active = 0;
            ports_state.scan_cancelled = 1;
            pthread_mutex_unlock(&ports_state.state_mutex);
            gui_set_scanning_status(FALSE);
            return NULL;
        }
        
        // Escanear el puerto usando la función del backend
        int is_open = scan_specific_port(port);
        
        if (is_open == 1) {  // Puerto abierto
            PortInfo *port_info = &scan_results[results_count];
            port_info->port = port;
            port_info->is_open = 1;
            
            // Obtener información del servicio usando funciones del backend
            // Como no tenemos acceso directo a get_service_name, usamos lógica similar
            int is_suspicious = 0;
            
            // Mapeo de servicios basado en el backend
            if (port == 31337 || port == 4444 || port == 6667) {
                is_suspicious = 1;
                if (port == 31337) strcpy(port_info->service_name, "Elite/Backdoor");
                else if (port == 4444) strcpy(port_info->service_name, "Metasploit");
                else if (port == 6667) strcpy(port_info->service_name, "IRC");
            } else if (port == 23) {
                strcpy(port_info->service_name, "Telnet");
                is_suspicious = 1;
            } else if (port == 3389) {
                strcpy(port_info->service_name, "RDP");
                is_suspicious = 1;
            } else if (port == 5900) {
                strcpy(port_info->service_name, "VNC");
                is_suspicious = 1;
            } else if (port == 21) {
                strcpy(port_info->service_name, "FTP");
            } else if (port == 22) {
                strcpy(port_info->service_name, "SSH");
            } else if (port == 80) {
                strcpy(port_info->service_name, "HTTP");
            } else if (port == 443) {
                strcpy(port_info->service_name, "HTTPS");
            } else if (port == 8080) {
                strcpy(port_info->service_name, "HTTP-Alt");
            } else if (port > 1024) {
                strcpy(port_info->service_name, "Unknown");
                is_suspicious = 1;  // Puertos altos sin justificación
            } else {
                strcpy(port_info->service_name, "Unknown");
            }
            
            port_info->is_suspicious = is_suspicious;
            
            open_ports++;            if (is_suspicious) {
                suspicious_ports++;
                // Reducir la cantidad de logs para puertos sospechosos en escaneo completo
                if (total_ports < 1000) {  // Solo log detallado para escaneos pequeños
                    snprintf(log_msg, sizeof(log_msg), 
                             "[ALERTA] Puerto %d/tcp abierto (%s) - SOSPECHOSO", 
                             port, port_info->service_name);
                    gui_add_log_entry("PORT_SCANNER", "WARNING", log_msg);
                }
            } else {
                // Solo log para escaneos pequeños
                if (total_ports < 1000) {
                    snprintf(log_msg, sizeof(log_msg), 
                             "[OK] Puerto %d/tcp (%s) abierto", 
                             port, port_info->service_name);
                    gui_add_log_entry("PORT_SCANNER", "INFO", log_msg);
                }
            }
            
            results_count++;
        }
          // Actualizar progreso
        pthread_mutex_lock(&ports_state.state_mutex);
        ports_state.ports_completed = port - pconfig->start_port + 1;
        ports_state.last_progress_percentage = 
            (float)ports_state.ports_completed / ports_state.total_ports_to_scan * 100.0;
        pthread_mutex_unlock(&ports_state.state_mutex);
        
        // Mostrar progreso cada 100 puertos para reducir carga en GUI
        if ((port - pconfig->start_port + 1) % 100 == 0) {
            snprintf(log_msg, sizeof(log_msg), 
                     "Progreso: %d/%d puertos escaneados (%.1f%%)", 
                     ports_state.ports_completed, ports_state.total_ports_to_scan,
                     ports_state.last_progress_percentage);
            gui_add_log_entry("PORT_SCANNER", "INFO", log_msg);
        }
    }
      // Finalizar escaneo y almacenar resultados
    pthread_mutex_lock(&ports_state.state_mutex);
    
    ports_state.last_results = scan_results;
    ports_state.last_results_count = results_count;
    ports_state.last_scan_completion_time = time(NULL);
    ports_state.scan_active = 0;
    
    pthread_mutex_unlock(&ports_state.state_mutex);
    
    // Reporte final
    snprintf(log_msg, sizeof(log_msg), 
             "Escaneo completado: %d puertos abiertos, %d sospechosos de %d totales", 
             open_ports, suspicious_ports, total_ports);
    gui_add_log_entry("PORT_SCANNER", "INFO", log_msg);
      // LLAMAR AL CALLBACK PARA ACTUALIZAR LA GUI
    on_port_scan_completed(scan_results, results_count, 0);  // 0 = no cancelado
    
    free(pconfig);
    return NULL;
}

// ============================================================================
// FUNCIONES DE ESCANEO PREDEFINIDAS
// ============================================================================

int perform_quick_port_scan(void) {
    PortScanConfig pconfig = {
        .scan_type = SCAN_TYPE_QUICK,
        .start_port = 1,
        .end_port = 32768,  // Extended range to include suspicious ports for testing
        .timeout_seconds = 1,
        .concurrent_scans = 1,
        .report_progress = 1
    };
    
    gui_add_log_entry("PORT_SCANNER", "INFO", 
                     "Iniciando escaneo rápido de puertos comunes (1-32768) + puertos sospechosos");
    
    return start_port_scan(&pconfig);
}

int perform_full_port_scan(void) {
    // Mostrar advertencia sobre el tiempo que puede tomar
    gui_add_log_entry("PORT_SCANNER", "WARNING", 
                     "Iniciando escaneo completo - ESTO PUEDE TOMAR VARIAS HORAS");
    
    PortScanConfig pconfig = {
        .scan_type = SCAN_TYPE_FULL,
        .start_port = 1,
        .end_port = 65535,
        .timeout_seconds = 2,
        .concurrent_scans = 1,
        .report_progress = 1
    };
    
    return start_port_scan(&pconfig);
}

int perform_custom_port_scan(int start_port, int end_port) {
    if (start_port < 1 || end_port > 65535 || start_port > end_port) {
        gui_add_log_entry("PORT_SCANNER", "ERROR", 
                         "Rango de puertos personalizado inválido");
        return -1;
    }
    
    PortScanConfig pconfig = {
        .scan_type = SCAN_TYPE_CUSTOM,
        .start_port = start_port,
        .end_port = end_port,
        .timeout_seconds = 1,
        .concurrent_scans = 1,
        .report_progress = 1
    };
    
    char scan_msg[256];
    snprintf(scan_msg, sizeof(scan_msg), 
             "Iniciando escaneo personalizado de puertos %d-%d", start_port, end_port);
    gui_add_log_entry("PORT_SCANNER", "INFO", scan_msg);
    
    return start_port_scan(&pconfig);
}

// ============================================================================
// GESTIÓN DE RESULTADOS Y ESTADÍSTICAS
// ============================================================================

int get_last_scan_results(PortInfo **result_buffer, int *buffer_size) {
    if (!result_buffer || !buffer_size) {
        return -1;
    }
    
    pthread_mutex_lock(&ports_state.state_mutex);
    
    if (!ports_state.last_results || ports_state.last_results_count == 0) {
        pthread_mutex_unlock(&ports_state.state_mutex);
        *result_buffer = NULL;
        *buffer_size = 0;
        return 0;
    }
    
    // Crear una copia de los resultados para thread-safety
    *buffer_size = ports_state.last_results_count;
    *result_buffer = malloc(ports_state.last_results_count * sizeof(PortInfo));
    
    if (*result_buffer) {
        memcpy(*result_buffer, ports_state.last_results, 
               ports_state.last_results_count * sizeof(PortInfo));
    }
    
    int count = ports_state.last_results_count;
    pthread_mutex_unlock(&ports_state.state_mutex);
    
    return count;
}

int get_port_statistics_for_gui(int *total_open, int *total_suspicious, time_t *last_scan_time) {
    if (!total_open || !total_suspicious || !last_scan_time) {
        return -1;
    }
    
    pthread_mutex_lock(&ports_state.state_mutex);
    
    *total_open = 0;
    *total_suspicious = 0;
    *last_scan_time = ports_state.last_scan_completion_time;
    
    // Contar puertos abiertos y sospechosos en los últimos resultados
    if (ports_state.last_results) {
        for (int i = 0; i < ports_state.last_results_count; i++) {
            if (ports_state.last_results[i].is_open) {
                (*total_open)++;
            }
            if (ports_state.last_results[i].is_suspicious) {
                (*total_suspicious)++;
            }
        }
    }
    
    pthread_mutex_unlock(&ports_state.state_mutex);
    return 0;
}

int generate_port_scan_report(const char *report_filename, int include_closed_ports) {
    if (!report_filename) {
        return -1;
    }
    
    pthread_mutex_lock(&ports_state.state_mutex);
    
    if (!ports_state.last_results || ports_state.last_results_count == 0) {
        pthread_mutex_unlock(&ports_state.state_mutex);
        gui_add_log_entry("PORT_INTEGRATION", "ERROR", 
                         "No hay resultados de escaneo para generar reporte");
        return -1;
    }
    
    // Crear una copia para trabajar sin bloquear por mucho tiempo
    int results_count = ports_state.last_results_count;
    PortInfo *results_copy = malloc(results_count * sizeof(PortInfo));
    memcpy(results_copy, ports_state.last_results, results_count * sizeof(PortInfo));
    time_t scan_time = ports_state.last_scan_completion_time;
    
    pthread_mutex_unlock(&ports_state.state_mutex);
    
    // Generar el reporte
    FILE *report_file = fopen(report_filename, "w");
    if (!report_file) {
        free(results_copy);
        gui_add_log_entry("PORT_INTEGRATION", "ERROR", 
                         "No se pudo crear archivo de reporte");
        return -1;
    }
    
    // Escribir encabezado del reporte
    fprintf(report_file, "REPORTE DE ESCANEO DE PUERTOS - MATCOM GUARD\n");
    fprintf(report_file, "=============================================\n\n");
    
    char time_str[64];
    struct tm *tm_info = localtime(&scan_time);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(report_file, "Fecha del escaneo: %s\n", time_str);
    fprintf(report_file, "Total de puertos analizados: %d\n\n", results_count);
    
    // Escribir resultados
    int open_count = 0, suspicious_count = 0;
    fprintf(report_file, "PUERTOS ABIERTOS ENCONTRADOS:\n");
    fprintf(report_file, "------------------------------\n");
    
    for (int i = 0; i < results_count; i++) {
        if (results_copy[i].is_open) {
            open_count++;
            fprintf(report_file, "Puerto %d - %s", 
                    results_copy[i].port, results_copy[i].service_name);
            
            if (results_copy[i].is_suspicious) {
                fprintf(report_file, " [SOSPECHOSO]");
                suspicious_count++;
            }
            fprintf(report_file, "\n");
        }
    }
    
    fprintf(report_file, "\nRESUMEN:\n");
    fprintf(report_file, "--------\n");
    fprintf(report_file, "Puertos abiertos: %d\n", open_count);
    fprintf(report_file, "Puertos sospechosos: %d\n", suspicious_count);
    
    if (suspicious_count > 0) {
        fprintf(report_file, "\nADVERTENCIA: Se encontraron puertos sospechosos.\n");
        fprintf(report_file, "Se recomienda investigar estos puertos inmediatamente.\n");
    }
    
    fclose(report_file);
    free(results_copy);
    
    char report_msg[512];
    snprintf(report_msg, sizeof(report_msg), 
             "Reporte de puertos generado: %s (%d puertos abiertos, %d sospechosos)", 
             report_filename, open_count, suspicious_count);
    gui_add_log_entry("PORT_INTEGRATION", "INFO", report_msg);
    
    return 0;
}

// ============================================================================
// FUNCIONES DE COMPATIBILIDAD CON GUI EXISTENTE
// ============================================================================

void gui_compatible_scan_ports(void) {
    // Esta función actúa como puente inteligente entre la GUI simple
    // y el sistema complejo de escaneo de puertos
    
    pthread_mutex_lock(&ports_state.state_mutex);
    
    if (!ports_state.initialized) {
        pthread_mutex_unlock(&ports_state.state_mutex);
        
        // Auto-inicializar si no está inicializado
        if (init_ports_integration() != 0) {
            gui_add_log_entry("PORT_INTEGRATION", "ERROR", 
                             "Error al inicializar integración de puertos");
            return;
        }
    }
    
    // Si ya hay un escaneo en progreso, mostrar progreso actual
    if (ports_state.scan_active) {
        pthread_mutex_unlock(&ports_state.state_mutex);
        
        float progress;
        int scanned, total, time_remaining;
        if (get_port_scan_progress(&progress, &scanned, &total, &time_remaining) == 0) {
            char progress_msg[256];
            if (time_remaining > 0) {
                snprintf(progress_msg, sizeof(progress_msg), 
                         "Escaneo en progreso: %.1f%% (%d/%d puertos) - %d segundos restantes", 
                         progress, scanned, total, time_remaining);
            } else {
                snprintf(progress_msg, sizeof(progress_msg), 
                         "Escaneo en progreso: %.1f%% (%d/%d puertos)", 
                         progress, scanned, total);
            }
            gui_add_log_entry("PORT_SCANNER", "INFO", progress_msg);
        } else {
            gui_add_log_entry("PORT_SCANNER", "INFO", 
                             "Escaneo en progreso - obteniendo información de estado...");
        }
        return;
    }
    
    pthread_mutex_unlock(&ports_state.state_mutex);
    
    // No hay escaneo en progreso, iniciar uno nuevo
    // Por defecto, realizar escaneo rápido que es lo más útil para la mayoría de usuarios
    gui_add_log_entry("PORT_INTEGRATION", "INFO", 
                     "Iniciando escaneo rápido de puertos solicitado por usuario");
    
    int result = perform_quick_port_scan();
    
    if (result != 0) {
        gui_add_log_entry("PORT_INTEGRATION", "ERROR", 
                         "Error al iniciar escaneo de puertos");
    }
}

int is_gui_port_scan_in_progress(void) {
    return is_port_scan_active();
}

// ============================================================================
// FUNCIONES DE CALLBACK Y NOTIFICACIÓN
// ============================================================================

void on_individual_port_scanned(const PortInfo *port_info, float progress_percentage) {
    if (!port_info) return;
    
    // Este callback se ejecuta frecuentemente durante escaneos largos,
    // así que solo registramos eventos significativos para no saturar el log
    
    static float last_logged_progress = 0.0;
    
    // Registrar progreso cada 10% para escaneos largos
    if (progress_percentage - last_logged_progress >= 10.0) {
        char progress_msg[256];
        snprintf(progress_msg, sizeof(progress_msg), 
                 "Progreso del escaneo: %.1f%% - Puerto %d escaneado", 
                 progress_percentage, port_info->port);
        gui_add_log_entry("PORT_SCANNER", "INFO", progress_msg);
        last_logged_progress = progress_percentage;
    }
    
    // Actualizar estadísticas en la GUI si es un puerto significativo
    if (port_info->is_open) {
        // Obtener estadísticas actuales para actualizar
        int total_open, total_suspicious;
        time_t last_scan;
        if (get_port_statistics_for_gui(&total_open, &total_suspicious, &last_scan) == 0) {
            gui_update_statistics(0, 0, total_open); // 0 para USB y procesos por ahora
        }
    }
}

/**
 * Callback principal que se ejecuta cuando el backend completa un escaneo de puertos.
 * Convierte los resultados del backend a estructuras GUI y actualiza la interfaz.
 * 
 * @param scan_results Array de estructuras PortInfo con los resultados del escaneo
 * @param result_count Número de puertos en scan_results
 * @param scan_cancelled Indica si el escaneo fue cancelado (0=completo, 1=cancelado)
 */
void on_port_scan_completed(const PortInfo *scan_results, int result_count, int scan_cancelled) {
    // LOG MUY VISIBLE PARA DEBUGGING
    char main_msg[256];
    snprintf(main_msg, sizeof(main_msg), 
             "🔥🔥🔥 on_port_scan_completed EJECUTADO: %d puertos, cancelado=%d 🔥🔥🔥", 
             result_count, scan_cancelled);
    gui_add_log_entry("PORT_CALLBACK", "ALERT", main_msg);
      // IMPORTANTE: Limpiar estado PRIMERO para evitar problemas de concurrencia
    pthread_mutex_lock(&ports_state.state_mutex);
    ports_state.scan_active = 0;
    ports_state.scan_cancelled = scan_cancelled;
    int thread_active = (ports_state.scan_thread != 0);
    pthread_mutex_unlock(&ports_state.state_mutex);
    
    // Log del estado para debugging
    char state_msg[256];
    snprintf(state_msg, sizeof(state_msg), 
             "🔧 Estado limpiado: scan_active=0, cancelled=%d, thread_active=%d", 
             scan_cancelled, thread_active);
    gui_add_log_entry("PORT_STATE", "INFO", state_msg);
    
    // ACTUALIZAR GUI UNA SOLA VEZ
    gui_set_scanning_status(FALSE);
    gui_add_log_entry("PORT_CALLBACK", "INFO", "🔄 Estado de GUI actualizado: escaneo finalizado");
    
    if (scan_cancelled) {
        gui_add_log_entry("PORT_SCANNER", "WARNING", 
                         "Escaneo de puertos cancelado por el usuario");
        return;
    }
    
    char completion_msg[512];
    snprintf(completion_msg, sizeof(completion_msg), 
             "🎯 Escaneo de puertos completado exitosamente: %d puertos abiertos encontrados", 
             result_count);
    gui_add_log_entry("PORT_SCANNER", "INFO", completion_msg);
    
    // ACTUALIZAR LA TABLA DE PUERTOS EN LA GUI DIRECTAMENTE
    gui_add_log_entry("GUI_UPDATE", "INFO", "🔄 Iniciando actualización de tabla de puertos en GUI...");
    
    if (scan_results != NULL && result_count > 0) {
        char update_msg[256];
        snprintf(update_msg, sizeof(update_msg), 
                 "📊 Procesando %d puertos para actualización de GUI", result_count);
        gui_add_log_entry("GUI_UPDATE", "INFO", update_msg);
        
        // Convertir y actualizar cada puerto en la GUI DIRECTAMENTE
        for (int i = 0; i < result_count; i++) {
            GUIPort gui_port;
            gui_port.port = scan_results[i].port;
            gui_port.is_suspicious = scan_results[i].is_suspicious;
            
            // Convertir estado
            if (scan_results[i].is_open) {
                strcpy(gui_port.status, "open");
            } else {
                strcpy(gui_port.status, "closed");            }
            
            // Usar nombre de servicio si está disponible
            if (strlen(scan_results[i].service_name) > 0) {
                strcpy(gui_port.service, scan_results[i].service_name);
            } else {
                strcpy(gui_port.service, "unknown");
            }
            
            // Actualizar GUI usando el hilo principal de GTK
            GUIPort *port_copy = malloc(sizeof(GUIPort));
            if (port_copy) {
                *port_copy = gui_port;  // Copiar estructura
                g_main_context_invoke(NULL, (GSourceFunc)gui_update_port_main_thread_wrapper, port_copy);
            } else {
                gui_add_log_entry("GUI_UPDATE", "ERROR", "Error de memoria al crear copia de puerto");
            }        }
        
        char final_msg[256];
        snprintf(final_msg, sizeof(final_msg), 
                 "✅ Actualización de GUI completada: %d puertos procesados", result_count);
        gui_add_log_entry("GUI_UPDATE", "INFO", final_msg);
        
    } else {
        gui_add_log_entry("GUI_UPDATE", "WARNING", 
                         "⚠️ No hay puertos para actualizar en la GUI");
    }
    
    // Actualizar estadísticas finales en la GUI  
    int total_open, total_suspicious;
    time_t last_scan;
    if (get_port_statistics_for_gui(&total_open, &total_suspicious, &last_scan) == 0) {
        gui_update_statistics(0, 0, total_open);
        
        if (total_suspicious > 0) {
            char warning_msg[256];
            snprintf(warning_msg, sizeof(warning_msg), 
                     "⚠️ Atención: %d puerto(s) sospechoso(s) detectado(s)", total_suspicious);
            gui_add_log_entry("PORT_SCANNER", "ALERT", warning_msg);
            
            // Actualizar estado del sistema para reflejar puertos sospechosos
            gui_update_system_status("Puertos Sospechosos Detectados", FALSE);
        } else if (total_open > 0) {
            gui_update_system_status("Sistema Operativo", TRUE);
        }
    }
      // Log de finalización del escaneo
    if (result_count > 0) {
        char completion_msg[256];
        snprintf(completion_msg, sizeof(completion_msg),
                 "Escaneo de puertos completado: %d puertos procesados", result_count);
        gui_add_log_entry("PORT_INTEGRATION", "INFO", completion_msg);
    }
    
    // IMPORTANTE: Limpiar el hilo de escaneo para evitar problemas de estado
    // Esto se hace en el hilo principal de GTK para evitar problemas
    g_timeout_add(100, (GSourceFunc)cleanup_scan_thread_callback, NULL);
      gui_add_log_entry("PORT_CALLBACK", "INFO", "🔄 Callback finalizado - estado limpiado");
}

/**
 * @brief Función callback para limpiar el hilo de escaneo desde el hilo principal
 * 
 * Esta función se ejecuta en el hilo principal de GTK para hacer pthread_join
 * del hilo de escaneo y asegurar que se libere completamente.
 */
static gboolean cleanup_scan_thread_callback(gpointer user_data) {
    (void)user_data; // Suprimir warning de parámetro no usado
    
    pthread_mutex_lock(&ports_state.state_mutex);
    
    // Solo hacer join si hay un hilo activo y el escaneo no está activo
    if (ports_state.scan_active == 0 && ports_state.scan_thread != 0) {
        pthread_t thread_to_join = ports_state.scan_thread;
        ports_state.scan_thread = 0;  // Limpiar antes del join
        pthread_mutex_unlock(&ports_state.state_mutex);
        
        // Hacer join del hilo para limpiarlo completamente
        int join_result = pthread_join(thread_to_join, NULL);
        if (join_result == 0) {
            gui_add_log_entry("PORT_CLEANUP", "INFO", "🧹 Hilo de escaneo limpiado correctamente");
        } else {
            gui_add_log_entry("PORT_CLEANUP", "WARNING", "⚠️ Error al limpiar hilo de escaneo");
        }
    } else {
        pthread_mutex_unlock(&ports_state.state_mutex);
    }
    
    // Retornar FALSE para que este callback se ejecute solo una vez
    return FALSE;
}

void on_suspicious_port_detected(const PortInfo *port_info, const char *threat_description) {
    if (!port_info || !threat_description) return;
    
    char alert_msg[1024];
    snprintf(alert_msg, sizeof(alert_msg), 
             "🚨 PUERTO SOSPECHOSO: Puerto %d (%s) - %s", 
             port_info->port, port_info->service_name, threat_description);
    gui_add_log_entry("PORT_SECURITY", "ALERT", alert_msg);
    
    // Si las notificaciones están habilitadas, mostrar notificación del sistema
    // if (is_notifications_enabled()) {
    //     show_system_notification("MatCom Guard - Puerto Sospechoso", alert_msg);
    // }
    
    // Si las alertas sonoras están habilitadas, reproducir sonido de alerta
    // if (is_sound_alerts_enabled()) {
    //     play_alert_sound();
    // }
}

// ============================================================================
// GESTIÓN DEL CICLO DE VIDA DEL ESCÁNER DE PUERTOS
// ============================================================================

int init_ports_integration(void) {
    pthread_mutex_lock(&ports_state.state_mutex);
    
    if (ports_state.initialized) {
        pthread_mutex_unlock(&ports_state.state_mutex);
        return 0; // Ya está inicializado
    }
    
    // Inicializar estado base
    ports_state.initialized = 1;
    ports_state.scan_active = 0;
    ports_state.should_stop_scan = 0;
    ports_state.last_results = NULL;
    ports_state.last_results_count = 0;
    
    pthread_mutex_unlock(&ports_state.state_mutex);
    
    gui_add_log_entry("PORT_INTEGRATION", "INFO", 
                     "Integración de escáner de puertos inicializada");
    
    return 0;
}

int start_port_scan(const PortScanConfig *pconfig) {
    if (!pconfig) {
        gui_add_log_entry("PORT_INTEGRATION", "ERROR", 
                         "Configuración de escaneo nula");
        return -1;
    }
    
    pthread_mutex_lock(&ports_state.state_mutex);
    
    if (!ports_state.initialized) {
        pthread_mutex_unlock(&ports_state.state_mutex);
        gui_add_log_entry("PORT_INTEGRATION", "ERROR", 
                         "Integración no inicializada");
        return -1;
    }
    
    if (ports_state.scan_active) {
        pthread_mutex_unlock(&ports_state.state_mutex);
        gui_add_log_entry("PORT_INTEGRATION", "WARNING", 
                         "Ya hay un escaneo de puertos en progreso");
        return -1;
    }
    
    // Validar configuración de escaneo
    if (pconfig->start_port < 1 || pconfig->end_port > 65535 || 
        pconfig->start_port > pconfig->end_port) {
        pthread_mutex_unlock(&ports_state.state_mutex);
        gui_add_log_entry("PORT_INTEGRATION", "ERROR", 
                         "Rango de puertos inválido");
        return -1;
    }
    
    // Configurar para nuevo escaneo
    ports_state.current_config = *pconfig;
    ports_state.scan_active = 1;
    ports_state.scan_cancelled = 0;
    ports_state.should_stop_scan = 0;
    ports_state.ports_completed = 0;
    ports_state.last_progress_percentage = 0.0;
    
    pthread_mutex_unlock(&ports_state.state_mutex);
    
    // Crear hilo de escaneo
    // Nota: pasamos una copia de la configuración al hilo
    PortScanConfig *thread_config = malloc(sizeof(PortScanConfig));
    *thread_config = *pconfig;
    
    int result = pthread_create(&ports_state.scan_thread, NULL, 
                               port_scanning_thread_function, thread_config);
    
    if (result != 0) {
        pthread_mutex_lock(&ports_state.state_mutex);
        ports_state.scan_active = 0;
        pthread_mutex_unlock(&ports_state.state_mutex);
        
        free(thread_config);
        gui_add_log_entry("PORT_INTEGRATION", "ERROR", 
                         "Error al crear hilo de escaneo de puertos");
        return -1;
    }
    
    char start_msg[256];
    snprintf(start_msg, sizeof(start_msg), 
             "Escaneo de puertos iniciado: rango %d-%d (%d puertos total)", 
             pconfig->start_port, pconfig->end_port, 
             pconfig->end_port - pconfig->start_port + 1);
    gui_add_log_entry("PORT_INTEGRATION", "INFO", start_msg);
    
    // Actualizar estado de la GUI para mostrar que está escaneando
    gui_set_scanning_status(TRUE);
    
    return 0;
}

int cancel_port_scan(void) {
    pthread_mutex_lock(&ports_state.state_mutex);
    
    if (!ports_state.scan_active) {
        pthread_mutex_unlock(&ports_state.state_mutex);
        gui_add_log_entry("PORT_INTEGRATION", "INFO", 
                         "No hay escaneo activo para cancelar");
        return -1;
    }
    
    // Señalar al hilo que debe detenerse
    ports_state.should_stop_scan = 1;
    ports_state.scan_cancelled = 1;
    
    pthread_mutex_unlock(&ports_state.state_mutex);
    
    gui_add_log_entry("PORT_INTEGRATION", "INFO", 
                     "Solicitando cancelación de escaneo de puertos...");
    
    // Esperar a que el hilo termine
    pthread_join(ports_state.scan_thread, NULL);
    
    gui_add_log_entry("PORT_INTEGRATION", "INFO", 
                     "Escaneo de puertos cancelado exitosamente");
    
    gui_set_scanning_status(FALSE);
    
    return 0;
}

int is_port_scan_active(void) {
    pthread_mutex_lock(&ports_state.state_mutex);
    int active = ports_state.scan_active;
    pthread_mutex_unlock(&ports_state.state_mutex);
    return active;
}

int get_port_scan_progress(float *progress_percentage, int *ports_scanned, 
                          int *total_ports, int *estimated_time_remaining) {
    if (!progress_percentage || !ports_scanned || !total_ports || !estimated_time_remaining) {
        return -1;
    }
    
    pthread_mutex_lock(&ports_state.state_mutex);
    
    if (!ports_state.scan_active) {
        pthread_mutex_unlock(&ports_state.state_mutex);
        return -1; // No hay escaneo activo
    }
    
    *progress_percentage = ports_state.last_progress_percentage;
    *ports_scanned = ports_state.ports_completed;
    *total_ports = ports_state.total_ports_to_scan;
    
    // Calcular tiempo estimado restante basándose en el progreso actual
    time_t current_time = time(NULL);
    time_t elapsed_time = current_time - ports_state.scan_start_time;
    
    if (ports_state.ports_completed > 0 && elapsed_time > 0) {
        // Calcular velocidad promedio (puertos por segundo)
        float ports_per_second = (float)ports_state.ports_completed / elapsed_time;
        int remaining_ports = ports_state.total_ports_to_scan - ports_state.ports_completed;
        
        if (ports_per_second > 0) {
            *estimated_time_remaining = (int)(remaining_ports / ports_per_second);
        } else {
            *estimated_time_remaining = -1; // No se puede estimar
        }
    } else {
        *estimated_time_remaining = -1; // Insuficientes datos para estimación
    }
    
    pthread_mutex_unlock(&ports_state.state_mutex);
    return 0;
}

// ============================================================================
// FUNCIONES DE LIMPIEZA Y GESTIÓN DE RECURSOS
// ============================================================================

/**
 * Función para limpiar todos los recursos del módulo de puertos de forma segura
 * Debe ser llamada antes de cerrar la aplicación
 */
void cleanup_ports_integration(void) {
    gui_add_log_entry("PORT_INTEGRATION", "INFO", "Iniciando limpieza de recursos de puertos...");
    
    // Detener cualquier escaneo en progreso
    pthread_mutex_lock(&ports_state.state_mutex);
    if (ports_state.scan_active) {
        ports_state.should_stop_scan = 1;
        pthread_mutex_unlock(&ports_state.state_mutex);
        
        // Esperar a que termine el hilo de escaneo
        pthread_join(ports_state.scan_thread, NULL);
        gui_add_log_entry("PORT_INTEGRATION", "INFO", "Escaneo en progreso detenido");
    } else {
        pthread_mutex_unlock(&ports_state.state_mutex);
    }
    
    // Limpiar resultados almacenados
    pthread_mutex_lock(&ports_state.state_mutex);
    if (ports_state.last_results) {
        free(ports_state.last_results);
        ports_state.last_results = NULL;
        ports_state.last_results_count = 0;
    }
    
    // Resetear estado
    ports_state.initialized = 0;
    ports_state.scan_active = 0;
    ports_state.scan_cancelled = 0;
    pthread_mutex_unlock(&ports_state.state_mutex);
    
    gui_add_log_entry("PORT_INTEGRATION", "INFO", "Limpieza de recursos de puertos completada");
}

/**
 * Función para validar el estado del sistema antes de operaciones críticas
 */
static int validate_ports_state(void) {
    if (!ports_state.initialized) {
        gui_add_log_entry("PORT_INTEGRATION", "ERROR", 
                         "Sistema de puertos no inicializado");
        return -1;
    }
    
    // Verificar que no hay corrupción de memoria básica
    if (ports_state.last_results_count < 0 || 
        ports_state.total_ports_to_scan < 0 ||
        ports_state.ports_completed < 0) {
        gui_add_log_entry("PORT_INTEGRATION", "ERROR", 
                         "Estado de puertos corrupto detectado");
        return -1;
    }
      return 0;
}

// ============================================================================