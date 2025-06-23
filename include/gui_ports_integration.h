#ifndef GUI_PORTS_INTEGRATION_H
#define GUI_PORTS_INTEGRATION_H

#include "gui.h"
#include "port_scanner.h"
#include "gui_backend_adapters.h"
#include <pthread.h>

// ============================================================================
// CONFIGURACIÓN Y CONSTANTES DEL ESCÁNER DE PUERTOS
// ============================================================================

// Esta enumeración define los diferentes tipos de escaneo que podemos realizar
// Cada tipo tiene diferentes características de tiempo y profundidad
typedef enum {
    SCAN_TYPE_QUICK,        // Escaneo rápido de puertos comunes (1-1024)
    SCAN_TYPE_FULL,         // Escaneo completo de todos los puertos (1-65535)
    SCAN_TYPE_CUSTOM        // Escaneo de rango personalizado especificado por usuario
} PortScanType;

// Estructura que define los parámetros de un escaneo de puertos
// Esta estructura nos permite configurar escaneos con diferentes características
typedef struct {
    PortScanType scan_type;     // Tipo de escaneo a realizar
    int start_port;             // Puerto inicial del rango
    int end_port;               // Puerto final del rango
    int timeout_seconds;        // Timeout por puerto individual
    int concurrent_scans;       // Número de puertos a escanear simultáneamente
    int report_progress;        // Si debe reportar progreso durante el escaneo
} PortScanConfig;

// ============================================================================
// GESTIÓN DEL CICLO DE VIDA DEL ESCÁNER DE PUERTOS
// ============================================================================

/**
 * @brief Inicializa la integración entre el escáner de puertos y la GUI
 * 
 * Esta función establece la infraestructura necesaria para coordinar entre
 * el backend de escaneo de puertos y la interfaz gráfica. El escáner de puertos
 * presenta desafíos únicos porque los escaneos pueden ser extremadamente largos
 * (hasta varias horas para un escaneo completo) y necesitamos proporcionar
 * feedback de progreso continuo sin bloquear la interfaz de usuario.
 * 
 * La inicialización configura:
 * 1. Estructuras de datos para rastrear escaneos en progreso
 * 2. Sistema de threading para escaneos no bloqueantes
 * 3. Mecanismos de cancelación para escaneos largos
 * 4. Cache de resultados para evitar re-escaneos innecesarios
 * 
 * @return int 0 si la inicialización es exitosa, -1 si hay error
 */
int init_ports_integration(void);

/**
 * @brief Inicia un escaneo de puertos con configuración especificada
 * 
 * Esta función es el punto de entrada principal para todos los tipos de escaneo
 * de puertos. Maneja la coordinación entre la solicitud de la GUI y la ejecución
 * del backend, incluyendo la gestión de threading y el reporte de progreso.
 * 
 * El proceso que maneja incluye:
 * 1. Validación de la configuración de escaneo
 * 2. Verificación de que no hay otro escaneo en progreso
 * 3. Creación de un hilo separado para el escaneo
 * 4. Configuración de callbacks para reporte de progreso
 * 5. Inicio del proceso de escaneo en el backend
 * 
 * @param config Configuración del escaneo a realizar
 * @return int 0 si el escaneo se inició correctamente, -1 si error
 */
int start_port_scan(const PortScanConfig *config);

/**
 * @brief Cancela un escaneo de puertos en progreso
 * 
 * Esta función permite al usuario cancelar escaneos largos que están en progreso.
 * Maneja la terminación segura del hilo de escaneo y la limpieza de recursos.
 * 
 * @return int 0 si la cancelación fue exitosa, -1 si error o no había escaneo activo
 */
int cancel_port_scan(void);

/**
 * @brief Verifica si hay un escaneo de puertos en progreso
 * 
 * @return int 1 si hay escaneo activo, 0 si no hay escaneo activo
 */
int is_port_scan_active(void);

/**
 * @brief Obtiene el progreso actual del escaneo en progreso
 * 
 * Esta función permite que la GUI muestre una barra de progreso precisa
 * durante escaneos largos. Es especialmente importante para escaneos completos
 * que pueden tomar horas.
 * 
 * @param progress_percentage Puntero donde almacenar el porcentaje completado (0-100)
 * @param ports_scanned Puntero donde almacenar el número de puertos ya escaneados
 * @param total_ports Puntero donde almacenar el total de puertos a escanear
 * @param estimated_time_remaining Puntero donde almacenar tiempo estimado restante en segundos
 * @return int 0 si es exitoso, -1 si no hay escaneo activo o error
 */
int get_port_scan_progress(float *progress_percentage, int *ports_scanned, 
                          int *total_ports, int *estimated_time_remaining);

/**
 * @brief Limpia recursos y finaliza la integración de puertos
 * 
 * Esta función debe ser llamada antes de cerrar la aplicación para asegurar
 * que todos los hilos de escaneo terminen correctamente y se libere toda
 * la memoria asignada. Es crítica para prevenir memory leaks y segfaults.
 */
void cleanup_ports_integration(void);

// ============================================================================
// FUNCIONES DE ESCANEO PREDEFINIDAS
// ============================================================================

/**
 * @brief Realiza un escaneo rápido de puertos comunes (1-1024)
 * 
 * Esta función implementa el escaneo más común que los usuarios necesitan:
 * verificar los puertos estándar que típicamente usan los servicios del sistema.
 * Es rápido (generalmente completa en menos de 2 minutos) y detecta la mayoría
 * de los servicios importantes.
 * 
 * @return int 0 si el escaneo se inició correctamente, -1 si error
 */
int perform_quick_port_scan(void);

/**
 * @brief Realiza un escaneo completo de todos los puertos posibles (1-65535)
 * 
 * Esta función inicia el escaneo más exhaustivo posible. ADVERTENCIA: puede
 * tomar varias horas en completarse dependiendo de la configuración de red
 * y el número de puertos abiertos encontrados.
 * 
 * @return int 0 si el escaneo se inició correctamente, -1 si error
 */
int perform_full_port_scan(void);

/**
 * @brief Realiza un escaneo de un rango personalizado de puertos
 * 
 * Esta función permite al usuario especificar exactamente qué rango de puertos
 * desea escanear, proporcionando un balance entre velocidad y cobertura.
 * 
 * @param start_port Puerto inicial del rango (1-65535)
 * @param end_port Puerto final del rango (1-65535)
 * @return int 0 si el escaneo se inició correctamente, -1 si error
 */
int perform_custom_port_scan(int start_port, int end_port);

// ============================================================================
// GESTIÓN DE RESULTADOS Y ESTADÍSTICAS
// ============================================================================

/**
 * @brief Obtiene los resultados del último escaneo completado
 * 
 * Esta función proporciona acceso a los resultados detallados del escaneo
 * más reciente. Los resultados incluyen información sobre cada puerto
 * encontrado, servicios detectados, y evaluación de seguridad.
 * 
 * @param result_buffer Puntero donde almacenar los resultados
 * @param buffer_size Tamaño del buffer de resultados
 * @return int Número de puertos en los resultados, -1 si error
 */
int get_last_scan_results(PortInfo **result_buffer, int *buffer_size);

/**
 * @brief Obtiene estadísticas agregadas de puertos para la GUI
 * 
 * Esta función recopila estadísticas resumidas que son útiles para mostrar
 * en el panel principal de la aplicación, como el número total de puertos
 * abiertos y cuántos son considerados sospechosos.
 * 
 * @param total_open Puntero donde almacenar el total de puertos abiertos
 * @param total_suspicious Puntero donde almacenar el total de puertos sospechosos
 * @param last_scan_time Puntero donde almacenar el timestamp del último escaneo
 * @return int 0 si es exitoso, -1 si error
 */
int get_port_statistics_for_gui(int *total_open, int *total_suspicious, time_t *last_scan_time);

/**
 * @brief Genera un reporte detallado de los resultados del escaneo
 * 
 * Esta función crea un reporte comprensivo que incluye todos los puertos
 * encontrados, análisis de seguridad, y recomendaciones. Es útil para
 * auditorías de seguridad y documentación.
 * 
 * @param report_filename Nombre del archivo donde guardar el reporte
 * @param include_closed_ports Si incluir puertos cerrados en el reporte
 * @return int 0 si el reporte se generó exitosamente, -1 si error
 */
int generate_port_scan_report(const char *report_filename, int include_closed_ports);

// ============================================================================
// FUNCIONES DE COMPATIBILIDAD CON GUI EXISTENTE
// ============================================================================

/**
 * @brief Función adaptadora para el callback ScanPortsCallback de la GUI
 * 
 * Esta función permite que el sistema de callbacks existente de la GUI
 * funcione con el nuevo backend de escaneo de puertos sin modificar el código
 * de la interfaz. Actúa como un wrapper inteligente que determina el tipo
 * de escaneo más apropiado basándose en el contexto y la configuración.
 * 
 * El comportamiento específico de esta función:
 * 1. Si no hay configuración específica, realiza un escaneo rápido
 * 2. Si hay un escaneo en progreso, muestra el progreso actual
 * 3. Proporciona feedback inmediato y continuo durante el escaneo
 * 4. Actualiza automáticamente la GUI con los resultados
 */
void gui_compatible_scan_ports(void);

/**
 * @brief Verifica si hay un escaneo de puertos en progreso para la GUI
 * 
 * Esta función complementa el sistema de threading existente de la GUI
 * permitiendo que los botones se deshabiliten correctamente durante
 * escaneos largos y se muestren indicadores de progreso apropiados.
 * 
 * @return int 1 si hay escaneo en progreso, 0 si no
 */
int is_gui_port_scan_in_progress(void);

// ============================================================================
// FUNCIONES DE CALLBACK Y NOTIFICACIÓN
// ============================================================================

/**
 * @brief Callback ejecutado cuando se completa el escaneo de un puerto individual
 * 
 * Esta función se ejecuta cada vez que el backend completa el escaneo de un
 * puerto específico. Permite actualizar la GUI en tiempo real durante escaneos
 * largos, proporcionando feedback continuo al usuario.
 * 
 * @param port_info Información del puerto recién escaneado
 * @param progress_info Información de progreso actual del escaneo
 */
void on_individual_port_scanned(const PortInfo *port_info, float progress_percentage);

/**
 * @brief Callback ejecutado cuando se completa todo el escaneo
 * 
 * Esta función se ejecuta cuando el escaneo completo ha terminado, ya sea
 * por completarse exitosamente o por cancelación del usuario.
 * 
 * @param scan_results Array con todos los resultados del escaneo
 * @param result_count Número de puertos en los resultados
 * @param scan_cancelled Si el escaneo fue cancelado antes de completarse
 */
void on_port_scan_completed(const PortInfo *scan_results, int result_count, int scan_cancelled);

/**
 * @brief Callback ejecutado cuando se detecta un puerto sospechoso
 * 
 * Esta función se ejecuta inmediatamente cuando se encuentra un puerto que
 * podría representar un riesgo de seguridad, permitiendo alertas en tiempo real
 * incluso durante escaneos largos.
 * 
 * @param port_info Información del puerto sospechoso detectado
 * @param threat_description Descripción del tipo de amenaza potencial
 */
void on_suspicious_port_detected(const PortInfo *port_info, const char *threat_description);

// ============================================================================
#endif // GUI_PORTS_INTEGRATION_H