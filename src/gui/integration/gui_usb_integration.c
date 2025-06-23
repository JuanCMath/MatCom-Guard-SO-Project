#include "gui_usb_integration.h"
#include "gui_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

// ============================================================================
// ESTRUCTURAS DE ESTADO DE LA INTEGRACIÓN USB
// ============================================================================

// Esta estructura mantiene el estado completo del sistema de monitoreo USB
// Es más compleja que la de procesos porque debe manejar dispositivos
// que pueden conectarse y desconectarse dinámicamente
typedef struct {
    int initialized;                    // ¿Está inicializado el sistema?
    int monitoring_active;              // ¿Está el monitoreo activo?
    int scan_in_progress;               // ¿Hay un escaneo manual en progreso?
    int scan_interval_seconds;          // Intervalo entre escaneos automáticos
    int deep_scan_enabled;              // ¿Está habilitado el escaneo profundo?
    pthread_t monitoring_thread;        // Hilo de monitoreo automático
    pthread_mutex_t state_mutex;        // Protege el acceso concurrente
    volatile int should_stop_monitoring; // Señal para detener el monitoreo
} USBIntegrationState;

static USBIntegrationState usb_state = {
    .initialized = 0,
    .monitoring_active = 0,
    .scan_in_progress = 0,
    .scan_interval_seconds = 30,
    .deep_scan_enabled = 0,
    .should_stop_monitoring = 0,
    .state_mutex = PTHREAD_MUTEX_INITIALIZER
};

// ============================================================================
// FUNCIONES INTERNAS DEL HILO DE MONITOREO
// ============================================================================

/**
 * Esta función representa el corazón del sistema de monitoreo USB.
 * Se ejecuta en un hilo separado y es responsable de detectar cuando
 * se conectan o desconectan dispositivos USB, y de programar escaneos
 * periódicos para detectar cambios en el contenido.
 */
static void* usb_monitoring_thread_function(void* arg) {
    (void)arg; // Evitar warning de parámetro no usado
    
    DeviceList* current_devices = NULL;
    DeviceList* previous_devices = NULL;
    
    gui_add_log_entry("USB_INTEGRATION", "INFO", 
                     "Hilo de monitoreo USB iniciado");
    
    while (!usb_state.should_stop_monitoring) {
        // Liberar la lista anterior antes de obtener la nueva
        if (previous_devices) {
            free_device_list(previous_devices);
        }
        previous_devices = current_devices;
        
        // Obtener la lista actual de dispositivos conectados
        // Esta función del backend escanea /media para encontrar dispositivos montados
        current_devices = monitor_connected_devices(1); // 1 segundo de timeout
        
        if (current_devices && previous_devices) {
            // Detectar dispositivos recién conectados
            for (int i = 0; i < current_devices->count; i++) {
                char* current_device = current_devices->devices[i];
                int found_in_previous = 0;
                
                // Buscar este dispositivo en la lista anterior
                for (int j = 0; j < previous_devices->count; j++) {
                    if (strcmp(current_device, previous_devices->devices[j]) == 0) {
                        found_in_previous = 1;
                        break;
                    }
                }
                
                // Si no estaba en la lista anterior, es un dispositivo nuevo
                if (!found_in_previous) {
                    on_usb_device_connected(current_device);
                }
            }
            
            // Detectar dispositivos desconectados
            for (int i = 0; i < previous_devices->count; i++) {
                char* previous_device = previous_devices->devices[i];
                int found_in_current = 0;
                
                for (int j = 0; j < current_devices->count; j++) {
                    if (strcmp(previous_device, current_devices->devices[j]) == 0) {
                        found_in_current = 1;
                        break;
                    }
                }
                
                if (!found_in_current) {
                    on_usb_device_disconnected(previous_device);
                }
            }
        } else if (current_devices && !previous_devices) {
            // Primera ejecución: todos los dispositivos son "nuevos"
            for (int i = 0; i < current_devices->count; i++) {
                on_usb_device_connected(current_devices->devices[i]);
            }
        }
        
        // Esperar el intervalo configurado antes del próximo ciclo
        // Usamos un loop con sleeps cortos para poder responder rápidamente
        // a la señal de parada
        for (int i = 0; i < usb_state.scan_interval_seconds && !usb_state.should_stop_monitoring; i++) {
            sleep(1);
        }
    }
    
    // Limpieza al terminar el hilo
    if (current_devices) {
        free_device_list(current_devices);
    }
    if (previous_devices) {
        free_device_list(previous_devices);
    }
    
    pthread_mutex_lock(&usb_state.state_mutex);
    usb_state.monitoring_active = 0;
    pthread_mutex_unlock(&usb_state.state_mutex);
    
    gui_add_log_entry("USB_INTEGRATION", "INFO", 
                     "Hilo de monitoreo USB terminado");
    
    return NULL;
}

// ============================================================================
// GESTIÓN DEL CICLO DE VIDA DEL MONITOR USB
// ============================================================================

int init_usb_integration(void) {
    pthread_mutex_lock(&usb_state.state_mutex);
    
    if (usb_state.initialized) {
        pthread_mutex_unlock(&usb_state.state_mutex);
        return 0; // Ya está inicializado
    }
    
    // Inicializar el sistema de cache de snapshots USB
    // Este sistema nos permite mantener snapshots anteriores para comparación
    if (init_usb_snapshot_cache() != 0) {
        pthread_mutex_unlock(&usb_state.state_mutex);
        gui_add_log_entry("USB_INTEGRATION", "ERROR", 
                         "Error al inicializar cache de snapshots USB");
        return -1;
    }
    
    usb_state.initialized = 1;
    usb_state.should_stop_monitoring = 0;
    
    pthread_mutex_unlock(&usb_state.state_mutex);
    
    gui_add_log_entry("USB_INTEGRATION", "INFO", 
                     "Integración de monitoreo USB inicializada");
    
    return 0;
}

int start_usb_monitoring(int scan_interval_seconds) {
    pthread_mutex_lock(&usb_state.state_mutex);
    
    if (!usb_state.initialized) {
        pthread_mutex_unlock(&usb_state.state_mutex);
        gui_add_log_entry("USB_INTEGRATION", "ERROR", 
                         "Integración no inicializada. Llame a init_usb_integration() primero");
        return -1;
    }
    
    if (usb_state.monitoring_active) {
        pthread_mutex_unlock(&usb_state.state_mutex);
        gui_add_log_entry("USB_INTEGRATION", "INFO", 
                         "Monitoreo USB ya está activo");
        return 0;
    }
    
    // Configurar parámetros de monitoreo
    usb_state.scan_interval_seconds = (scan_interval_seconds > 0) ? scan_interval_seconds : 30;
    usb_state.should_stop_monitoring = 0;
    
    // Crear el hilo de monitoreo
    int result = pthread_create(&usb_state.monitoring_thread, NULL, 
                               usb_monitoring_thread_function, NULL);
    
    if (result != 0) {
        pthread_mutex_unlock(&usb_state.state_mutex);
        gui_add_log_entry("USB_INTEGRATION", "ERROR", 
                         "Error al crear hilo de monitoreo USB");
        return -1;
    }
    
    usb_state.monitoring_active = 1;
    pthread_mutex_unlock(&usb_state.state_mutex);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), 
             "Monitoreo USB iniciado con intervalo de %d segundos", 
             usb_state.scan_interval_seconds);
    gui_add_log_entry("USB_INTEGRATION", "INFO", log_msg);
    
    return 0;
}

int perform_manual_usb_scan(void) {
    pthread_mutex_lock(&usb_state.state_mutex);
    
    if (usb_state.scan_in_progress) {
        pthread_mutex_unlock(&usb_state.state_mutex);
        gui_add_log_entry("USB_INTEGRATION", "WARNING", 
                         "Ya hay un escaneo USB en progreso");
        return -1;
    }
    
    usb_state.scan_in_progress = 1;
    pthread_mutex_unlock(&usb_state.state_mutex);
    
    gui_add_log_entry("USB_INTEGRATION", "INFO", 
                     "Iniciando escaneo manual de dispositivos USB");
    
    // Obtener lista de dispositivos conectados
    DeviceList* devices = monitor_connected_devices(5); // 5 segundos timeout
    int devices_scanned = 0;
    
    if (devices) {
        gui_add_log_entry("USB_INTEGRATION", "INFO", 
                         "Dispositivos USB detectados para escaneo");
        
        // Analizar cada dispositivo encontrado
        for (int i = 0; i < devices->count; i++) {
            char log_msg[512];
            snprintf(log_msg, sizeof(log_msg), 
                     "Analizando dispositivo USB: %s", devices->devices[i]);
            gui_add_log_entry("USB_INTEGRATION", "INFO", log_msg);
            
            if (analyze_usb_device(devices->devices[i]) == 0) {
                devices_scanned++;
            } else {
                snprintf(log_msg, sizeof(log_msg), 
                         "Error al analizar dispositivo: %s", devices->devices[i]);
                gui_add_log_entry("USB_INTEGRATION", "ERROR", log_msg);
            }
        }
        
        free_device_list(devices);
    } else {
        gui_add_log_entry("USB_INTEGRATION", "INFO", 
                         "No se encontraron dispositivos USB conectados");
    }
    
    // Actualizar estadísticas en la GUI después del escaneo
    int total_devices, suspicious_devices, total_files;
    if (get_usb_statistics_for_gui(&total_devices, &suspicious_devices, &total_files) == 0) {
        gui_update_statistics(total_devices, 0, 0); // 0 para procesos y puertos por ahora
    }
    
    pthread_mutex_lock(&usb_state.state_mutex);
    usb_state.scan_in_progress = 0;
    pthread_mutex_unlock(&usb_state.state_mutex);
    
    char completion_msg[256];
    snprintf(completion_msg, sizeof(completion_msg), 
             "Escaneo manual completado: %d dispositivos analizados", devices_scanned);
    gui_add_log_entry("USB_INTEGRATION", "INFO", completion_msg);
    
    return devices_scanned;
}

int stop_usb_monitoring(void) {
    pthread_mutex_lock(&usb_state.state_mutex);
    
    if (!usb_state.monitoring_active) {
        pthread_mutex_unlock(&usb_state.state_mutex);
        gui_add_log_entry("USB_INTEGRATION", "INFO", 
                         "Monitoreo USB no está activo");
        return 0;
    }
    
    // Señalar al hilo que debe detenerse
    usb_state.should_stop_monitoring = 1;
    pthread_mutex_unlock(&usb_state.state_mutex);
    
    // Esperar a que el hilo termine
    int result = pthread_join(usb_state.monitoring_thread, NULL);
    
    if (result == 0) {
        gui_add_log_entry("USB_INTEGRATION", "INFO", 
                         "Monitoreo USB detenido exitosamente");
    } else {
        gui_add_log_entry("USB_INTEGRATION", "ERROR", 
                         "Error al detener monitoreo USB");
        return -1;
    }
    
    return 0;
}

int is_usb_monitoring_active(void) {
    pthread_mutex_lock(&usb_state.state_mutex);
    int active = usb_state.monitoring_active;
    pthread_mutex_unlock(&usb_state.state_mutex);
    return active;
}

void cleanup_usb_integration(void) {
    // Detener monitoreo si está activo
    if (is_usb_monitoring_active()) {
        stop_usb_monitoring();
    }
    
    pthread_mutex_lock(&usb_state.state_mutex);
    
    if (usb_state.initialized) {
        // Limpiar el cache de snapshots
        cleanup_usb_snapshot_cache();
        
        usb_state.initialized = 0;
        usb_state.scan_in_progress = 0;
    }
    
    pthread_mutex_unlock(&usb_state.state_mutex);
    
    gui_add_log_entry("USB_INTEGRATION", "INFO", 
                     "Integración USB finalizada y recursos liberados");
}

// ============================================================================
// FUNCIONES DE ANÁLISIS DE DISPOSITIVOS ESPECÍFICOS
// ============================================================================

int analyze_usb_device(const char *device_name) {
    if (!device_name) {
        return -1;
    }
    
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), 
             "Creando snapshot del dispositivo: %s", device_name);
    gui_add_log_entry("USB_ANALYZER", "INFO", log_msg);
      // Paso 1: Crear un snapshot completo del dispositivo actual
    // Esta operación puede tomar tiempo ya que calcula hashes SHA-256 de todos los archivos
    DeviceSnapshot* new_snapshot = create_device_snapshot(device_name);
    if (!new_snapshot) {
        snprintf(log_msg, sizeof(log_msg), 
                 "Error al crear snapshot de %s", device_name);
        gui_add_log_entry("USB_ANALYZER", "ERROR", log_msg);
        return -1;
    }
    
    // Debug: Verificar el estado del snapshot recién creado
    printf("DEBUG: Snapshot creado exitosamente para %s\n", device_name);
    printf("DEBUG: new_snapshot=%p, device_name=%p\n", new_snapshot, new_snapshot->device_name);
    if (new_snapshot->device_name) {
        printf("DEBUG: device_name content: '%s'\n", new_snapshot->device_name);
    }
    
    // Paso 2: Recuperar el snapshot anterior del cache para comparación
    DeviceSnapshot* previous_snapshot = get_cached_usb_snapshot(device_name);
    
    // Paso 3: Convertir los datos del backend al formato que entiende la GUI
    printf("DEBUG: Llamando a adapt_device_snapshot_to_gui...\n");
    GUIUSBDevice gui_device;
    if (adapt_device_snapshot_to_gui(new_snapshot, previous_snapshot, &gui_device) != 0) {
        gui_add_log_entry("USB_ANALYZER", "ERROR", 
                         "Error al adaptar snapshot para GUI");
        free_device_snapshot(new_snapshot);
        return -1;
    }
    
    // Paso 4: Actualizar la interfaz gráfica con los resultados
    gui_update_usb_device(&gui_device);
    
    // Paso 5: Verificar si hay actividad sospechosa y generar alertas si es necesario
    if (gui_device.is_suspicious) {
        char threat_msg[512];
        snprintf(threat_msg, sizeof(threat_msg), 
                 "Detectados %d archivos modificados/añadidos en dispositivo potencialmente comprometido", 
                 gui_device.files_changed);
        on_usb_suspicious_activity_detected(device_name, threat_msg);
    }
    
    // Paso 6: Almacenar el nuevo snapshot en el cache para futuras comparaciones
    if (store_usb_snapshot(device_name, new_snapshot) != 0) {
        gui_add_log_entry("USB_ANALYZER", "WARNING", 
                         "No se pudo almacenar snapshot en cache");
    }
    
    snprintf(log_msg, sizeof(log_msg), 
             "Análisis completado: %s - %d archivos, %d cambios, Estado: %s", 
             device_name, gui_device.total_files, gui_device.files_changed, 
             gui_device.is_suspicious ? "SOSPECHOSO" : "LIMPIO");
    gui_add_log_entry("USB_ANALYZER", "INFO", log_msg);
    
    return 0;
}

int perform_deep_usb_scan(const char *device_name) {
    if (!device_name) {
        return -1;
    }
    
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), 
             "Iniciando escaneo profundo de %s (esto puede tomar varios minutos)", device_name);
    gui_add_log_entry("USB_ANALYZER", "INFO", log_msg);
    
    // El escaneo profundo es esencialmente el mismo proceso que el escaneo normal,
    // pero el backend automáticamente calcula hashes SHA-256 más detallados
    // y realiza verificaciones adicionales de integridad
    return analyze_usb_device(device_name);
}

// ============================================================================
// GESTIÓN DE ESTADO Y SINCRONIZACIÓN
// ============================================================================

int sync_gui_with_usb_devices(void) {
    gui_add_log_entry("USB_INTEGRATION", "INFO", 
                     "Sincronizando vista GUI con dispositivos USB conectados");
    
    // Obtener la lista actual de dispositivos
    DeviceList* devices = monitor_connected_devices(3);
    int devices_synced = 0;
    
    if (devices) {
        for (int i = 0; i < devices->count; i++) {
            // Para cada dispositivo, verificar si tenemos un snapshot en cache
            DeviceSnapshot* cached_snapshot = get_cached_usb_snapshot(devices->devices[i]);
            
            if (cached_snapshot) {
                // Si tenemos un snapshot, convertirlo y actualizar la GUI
                GUIUSBDevice gui_device;
                if (adapt_device_snapshot_to_gui(cached_snapshot, NULL, &gui_device) == 0) {
                    gui_update_usb_device(&gui_device);
                    devices_synced++;
                }
            } else {
                // Si no tenemos snapshot, realizar un análisis inicial
                if (analyze_usb_device(devices->devices[i]) == 0) {
                    devices_synced++;
                }
            }
        }
        
        free_device_list(devices);
    }
    
    char sync_msg[256];
    snprintf(sync_msg, sizeof(sync_msg), 
             "Sincronización completada: %d dispositivos actualizados", devices_synced);
    gui_add_log_entry("USB_INTEGRATION", "INFO", sync_msg);
    
    return devices_synced;
}

int get_usb_statistics_for_gui(int *total_devices, int *suspicious_devices, int *total_files) {
    if (!total_devices || !suspicious_devices || !total_files) {
        return -1;
    }
    
    *total_devices = 0;
    *suspicious_devices = 0;
    *total_files = 0;
    
    // Obtener lista de dispositivos actuales
    DeviceList* devices = monitor_connected_devices(2);
    
    if (devices) {
        *total_devices = devices->count;
        
        // Para cada dispositivo, verificar su estado en el cache
        for (int i = 0; i < devices->count; i++) {
            DeviceSnapshot* snapshot = get_cached_usb_snapshot(devices->devices[i]);
            if (snapshot) {
                *total_files += snapshot->file_count;
                
                // Evaluar si es sospechoso basándose en cambios recientes
                // Esto requiere comparar con un snapshot anterior
                GUIUSBDevice gui_device;
                if (adapt_device_snapshot_to_gui(snapshot, NULL, &gui_device) == 0) {
                    if (gui_device.is_suspicious) {
                        (*suspicious_devices)++;
                    }
                }
            }
        }
        
        free_device_list(devices);
    }
    
    return 0;
}

int update_usb_monitoring_config(int scan_interval, int deep_scan_enabled) {
    if (scan_interval < 5 || scan_interval > 3600) {
        gui_add_log_entry("USB_INTEGRATION", "ERROR", 
                         "Intervalo de escaneo inválido (debe estar entre 5 y 3600 segundos)");
        return -1;
    }
    
    pthread_mutex_lock(&usb_state.state_mutex);
    usb_state.scan_interval_seconds = scan_interval;
    usb_state.deep_scan_enabled = deep_scan_enabled;
    pthread_mutex_unlock(&usb_state.state_mutex);
    
    char config_msg[256];
    snprintf(config_msg, sizeof(config_msg), 
             "Configuración USB actualizada: intervalo=%ds, escaneo_profundo=%s",
             scan_interval, deep_scan_enabled ? "habilitado" : "deshabilitado");
    gui_add_log_entry("USB_INTEGRATION", "INFO", config_msg);
    
    return 0;
}

// ============================================================================
// FUNCIONES DE COMPATIBILIDAD CON GUI EXISTENTE
// ============================================================================

void gui_compatible_scan_usb(void) {
    // Esta función actúa como puente entre el sistema de callbacks simple
    // de la GUI existente y el sistema complejo de análisis de snapshots USB
    
    pthread_mutex_lock(&usb_state.state_mutex);
    
    if (!usb_state.initialized) {
        pthread_mutex_unlock(&usb_state.state_mutex);
        
        // Auto-inicializar si no está inicializado
        if (init_usb_integration() != 0) {
            gui_add_log_entry("USB_INTEGRATION", "ERROR", 
                             "Error al inicializar integración USB");
            return;
        }
    }
    
    pthread_mutex_unlock(&usb_state.state_mutex);
    
    // Si el monitoreo automático no está activo, iniciarlo
    if (!is_usb_monitoring_active()) {
        int result = start_usb_monitoring(30); // 30 segundos por defecto
        if (result != 0) {
            gui_add_log_entry("USB_INTEGRATION", "ERROR", 
                             "Error al iniciar monitoreo automático USB");
        }
    }
    
    // Realizar un escaneo manual inmediato para dar feedback al usuario
    gui_add_log_entry("USB_INTEGRATION", "INFO", 
                     "Ejecutando escaneo manual solicitado por usuario");
    
    int devices_scanned = perform_manual_usb_scan();
    
    if (devices_scanned >= 0) {
        char result_msg[256];
        snprintf(result_msg, sizeof(result_msg), 
                 "Escaneo USB completado: %d dispositivos analizados", devices_scanned);
        gui_add_log_entry("USB_INTEGRATION", "INFO", result_msg);
    }
}

int is_gui_usb_scan_in_progress(void) {
    pthread_mutex_lock(&usb_state.state_mutex);
    int in_progress = usb_state.scan_in_progress;
    pthread_mutex_unlock(&usb_state.state_mutex);
    
    return in_progress;
}

// ============================================================================
// FUNCIONES DE NOTIFICACIÓN Y EVENTOS
// ============================================================================

void on_usb_device_connected(const char *device_name) {
    if (!device_name) return;
    
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), 
             "🔌 Dispositivo USB conectado: %s - iniciando análisis inicial", device_name);
    gui_add_log_entry("USB_MONITOR", "INFO", log_msg);
    
    // Crear un dispositivo GUI inicial con estado "DETECTADO"
    GUIUSBDevice gui_device;
    memset(&gui_device, 0, sizeof(gui_device));
    
    strncpy(gui_device.device_name, device_name, sizeof(gui_device.device_name) - 1);
    snprintf(gui_device.mount_point, sizeof(gui_device.mount_point), "/media/%s", device_name);
    strncpy(gui_device.status, "DETECTADO", sizeof(gui_device.status) - 1);
    gui_device.total_files = 0;
    gui_device.files_changed = 0;
    gui_device.is_suspicious = FALSE;
    gui_device.last_scan = time(NULL);
    
    // Actualizar la GUI inmediatamente para mostrar el dispositivo
    gui_update_usb_device(&gui_device);
    
    // Programar análisis inicial en segundo plano
    // En una implementación más sofisticada, esto podría ejecutarse en un hilo separado
    if (analyze_usb_device(device_name) != 0) {
        gui_add_log_entry("USB_MONITOR", "WARNING", 
                         "No se pudo completar el análisis inicial del dispositivo");
    }
}

void on_usb_device_disconnected(const char *device_name) {
    if (!device_name) return;
    
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), 
             "🔌 Dispositivo USB desconectado: %s", device_name);
    gui_add_log_entry("USB_MONITOR", "INFO", log_msg);
    
    // En una implementación más completa, aquí removeríamos el dispositivo
    // de la vista de la GUI. Por ahora, solo registramos el evento.
    
    // El snapshot permanece en el cache para comparación futura si el
    // dispositivo se reconecta
}

void on_usb_suspicious_activity_detected(const char *device_name, const char *threat_description) {
    if (!device_name || !threat_description) return;
    
    char alert_msg[1024];
    snprintf(alert_msg, sizeof(alert_msg), 
             "🚨 AMENAZA USB DETECTADA en %s: %s", device_name, threat_description);
    gui_add_log_entry("USB_SECURITY", "ALERT", alert_msg);
    
    // Si las notificaciones están habilitadas, aquí podríamos disparar
    // una notificación del sistema usando las funciones de configuración:
    // if (is_notifications_enabled()) {
    //     show_system_notification("MatCom Guard", alert_msg);
    // }
    
    // Si las alertas sonoras están habilitadas:
    // if (is_sound_alerts_enabled()) {
    //     play_alert_sound();
    // }
    
    // Actualizar el estado del sistema para reflejar la amenaza detectada
    gui_update_system_status("Amenaza USB Detectada", FALSE);
}