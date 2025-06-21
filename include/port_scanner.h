/*
 * ===============================================================================
 * HEADER ARCHIVO port_scanner.h - MatCom Guard Sistema de Seguridad USB
 * ===============================================================================
 * 
 * ¿QUÉ ES UN HEADER EN C?
 * Un archivo .h (header) es como un "índice de un libro" que contiene:
 * - DECLARACIONES de funciones (qué hacen, pero no CÓMO lo hacen)
 * - DEFINICIONES de estructuras de datos
 * - CONSTANTES y macros
 * 
 * Es como un "contrato" que dice "estas funciones existen y puedes usarlas"
 * pero la implementación real está en el archivo .c correspondiente.
 */

#ifndef PORT_SCANNER_H    // Guard: previene inclusión múltiple
#define PORT_SCANNER_H    // (como cerrar la puerta para que no entre dos veces)

// ===============================================================================
// LIBRERÍAS NECESARIAS (includes)
// ===============================================================================

#include <time.h>         // Para manejar fechas y horas
#include <stdint.h>       // Para tipos de datos de tamaño fijo
#include <sys/types.h>    // Para tipos de datos del sistema (off_t, mode_t, uid_t, gid_t)
#include <sys/stat.h>     // Para constantes de permisos y funciones de archivos

// ===============================================================================
// CONSTANTES DEL SISTEMA (como las "reglas del reino")
// ===============================================================================

#define MAX_PATH_LENGTH 1024      // Longitud máxima de una ruta de archivo
#define MAX_DEVICES 32            // Máximo número de dispositivos USB a monitorear
#define MAX_FILES_PER_DEVICE 8192 // Máximo archivos por dispositivo USB
#define SHA256_HASH_SIZE 32       // Tamaño del hash SHA-256 (256 bits = 32 bytes)
#define DEVICE_NAME_SIZE 256      // Tamaño máximo del nombre de dispositivo
#define DEFAULT_CHANGE_THRESHOLD 0.10  // 10% - umbral de cambios sospechosos

// ===============================================================================
// ESTRUCTURAS DE DATOS (como "formularios" para organizar información)
// ===============================================================================

/*
 * ESTRUCTURA: FileInfo
 * ¿Qué es? Es como una "ficha" que guarda toda la información importante
 * de un archivo para poder detectar si alguien lo modificó.
 * 
 * Es como un "pasaporte" de cada archivo que contiene:
 * - Su "huella dactilar" (hash SHA-256)
 * - Su tamaño
 * - Cuándo fue modificado por última vez
 * - Sus permisos de acceso
 * - Quién es el dueño
 */
typedef struct {
    char file_path[MAX_PATH_LENGTH];           // Ruta completa del archivo
    unsigned char hash[SHA256_HASH_SIZE];      // "Huella dactilar" SHA-256 del archivo
    off_t file_size;                           // Tamaño del archivo en bytes
    time_t last_modified;                      // Fecha de última modificación
    mode_t permissions;                        // Permisos del archivo (rwx)
    uid_t owner_id;                           // ID del propietario del archivo
    gid_t group_id;                           // ID del grupo del archivo
} FileInfo;

/*
 * ESTRUCTURA: USBDevice
 * ¿Qué es? Es como un "expediente completo" de cada dispositivo USB
 * que se conecta al sistema. Guarda toda la información necesaria
 * para monitorearlo y detectar actividades sospechosas.
 */
typedef struct {
    char device_name[DEVICE_NAME_SIZE];        // Nombre del dispositivo (ej: "USB Kingston")
    char mount_point[MAX_PATH_LENGTH];         // Dónde está montado (ej: "/media/usb")
    time_t first_seen;                         // Cuándo se detectó por primera vez
    time_t last_scan;                          // Cuándo fue el último escaneo
    int total_files;                           // Total de archivos en el dispositivo
    int suspicious_changes;                    // Número de cambios sospechosos detectados
    int is_suspicious;                         // 1=sospechoso, 0=limpio
    FileInfo baseline_files[MAX_FILES_PER_DEVICE]; // "Base de datos" de archivos originales
    int baseline_count;                        // Cuántos archivos hay en la baseline
} USBDevice;

/*
 * ESTRUCTURA: ScanResult
 * ¿Qué es? Es como un "reporte" que se genera después de escanear
 * un dispositivo USB. Contiene todos los resultados del análisis.
 */
typedef struct {
    int files_scanned;           // Cuántos archivos se escanearon
    int files_added;             // Cuántos archivos nuevos se encontraron
    int files_deleted;           // Cuántos archivos fueron eliminados
    int files_modified;          // Cuántos archivos fueron modificados
    int suspicious_files;        // Cuántos archivos tienen cambios sospechosos
    double change_percentage;    // Porcentaje de archivos que cambiaron
    int threat_level;           // Nivel de amenaza: 0=limpio, 1=sospechoso, 2=peligroso
    char detailed_report[2048]; // Reporte detallado en texto
} ScanResult;

// ===============================================================================
// DECLARACIONES DE FUNCIONES (el "menú" de funciones disponibles)
// ===============================================================================

/*
 * FUNCIÓN: inicializar_monitor_usb
 * ¿Qué hace? Prepara el sistema para comenzar a monitorear dispositivos USB.
 * Es como "encender las cámaras de seguridad" del reino.
 * 
 * Parámetros: ninguno
 * Retorna: 1 si se inicializó correctamente, 0 si hubo error
 */
int inicializar_monitor_usb(void);

/*
 * FUNCIÓN: detectar_dispositivos_usb
 * ¿Qué hace? Busca todos los dispositivos USB conectados al sistema.
 * Es como "revisar todas las puertas de entrada" del reino.
 * 
 * Parámetros:
 *   - mount_directory: directorio donde se montan los USB (ej: "/media")
 *   - dispositivos: array donde se guardarán los dispositivos encontrados
 *   - max_dispositivos: máximo número de dispositivos a detectar
 * 
 * Retorna: número de dispositivos USB encontrados
 */
int detectar_dispositivos_usb(const char* mount_directory, 
                             USBDevice* dispositivos, 
                             int max_dispositivos);

/*
 * FUNCIÓN: crear_baseline_dispositivo
 * ¿Qué hace? Crea la "fotografía inicial" de todos los archivos en un USB.
 * Es como tomar una foto de todos los habitantes del reino cuando llegan.
 * Esta "fotografía" sirve para detectar cambios posteriores.
 * 
 * Parámetros:
 *   - dispositivo: puntero al dispositivo USB a procesar
 * 
 * Retorna: 1 si se creó correctamente, 0 si hubo error
 */
int crear_baseline_dispositivo(USBDevice* dispositivo);

/*
 * FUNCIÓN: escanear_dispositivo_cambios
 * ¿Qué hace? Compara el estado actual del USB con la "fotografía inicial".
 * Es como revisar si alguien nuevo entró al reino o si alguien cambió.
 * 
 * Parámetros:
 *   - dispositivo: dispositivo USB a escanear
 *   - resultado: estructura donde se guardarán los resultados del escaneo
 *   - umbral_cambios: porcentaje de cambios permitidos antes de alertar
 * 
 * Retorna: 1 si se detectaron cambios sospechosos, 0 si todo está normal
 */
int escanear_dispositivo_cambios(USBDevice* dispositivo, 
                                ScanResult* resultado, 
                                double umbral_cambios);

/*
 * FUNCIÓN: detectar_cambios_sospechosos
 * ¿Qué hace? Analiza UN archivo específico para ver si tiene cambios raros.
 * Es como "interrogar" a un habitante para ver si es sospechoso.
 * 
 * Parámetros:
 *   - archivo_original: información del archivo en la baseline
 *   - archivo_actual: información actual del archivo
 * 
 * Retorna: número que indica qué tipo de cambio sospechoso (0=normal)
 */
int detectar_cambios_sospechosos(const FileInfo* archivo_original, 
                                 const FileInfo* archivo_actual);

/*
 * FUNCIÓN: calcular_hash_archivo
 * ¿Qué hace? Calcula la "huella dactilar" (hash SHA-256) de un archivo.
 * Es como tomar las huellas dactilares de una persona para identificarla.
 * 
 * Parámetros:
 *   - ruta_archivo: ruta completa al archivo
 *   - hash_resultado: array donde se guardará el hash calculado
 * 
 * Retorna: 1 si se calculó correctamente, 0 si hubo error
 */
int calcular_hash_archivo(const char* ruta_archivo, 
                         unsigned char* hash_resultado);

/*
 * FUNCIÓN: obtener_info_archivo
 * ¿Qué hace? Recopila toda la información importante de un archivo
 * (tamaño, permisos, propietario, etc.).
 * 
 * Parámetros:
 *   - ruta_archivo: ruta completa al archivo
 *   - info: estructura donde se guardará la información
 * 
 * Retorna: 1 si se obtuvo la información, 0 si hubo error
 */
int obtener_info_archivo(const char* ruta_archivo, FileInfo* info);

/*
 * FUNCIÓN: generar_alerta_seguridad
 * ¿Qué hace? Genera una alerta cuando se detecta algo sospechoso.
 * Es como "tocar las campanas de alarma" del reino.
 * 
 * Parámetros:
 *   - dispositivo: dispositivo donde se detectó el problema
 *   - tipo_alerta: tipo de amenaza detectada
 *   - mensaje: descripción detallada del problema
 * 
 * Retorna: nada (void)
 */
void generar_alerta_seguridad(const USBDevice* dispositivo, 
                             const char* tipo_alerta, 
                             const char* mensaje);

/*
 * FUNCIÓN: limpiar_monitor_usb
 * ¿Qué hace? Libera toda la memoria y recursos utilizados.
 * Es como "cerrar y limpiar" todas las instalaciones de seguridad.
 * 
 * Parámetros: ninguno
 * Retorna: nada (void)
 */
void limpiar_monitor_usb(void);

/*
 * FUNCIÓN: obtener_estadisticas_dispositivo
 * ¿Qué hace? Proporciona un resumen estadístico de un dispositivo.
 * 
 * Parámetros:
 *   - dispositivo: dispositivo del cual obtener estadísticas
 *   - resultado: estructura donde se guardarán las estadísticas
 * 
 * Retorna: nada (void)
 */
void obtener_estadisticas_dispositivo(const USBDevice* dispositivo, 
                                     ScanResult* resultado);

// ===============================================================================
// ENUMS Y CONSTANTES PARA TIPOS DE AMENAZAS
// ===============================================================================

/*
 * ENUM: TipoAmenaza
 * ¿Qué es? Una lista de los diferentes tipos de amenazas que podemos detectar.
 * Es como una "lista de crímenes" que pueden cometer los archivos sospechosos.
 */
typedef enum {
    AMENAZA_NINGUNA = 0,           // No hay amenaza
    AMENAZA_CRECIMIENTO_INUSUAL = 1,   // Archivo creció demasiado rápido
    AMENAZA_REPLICACION = 2,           // Se están creando copias de archivos
    AMENAZA_CAMBIO_EXTENSION = 4,      // Archivo cambió de extensión (.txt a .exe)
    AMENAZA_PERMISOS_PELIGROSOS = 8,   // Permisos cambiaron a 777 (todos pueden hacer todo)
    AMENAZA_TIMESTAMP_ANOMALO = 16,    // Fecha de modificación es extraña
    AMENAZA_CAMBIO_PROPIETARIO = 32    // El dueño del archivo cambió
} TipoAmenaza;

/*
 * CONSTANTES para niveles de amenaza
 */
#define NIVEL_LIMPIO 0        // Todo normal
#define NIVEL_SOSPECHOSO 1    // Algo raro, pero no crítico
#define NIVEL_PELIGROSO 2     // Amenaza seria detectada

#endif /* PORT_SCANNER_H */

/*
 * ===============================================================================
 * RESUMEN DE ESTE ARCHIVO:
 * ===============================================================================
 * 
 * Este header define la "interfaz" del sistema de monitoreo USB. Es como
 * el "manual de instrucciones" que le dice a otros archivos qué funciones
 * están disponibles y cómo usarlas.
 * 
 * Las estructuras de datos (FileInfo, USBDevice, ScanResult) son como
 * "formularios" que organizan la información de manera ordenada.
 * 
 * Las funciones declaradas aquí son como "herramientas" que se pueden
 * usar para detectar amenazas en dispositivos USB. La implementación
 * real de estas funciones está en port_scanner.c
 */