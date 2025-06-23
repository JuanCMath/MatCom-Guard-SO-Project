#ifndef GUI_SYSTEM_COORDINATOR_H
#define GUI_SYSTEM_COORDINATOR_H

#include "gui.h"
#include "gui_process_integration.h"
#include "gui_usb_integration.h"
#include "gui_ports_integration.h"
#include <pthread.h>
#include <time.h>

// ============================================================================
// ENUMERACIONES DE ESTADO DEL SISTEMA
// ============================================================================

/**
 * Esta enumeración define los diferentes niveles de seguridad que puede
 * tener el sistema en su conjunto. Es el resultado de evaluar la información
 * agregada de todos los módulos de monitoreo.
 */
typedef enum {
    SECURITY_LEVEL_SAFE,        // Sistema completamente seguro
    SECURITY_LEVEL_MONITORING,  // Monitoreo activo, sin amenazas detectadas
    SECURITY_LEVEL_WARNING,     // Actividad sospechosa detectada
    SECURITY_LEVEL_CRITICAL,    // Amenazas activas confirmadas
    SECURITY_LEVEL_UNKNOWN      // Estado no determinado o error
} SystemSecurityLevel;

/**
 * Estado de actividad de cada módulo del sistema. Esto nos permite
 * rastrear qué componentes están funcionando y cuáles pueden tener problemas.
 */
typedef enum {
    MODULE_STATUS_INACTIVE,     // Módulo no iniciado
    MODULE_STATUS_ACTIVE,       // Funcionando normalmente
    MODULE_STATUS_SCANNING,     // Realizando operación activa
    MODULE_STATUS_ERROR,        // Error detectado
    MODULE_STATUS_MAINTENANCE   // En mantenimiento o actualización
} ModuleStatus;

// ============================================================================
// ESTRUCTURAS DE ESTADO GLOBAL
// ============================================================================

/**
 * Esta estructura mantiene el estado consolidado de todo el sistema.
 * Es la "fuente única de verdad" para el estado global de MatCom Guard.
 * 
 * Piensa en esta estructura como el tablero de control de una central nuclear:
 * debe mostrar de manera precisa y en tiempo real el estado de todos los
 * sistemas críticos, y debe ser actualizada de manera thread-safe por
 * múltiples subsistemas que corren simultáneamente.
 */
typedef struct {
    // Estado de seguridad general del sistema
    SystemSecurityLevel security_level;
    time_t last_security_evaluation;
    char security_description[512];
    
    // Estado de actividad de cada módulo principal
    ModuleStatus process_module_status;
    ModuleStatus usb_module_status;
    ModuleStatus ports_module_status;
    
    // Estadísticas agregadas de todos los módulos
    struct {
        // Estadísticas de procesos
        int total_processes_monitored;
        int suspicious_processes;
        int processes_exceeding_cpu_threshold;
        int processes_exceeding_memory_threshold;
        time_t last_process_scan;
        
        // Estadísticas de dispositivos USB
        int total_usb_devices;
        int suspicious_usb_devices;
        int total_files_monitored;
        int files_with_changes;
        time_t last_usb_scan;
        
        // Estadísticas de puertos de red
        int total_ports_scanned;
        int open_ports_found;
        int suspicious_ports;
        time_t last_port_scan;
        
        // Estadísticas temporales para cálculo de tendencias
        time_t statistics_last_updated;
    } aggregate_stats;
    
    // Información de rendimiento del sistema coordinador
    struct {
        time_t system_start_time;
        int coordination_cycles_completed;
        int gui_updates_sent;
        float average_coordination_time_ms;
    } performance_metrics;
    
    // Control de threading y sincronización
    pthread_mutex_t state_mutex;
    pthread_cond_t state_changed_condition;
    volatile int shutdown_requested;
    
} SystemGlobalState;

// ============================================================================
// FUNCIONES DE INICIALIZACIÓN Y GESTIÓN DEL COORDINADOR
// ============================================================================

/**
 * @brief Inicializa el sistema coordinador global
 * 
 * Esta función establece la infraestructura para coordinar todos los módulos
 * del sistema. Crea el estado global compartido, inicializa los mecanismos
 * de sincronización, y establece las conexiones entre todos los subsistemas.
 * 
 * El coordinador actúa como el "sistema nervioso central" de MatCom Guard,
 * recibiendo información de todos los módulos y proporcionando una vista
 * unificada del estado del sistema a la interfaz de usuario.
 * 
 * @return int 0 si la inicialización es exitosa, -1 si hay error
 */
int init_system_coordinator(void);

/**
 * @brief Inicia el hilo coordinador que mantiene sincronizado el estado global
 * 
 * Este hilo es responsable de recopilar periódicamente información de todos
 * los módulos, evaluar el estado de seguridad general del sistema, y actualizar
 * la interfaz de usuario con información consolidada.
 * 
 * El hilo coordinador implementa un patrón de "heartbeat" donde verifica
 * periódicamente el estado de todos los subsistemas y detecta problemas
 * de manera proactiva.
 * 
 * @param update_interval_seconds Intervalo en segundos entre actualizaciones
 * @return int 0 si el coordinador se inició correctamente, -1 si error
 */
int start_system_coordinator(int update_interval_seconds);

/**
 * @brief Detiene el sistema coordinador de manera ordenada
 * 
 * Esta función asegura que el hilo coordinador termine de manera segura,
 * completando cualquier operación en progreso y liberando todos los recursos.
 * 
 * @return int 0 si se detuvo correctamente, -1 si error
 */
int stop_system_coordinator(void);

/**
 * @brief Limpia todos los recursos del sistema coordinador
 * 
 * Esta función debe llamarse al cerrar la aplicación para asegurar que
 * todos los recursos del coordinador sean liberados correctamente.
 */
void cleanup_system_coordinator(void);

// ============================================================================
// FUNCIONES DE EVALUACIÓN Y ACTUALIZACIÓN DE ESTADO
// ============================================================================

/**
 * @brief Evalúa el nivel de seguridad general del sistema
 * 
 * Esta función implementa la lógica central de evaluación de seguridad,
 * analizando información de todos los módulos para determinar el nivel
 * de riesgo general del sistema.
 * 
 * La evaluación considera factores como:
 * - Número y severidad de amenazas detectadas en cada módulo
 * - Tendencias temporales de actividad sospechosa
 * - Correlaciones entre eventos de diferentes módulos
 * - Estado de salud de los sistemas de monitoreo
 * 
 * @return SystemSecurityLevel Nivel de seguridad calculado
 */
SystemSecurityLevel evaluate_system_security_level(void);

/**
 * @brief Actualiza las estadísticas agregadas del sistema
 * 
 * Esta función recopila información actualizada de todos los módulos
 * y calcula estadísticas agregadas que son útiles para mostrar en
 * el panel principal de la aplicación.
 * 
 * @return int 0 si la actualización fue exitosa, -1 si error
 */
int update_aggregate_statistics(void);

/**
 * @brief Detecta correlaciones entre eventos de diferentes módulos
 * 
 * Esta función implementa análisis cruzado para detectar patrones
 * de ataque que podrían no ser obvios cuando se miran los módulos
 * individualmente. Por ejemplo, actividad sospechosa simultánea
 * en procesos y dispositivos USB podría indicar un ataque coordinado.
 * 
 * @return int Número de correlaciones detectadas, -1 si error
 */
int detect_cross_module_correlations(void);

/**
 * @brief Actualiza la interfaz de usuario con el estado global actual
 * 
 * Esta función toma el estado global consolidado y actualiza todos
 * los elementos relevantes de la interfaz de usuario, incluyendo
 * el panel de estadísticas, indicadores de estado, y alertas.
 * 
 * @return int 0 si la actualización fue exitosa, -1 si error
 */
int update_gui_with_global_state(void);

// ============================================================================
// FUNCIONES DE ACCESO AL ESTADO GLOBAL (THREAD-SAFE)
// ============================================================================

/**
 * @brief Obtiene una copia thread-safe del estado global actual
 * 
 * Esta función proporciona acceso seguro al estado global desde cualquier
 * hilo, creando una copia que puede ser utilizada sin preocuparse por
 * modificaciones concurrentes.
 * 
 * @param state_copy Puntero donde almacenar la copia del estado
 * @return int 0 si es exitoso, -1 si error
 */
int get_global_state_copy(SystemGlobalState *state_copy);

/**
 * @brief Obtiene el nivel de seguridad actual del sistema
 * 
 * @return SystemSecurityLevel Nivel de seguridad actual
 */
SystemSecurityLevel get_current_security_level(void);

/**
 * @brief Obtiene estadísticas consolidadas para el panel principal de la GUI
 * 
 * Esta función proporciona los números clave que se muestran en el
 * panel de estadísticas principal de la aplicación.
 * 
 * @param total_devices Puntero donde almacenar total de dispositivos USB
 * @param total_processes Puntero donde almacenar total de procesos monitoreados
 * @param total_open_ports Puntero donde almacenar total de puertos abiertos
 * @param security_alerts Puntero donde almacenar número de alertas de seguridad activas
 * @return int 0 si es exitoso, -1 si error
 */
int get_consolidated_statistics(int *total_devices, int *total_processes, 
                               int *total_open_ports, int *security_alerts);

// ============================================================================
// FUNCIONES DE NOTIFICACIÓN DE EVENTOS ENTRE MÓDULOS
// ============================================================================

/**
 * @brief Notifica al coordinador sobre un evento de seguridad
 * 
 * Esta función permite que cualquier módulo notifique al coordinador
 * sobre eventos de seguridad que podrían afectar la evaluación global
 * del sistema. El coordinador puede entonces evaluar si este evento
 * cambia el nivel de seguridad general o requiere notificaciones adicionales.
 * 
 * @param source_module Módulo que genera la notificación
 * @param event_severity Severidad del evento (1-10, donde 10 es crítico)
 * @param event_description Descripción del evento
 * @return int 0 si la notificación fue procesada, -1 si error
 */
int notify_security_event(const char *source_module, int event_severity, 
                         const char *event_description);

/**
 * @brief Notifica cambios en el estado de un módulo
 * 
 * Esta función permite que los módulos notifiquen al coordinador cuando
 * cambia su estado operacional (por ejemplo, cuando inician o terminan
 * operaciones de escaneo).
 * 
 * @param module_name Nombre del módulo que reporta el cambio
 * @param new_status Nuevo estado del módulo
 * @param status_description Descripción adicional del estado
 * @return int 0 si la notificación fue procesada, -1 si error
 */
int notify_module_status_change(const char *module_name, ModuleStatus new_status,
                               const char *status_description);

// ============================================================================
// FUNCIONES DE CONFIGURACIÓN Y AJUSTES DINÁMICOS
// ============================================================================

/**
 * @brief Actualiza la configuración del coordinador dinámicamente
 * 
 * Esta función permite cambiar parámetros del coordinador sin necesidad
 * de reiniciar el sistema, incluyendo intervalos de actualización,
 * umbrales de evaluación de seguridad, y otros parámetros operacionales.
 * 
 * @param update_interval Nuevo intervalo de actualización en segundos
 * @param security_evaluation_sensitivity Sensibilidad de evaluación (1-10)
 * @return int 0 si la configuración fue actualizada, -1 si error
 */
int update_coordinator_configuration(int update_interval, int security_evaluation_sensitivity);

/**
 * @brief Solicita una evaluación inmediata del estado del sistema
 * 
 * Esta función permite forzar una reevaluación inmediata del estado
 * global del sistema, útil después de eventos significativos o cuando
 * el usuario solicita información actualizada.
 * 
 * @return int 0 si la evaluación fue completada, -1 si error
 */
int request_immediate_system_evaluation(void);

#endif // GUI_SYSTEM_COORDINATOR_H