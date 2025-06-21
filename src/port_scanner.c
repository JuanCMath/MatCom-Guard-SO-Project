/*
 * ===============================================================================
 * ARCHIVO port_scanner.c - MatCom Guard Sistema de Seguridad USB
 * ===============================================================================
 * 
 * ¿QUÉ ES UN ARCHIVO .c EN C?
 * Un archivo .c contiene la IMPLEMENTACIÓN real de las funciones.
 * Es como el "motor" que hace funcionar todo lo que se declara en el .h
 * 
 * Aquí es donde escribimos el código que REALMENTE hace el trabajo:
 * - Escanear dispositivos USB
 * - Calcular hashes de archivos
 * - Detectar cambios sospechosos
 * - Generar alertas de seguridad
 */

// ===============================================================================
// INCLUDES (librerías necesarias)
// ===============================================================================

#include "port_scanner.h"     // Nuestro propio header con las declaraciones
#include <stdio.h>           // Para printf, fopen, etc.
#include <stdlib.h>          // Para malloc, free, exit, etc.
#include <string.h>          // Para strcmp, strcpy, strlen, etc.
#include <dirent.h>          // Para opendir, readdir (leer directorios)
#include <unistd.h>          // Para access, sleep
#include <sys/stat.h>        // Para stat (obtener info de archivos)
#include <sys/types.h>       // Para tipos de datos del sistema
#include <errno.h>           // Para códigos de error
#include <openssl/sha.h>     // Para calcular hash SHA-256

// ===============================================================================
// VARIABLES GLOBALES (como la "memoria" del sistema)
// ===============================================================================

/*
 * ¿Qué son las variables globales?
 * Son variables que pueden ser accedidas desde cualquier función del archivo.
 * Es como tener una "pizarra común" donde todas las funciones pueden escribir
 * y leer información.
 */

static USBDevice dispositivos_monitoreados[MAX_DEVICES];  // Array de dispositivos conectados
static int numero_dispositivos = 0;                       // Cuántos dispositivos tenemos
static int sistema_inicializado = 0;                      // ¿Está el sistema iniciado?

// ===============================================================================
// FUNCIÓN: inicializar_monitor_usb
// Prepara el sistema para comenzar el monitoreo
// ===============================================================================

int inicializar_monitor_usb(void) {
    /*
     * ¿Qué hace esta función?
     * Es como "encender" el sistema de seguridad. Prepara todo lo necesario
     * para empezar a monitorear los dispositivos USB.
     */
    
    printf("🔧 Inicializando sistema de monitoreo USB MatCom Guard...\n");
    
    // Limpiar la memoria donde guardamos los dispositivos
    // Es como "borrar la pizarra" para empezar limpio
    memset(dispositivos_monitoreados, 0, sizeof(dispositivos_monitoreados));
    numero_dispositivos = 0;
    
    // Marcar que el sistema ya está iniciado
    sistema_inicializado = 1;
    
    printf("✅ Sistema de monitoreo USB inicializado correctamente.\n");
    return 1;  // Retorna 1 = éxito
}

// ===============================================================================
// FUNCIÓN: detectar_dispositivos_usb
// Busca todos los dispositivos USB conectados
// ===============================================================================

int detectar_dispositivos_usb(const char* mount_directory, 
                             USBDevice* dispositivos, 
                             int max_dispositivos) {
    /*
     * ¿Qué hace esta función?
     * Es como un "guardia" que revisa todas las "puertas de entrada" (directorios
     * de montaje) para ver qué dispositivos USB están conectados.
     * 
     * Los pasos son:
     * 1. Abrir el directorio donde se montan los USB
     * 2. Leer cada subdirectorio (cada uno es un dispositivo)
     * 3. Guardar la información de cada dispositivo encontrado
     */
    
    printf("🔍 Buscando dispositivos USB en: %s\n", mount_directory);
    
    // Verificar que el sistema esté inicializado
    if (!sistema_inicializado) {
        printf("❌ Error: Sistema no inicializado. Llama primero a inicializar_monitor_usb()\n");
        return 0;
    }
    
    // Abrir el directorio de montaje
    // DIR* es como un "libro" que nos permite leer los contenidos de un directorio
    DIR* directorio = opendir(mount_directory);
    if (directorio == NULL) {
        printf("❌ Error: No se puede abrir el directorio %s\n", mount_directory);
        return 0;
    }
    
    int dispositivos_encontrados = 0;
    struct dirent* entrada;  // Cada "entrada" es un archivo o directorio
    
    // Leer cada entrada del directorio
    // Es como "hojear" cada página del "libro" del directorio
    while ((entrada = readdir(directorio)) != NULL && dispositivos_encontrados < max_dispositivos) {
        
        // Saltar las entradas especiales "." y ".."
        // Estas son entradas especiales que no nos interesan
        if (strcmp(entrada->d_name, ".") == 0 || strcmp(entrada->d_name, "..") == 0) {
            continue;
        }
        
        // Construir la ruta completa del dispositivo
        char ruta_completa[MAX_PATH_LENGTH];
        snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s", mount_directory, entrada->d_name);
        
        // Verificar si es realmente un directorio (dispositivo montado)
        struct stat info_archivo;
        if (stat(ruta_completa, &info_archivo) == 0 && S_ISDIR(info_archivo.st_mode)) {
            
            // ¡Encontramos un dispositivo USB!
            printf("📱 Dispositivo USB detectado: %s\n", entrada->d_name);
            
            // Guardar la información del dispositivo
            USBDevice* dispositivo = &dispositivos[dispositivos_encontrados];
            
            // Copiar el nombre del dispositivo
            strncpy(dispositivo->device_name, entrada->d_name, DEVICE_NAME_SIZE - 1);
            dispositivo->device_name[DEVICE_NAME_SIZE - 1] = '\0';  // Asegurar terminación
            
            // Copiar la ruta de montaje
            strncpy(dispositivo->mount_point, ruta_completa, MAX_PATH_LENGTH - 1);
            dispositivo->mount_point[MAX_PATH_LENGTH - 1] = '\0';
            
            // Establecer timestamps
            dispositivo->first_seen = time(NULL);    // Momento actual
            dispositivo->last_scan = 0;              // Aún no escaneado
            
            // Inicializar contadores
            dispositivo->total_files = 0;
            dispositivo->suspicious_changes = 0;
            dispositivo->is_suspicious = 0;           // Inicialmente limpio
            dispositivo->baseline_count = 0;
            
            dispositivos_encontrados++;
        }
    }
    
    closedir(directorio);  // Cerrar el "libro" del directorio
    
    printf("✅ Se detectaron %d dispositivos USB\n", dispositivos_encontrados);
    return dispositivos_encontrados;
}

// ===============================================================================
// FUNCIÓN: calcular_hash_archivo
// Calcula la "huella dactilar" SHA-256 de un archivo
// ===============================================================================

int calcular_hash_archivo(const char* ruta_archivo, unsigned char* hash_resultado) {
    /*
     * ¿Qué hace esta función?
     * Es como tomar las "huellas dactilares" de un archivo. El hash SHA-256
     * es un número único que identifica el contenido exacto del archivo.
     * Si el archivo cambia aunque sea 1 bit, el hash será completamente diferente.
     * 
     * Es la base de nuestra detección de cambios.
     */
    
    // Abrir el archivo para lectura binaria
    FILE* archivo = fopen(ruta_archivo, "rb");
    if (archivo == NULL) {
        printf("❌ Error: No se puede abrir el archivo %s\n", ruta_archivo);
        return 0;
    }
    
    // Inicializar el contexto SHA-256
    // Es como "preparar la máquina de huellas dactilares"
    SHA256_CTX contexto_sha;
    SHA256_Init(&contexto_sha);
    
    // Leer el archivo en bloques y procesar cada bloque
    unsigned char buffer[4096];  // Buffer para leer bloques de 4KB
    size_t bytes_leidos;
    
    // Leer el archivo bloque por bloque
    while ((bytes_leidos = fread(buffer, 1, sizeof(buffer), archivo)) > 0) {
        // Procesar este bloque con SHA-256
        SHA256_Update(&contexto_sha, buffer, bytes_leidos);
    }
    
    // Finalizar el cálculo del hash
    SHA256_Final(hash_resultado, &contexto_sha);
    
    fclose(archivo);
    
    return 1;  // Éxito
}

// ===============================================================================
// FUNCIÓN: obtener_info_archivo
// Recopila toda la información importante de un archivo
// ===============================================================================

int obtener_info_archivo(const char* ruta_archivo, FileInfo* info) {
    /*
     * ¿Qué hace esta función?
     * Es como "interrogar" a un archivo para obtener toda su información:
     * - ¿Cuándo fue modificado?
     * - ¿Qué tamaño tiene?
     * - ¿Quién es el dueño?
     * - ¿Qué permisos tiene?
     * - ¿Cuál es su huella dactilar (hash)?
     */
    
    // Obtener información del sistema operativo sobre el archivo
    struct stat stat_archivo;
    if (stat(ruta_archivo, &stat_archivo) != 0) {
        printf("❌ Error: No se puede obtener información de %s\n", ruta_archivo);
        return 0;
    }
    
    // Guardar la ruta del archivo
    strncpy(info->file_path, ruta_archivo, MAX_PATH_LENGTH - 1);
    info->file_path[MAX_PATH_LENGTH - 1] = '\0';
    
    // Guardar toda la información del archivo
    info->file_size = stat_archivo.st_size;        // Tamaño en bytes
    info->last_modified = stat_archivo.st_mtime;   // Fecha de última modificación
    info->permissions = stat_archivo.st_mode;      // Permisos (rwx)
    info->owner_id = stat_archivo.st_uid;          // ID del propietario
    info->group_id = stat_archivo.st_gid;          // ID del grupo
    
    // Calcular el hash SHA-256 del archivo
    if (!calcular_hash_archivo(ruta_archivo, info->hash)) {
        printf("❌ Error: No se pudo calcular hash de %s\n", ruta_archivo);
        return 0;
    }
    
    return 1;  // Éxito
}

// ===============================================================================
// FUNCIÓN: escanear_directorio_recursivo
// Función auxiliar para escanear un directorio y todos sus subdirectorios
// ===============================================================================

static int escanear_directorio_recursivo(const char* ruta_directorio, 
                                        FileInfo* archivos, 
                                        int* contador_archivos, 
                                        int max_archivos) {
    /*
     * ¿Qué hace esta función?
     * Es como "explorar" todo un territorio (directorio) y sus subterritorios,
     * catalogando cada habitante (archivo) que encuentra.
     * 
     * Es recursiva: se llama a sí misma para explorar subdirectorios.
     */
    
    DIR* directorio = opendir(ruta_directorio);
    if (directorio == NULL) {
        return 0;  // No se pudo abrir
    }
    
    struct dirent* entrada;
    
    while ((entrada = readdir(directorio)) != NULL) {
        // Saltar entradas especiales
        if (strcmp(entrada->d_name, ".") == 0 || strcmp(entrada->d_name, "..") == 0) {
            continue;
        }
        
        // Construir ruta completa
        char ruta_completa[MAX_PATH_LENGTH];
        snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s", ruta_directorio, entrada->d_name);
        
        struct stat stat_entrada;
        if (stat(ruta_completa, &stat_entrada) != 0) {
            continue;  // No se pudo obtener información
        }
        
        if (S_ISDIR(stat_entrada.st_mode)) {
            // Es un directorio: explorar recursivamente
            escanear_directorio_recursivo(ruta_completa, archivos, contador_archivos, max_archivos);
        } else if (S_ISREG(stat_entrada.st_mode)) {
            // Es un archivo regular: catalogarlo
            if (*contador_archivos < max_archivos) {
                if (obtener_info_archivo(ruta_completa, &archivos[*contador_archivos])) {
                    (*contador_archivos)++;
                }
            }
        }
    }
    
    closedir(directorio);
    return 1;
}

// ===============================================================================
// FUNCIÓN: crear_baseline_dispositivo
// Crea la "fotografía inicial" de todos los archivos en un USB
// ===============================================================================

int crear_baseline_dispositivo(USBDevice* dispositivo) {
    /*
     * ¿Qué hace esta función?
     * Es como tomar una "fotografía completa" del dispositivo USB cuando
     * se conecta por primera vez. Esta fotografía incluye:
     * - Lista de todos los archivos
     * - Hash de cada archivo
     * - Permisos, propietarios, fechas, etc.
     * 
     * Esta "fotografía" sirve como referencia para detectar cambios posteriores.
     */
    
    printf("📸 Creando baseline para dispositivo: %s\n", dispositivo->device_name);
    
    // Resetear el contador de archivos
    dispositivo->baseline_count = 0;
    
    // Escanear recursivamente todo el dispositivo
    if (!escanear_directorio_recursivo(dispositivo->mount_point, 
                                      dispositivo->baseline_files, 
                                      &dispositivo->baseline_count, 
                                      MAX_FILES_PER_DEVICE)) {
        printf("❌ Error: No se pudo escanear el dispositivo %s\n", dispositivo->device_name);
        return 0;
    }
    
    // Actualizar información del dispositivo
    dispositivo->total_files = dispositivo->baseline_count;
    dispositivo->last_scan = time(NULL);
    
    printf("✅ Baseline creado: %d archivos catalogados en %s\n", 
           dispositivo->baseline_count, dispositivo->device_name);
    
    return 1;  // Éxito
}

// ===============================================================================
// FUNCIÓN: detectar_cambios_sospechosos
// Analiza un archivo específico para detectar cambios raros
// ===============================================================================

int detectar_cambios_sospechosos(const FileInfo* archivo_original, const FileInfo* archivo_actual) {
    /*
     * ¿Qué hace esta función?
     * Es como "interrogar" a un archivo para ver si se comporta de manera sospechosa.
     * Compara cómo era el archivo originalmente con cómo está ahora.
     * 
     * Los tipos de cambios sospechosos que detecta:
     * 1. Crecimiento inusual de tamaño
     * 2. Cambios de permisos peligrosos
     * 3. Cambios de propietario
     * 4. Timestamps anómalos
     */
    
    int amenazas_detectadas = AMENAZA_NINGUNA;
    
    // 1. DETECTAR CRECIMIENTO INUSUAL
    // Si un archivo crece más de 100 veces su tamaño original, es sospechoso
    if (archivo_actual->file_size > archivo_original->file_size * 100) {
        printf("🚨 AMENAZA: Crecimiento inusual detectado en %s\n", archivo_actual->file_path);
        printf("   Tamaño original: %ld bytes → Tamaño actual: %ld bytes\n", 
               archivo_original->file_size, archivo_actual->file_size);
        amenazas_detectadas |= AMENAZA_CRECIMIENTO_INUSUAL;
    }
    
    // 2. DETECTAR PERMISOS PELIGROSOS
    // Si los permisos cambiaron a 777 (todos pueden hacer todo), es peligroso
    mode_t permisos_originales = archivo_original->permissions & 0777;
    mode_t permisos_actuales = archivo_actual->permissions & 0777;
    
    if (permisos_actuales == 0777 && permisos_originales != 0777) {
        printf("🚨 AMENAZA: Permisos cambiados a 777 en %s\n", archivo_actual->file_path);
        amenazas_detectadas |= AMENAZA_PERMISOS_PELIGROSOS;
    }
    
    // 3. DETECTAR CAMBIO DE PROPIETARIO
    // Si el dueño del archivo cambió, puede ser sospechoso
    if (archivo_actual->owner_id != archivo_original->owner_id) {
        printf("🚨 AMENAZA: Cambio de propietario en %s\n", archivo_actual->file_path);
        printf("   Propietario original: %d → Propietario actual: %d\n", 
               archivo_original->owner_id, archivo_actual->owner_id);
        amenazas_detectadas |= AMENAZA_CAMBIO_PROPIETARIO;
    }
    
    // 4. DETECTAR TIMESTAMPS ANÓMALOS
    // Si la fecha de modificación es muy antigua o futura, es raro
    time_t ahora = time(NULL);
    if (archivo_actual->last_modified > ahora + 3600 ||  // Más de 1 hora en el futuro
        archivo_actual->last_modified < archivo_original->last_modified - 86400) {  // Más de 1 día atrás
        printf("🚨 AMENAZA: Timestamp anómalo en %s\n", archivo_actual->file_path);
        amenazas_detectadas |= AMENAZA_TIMESTAMP_ANOMALO;
    }
    
    return amenazas_detectadas;
}

// ===============================================================================
// FUNCIÓN: buscar_archivo_en_baseline
// Busca un archivo en la baseline por su ruta
// ===============================================================================

static FileInfo* buscar_archivo_en_baseline(USBDevice* dispositivo, const char* ruta_archivo) {
    /*
     * ¿Qué hace esta función?
     * Busca un archivo específico en la "fotografía inicial" (baseline) del dispositivo.
     * Es como buscar a una persona específica en una foto grupal.
     */
    
    for (int i = 0; i < dispositivo->baseline_count; i++) {
        if (strcmp(dispositivo->baseline_files[i].file_path, ruta_archivo) == 0) {
            return &dispositivo->baseline_files[i];  // Encontrado
        }
    }
    return NULL;  // No encontrado
}

// ===============================================================================
// FUNCIÓN: detectar_replicacion_archivos
// Detecta si se están creando copias sospechosas de archivos
// ===============================================================================

static int detectar_replicacion_archivos(FileInfo* archivos_actuales, int num_archivos) {
    /*
     * ¿Qué hace esta función?
     * Detecta si hay archivos con hashes idénticos pero nombres diferentes.
     * Esto puede indicar que un malware está replicando archivos.
     */
    
    int replicaciones_detectadas = 0;
    
    for (int i = 0; i < num_archivos; i++) {
        for (int j = i + 1; j < num_archivos; j++) {
            // Comparar hashes (¿tienen la misma "huella dactilar"?)
            if (memcmp(archivos_actuales[i].hash, archivos_actuales[j].hash, SHA256_HASH_SIZE) == 0) {
                // ¡Archivos con contenido idéntico pero nombres diferentes!
                printf("🚨 REPLICACIÓN DETECTADA:\n");
                printf("   Archivo 1: %s\n", archivos_actuales[i].file_path);
                printf("   Archivo 2: %s\n", archivos_actuales[j].file_path);
                replicaciones_detectadas++;
            }
        }
    }
    
    return replicaciones_detectadas;
}

// ===============================================================================
// FUNCIÓN: escanear_dispositivo_cambios
// Compara el estado actual del USB con la "fotografía inicial"
// ===============================================================================

int escanear_dispositivo_cambios(USBDevice* dispositivo, ScanResult* resultado, double umbral_cambios) {
    /*
     * ¿Qué hace esta función?
     * Es la función más importante del sistema. Compara cómo está el dispositivo
     * AHORA versus cómo estaba cuando se conectó (baseline).
     * 
     * Es como comparar dos fotografías del mismo lugar tomadas en momentos diferentes
     * para ver qué cambió.
     * 
     * Los pasos son:
     * 1. Escanear todos los archivos actuales del dispositivo
     * 2. Comparar cada archivo actual con su versión en la baseline
     * 3. Detectar archivos nuevos, eliminados, y modificados
     * 4. Analizar si los cambios son sospechosos
     * 5. Generar un reporte completo
     */
    
    printf("🔍 Escaneando cambios en dispositivo: %s\n", dispositivo->device_name);
    
    // Inicializar el resultado
    memset(resultado, 0, sizeof(ScanResult));
    
    // Array para guardar la información actual de los archivos
    FileInfo archivos_actuales[MAX_FILES_PER_DEVICE];
    int archivos_actuales_count = 0;
    
    // Escanear el estado actual del dispositivo
    if (!escanear_directorio_recursivo(dispositivo->mount_point, 
                                      archivos_actuales, 
                                      &archivos_actuales_count, 
                                      MAX_FILES_PER_DEVICE)) {
        printf("❌ Error: No se pudo escanear el dispositivo\n");
        return 0;
    }
    
    resultado->files_scanned = archivos_actuales_count;
    
    // Comparar archivos actuales con baseline
    for (int i = 0; i < archivos_actuales_count; i++) {
        FileInfo* archivo_en_baseline = buscar_archivo_en_baseline(dispositivo, archivos_actuales[i].file_path);
        
        if (archivo_en_baseline == NULL) {
            // ARCHIVO NUEVO: No estaba en la baseline
            resultado->files_added++;
            printf("📄 Archivo nuevo detectado: %s\n", archivos_actuales[i].file_path);
        } else {
            // ARCHIVO EXISTENTE: Comparar si cambió
            if (memcmp(archivo_en_baseline->hash, archivos_actuales[i].hash, SHA256_HASH_SIZE) != 0) {
                // El hash cambió = el archivo fue modificado
                resultado->files_modified++;
                printf("✏️  Archivo modificado: %s\n", archivos_actuales[i].file_path);
                
                // Analizar si el cambio es sospechoso
                int amenazas = detectar_cambios_sospechosos(archivo_en_baseline, &archivos_actuales[i]);
                if (amenazas != AMENAZA_NINGUNA) {
                    resultado->suspicious_files++;
                }
            }
        }
    }
    
    // Detectar archivos eliminados
    for (int i = 0; i < dispositivo->baseline_count; i++) {
        int encontrado = 0;
        for (int j = 0; j < archivos_actuales_count; j++) {
            if (strcmp(dispositivo->baseline_files[i].file_path, archivos_actuales[j].file_path) == 0) {
                encontrado = 1;
                break;
            }
        }
        if (!encontrado) {
            resultado->files_deleted++;
            printf("🗑️  Archivo eliminado: %s\n", dispositivo->baseline_files[i].file_path);
        }
    }
    
    // Detectar replicación de archivos
    int replicaciones = detectar_replicacion_archivos(archivos_actuales, archivos_actuales_count);
    if (replicaciones > 0) {
        resultado->suspicious_files += replicaciones;
    }
    
    // Calcular porcentaje de cambios
    if (dispositivo->baseline_count > 0) {
        resultado->change_percentage = (double)(resultado->files_added + resultado->files_modified + resultado->files_deleted) / dispositivo->baseline_count;
    }
    
    // Determinar nivel de amenaza
    if (resultado->suspicious_files > 0) {
        resultado->threat_level = NIVEL_PELIGROSO;
    } else if (resultado->change_percentage > umbral_cambios) {
        resultado->threat_level = NIVEL_SOSPECHOSO;
    } else {
        resultado->threat_level = NIVEL_LIMPIO;
    }
    
    // Generar reporte detallado
    snprintf(resultado->detailed_report, sizeof(resultado->detailed_report),
        "REPORTE DE ESCANEO - %s\n"
        "Archivos escaneados: %d\n"
        "Archivos nuevos: %d\n"
        "Archivos modificados: %d\n"
        "Archivos eliminados: %d\n"
        "Archivos sospechosos: %d\n"
        "Porcentaje de cambios: %.2f%%\n"
        "Nivel de amenaza: %s\n",
        dispositivo->device_name,
        resultado->files_scanned,
        resultado->files_added,
        resultado->files_modified,
        resultado->files_deleted,
        resultado->suspicious_files,
        resultado->change_percentage * 100,
        (resultado->threat_level == NIVEL_LIMPIO) ? "LIMPIO" :
        (resultado->threat_level == NIVEL_SOSPECHOSO) ? "SOSPECHOSO" : "PELIGROSO");
    
    // Actualizar información del dispositivo
    dispositivo->last_scan = time(NULL);
    dispositivo->suspicious_changes = resultado->suspicious_files;
    dispositivo->is_suspicious = (resultado->threat_level != NIVEL_LIMPIO);
    
    printf("✅ Escaneo completado. Nivel de amenaza: %s\n", 
           (resultado->threat_level == NIVEL_LIMPIO) ? "LIMPIO" :
           (resultado->threat_level == NIVEL_SOSPECHOSO) ? "SOSPECHOSO" : "PELIGROSO");
    
    return (resultado->threat_level != NIVEL_LIMPIO) ? 1 : 0;
}

// ===============================================================================
// FUNCIÓN: generar_alerta_seguridad
// Genera una alerta cuando se detecta algo sospechoso
// ===============================================================================

void generar_alerta_seguridad(const USBDevice* dispositivo, const char* tipo_alerta, const char* mensaje) {
    /*
     * ¿Qué hace esta función?
     * Es como "tocar las campanas de alarma" cuando se detecta una amenaza.
     * Genera una alerta que puede ser mostrada al usuario o enviada a un sistema
     * de monitoreo.
     */
    
    time_t ahora = time(NULL);
    char* timestamp = ctime(&ahora);
    timestamp[strlen(timestamp) - 1] = '\0';  // Remover salto de línea
    
    printf("\n");
    printf("🚨🚨🚨 ALERTA DE SEGURIDAD 🚨🚨🚨\n");
    printf("Timestamp: %s\n", timestamp);
    printf("Dispositivo: %s\n", dispositivo->device_name);
    printf("Punto de montaje: %s\n", dispositivo->mount_point);
    printf("Tipo de alerta: %s\n", tipo_alerta);
    printf("Mensaje: %s\n", mensaje);
    printf("🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨\n");
    printf("\n");
    
    // Aquí se podría agregar código para:
    // - Escribir a un archivo de log
    // - Enviar notificación por email
    // - Actualizar una base de datos
    // - Enviar a un sistema SIEM
}

// ===============================================================================
// FUNCIÓN: obtener_estadisticas_dispositivo
// Proporciona un resumen estadístico de un dispositivo
// ===============================================================================

void obtener_estadisticas_dispositivo(const USBDevice* dispositivo, ScanResult* resultado) {
    /*
     * ¿Qué hace esta función?
     * Genera un resumen estadístico del dispositivo sin hacer un escaneo completo.
     * Es como revisar el "expediente" del dispositivo.
     */
    
    memset(resultado, 0, sizeof(ScanResult));
    
    resultado->files_scanned = dispositivo->total_files;
    resultado->suspicious_files = dispositivo->suspicious_changes;
    resultado->threat_level = dispositivo->is_suspicious ? NIVEL_SOSPECHOSO : NIVEL_LIMPIO;
    
    snprintf(resultado->detailed_report, sizeof(resultado->detailed_report),
        "ESTADÍSTICAS - %s\n"
        "Total de archivos: %d\n"
        "Cambios sospechosos: %d\n"
        "Estado: %s\n"
        "Primera detección: %s"
        "Último escaneo: %s",
        dispositivo->device_name,
        dispositivo->total_files,
        dispositivo->suspicious_changes,
        dispositivo->is_suspicious ? "SOSPECHOSO" : "LIMPIO",
        ctime(&dispositivo->first_seen),
        dispositivo->last_scan > 0 ? ctime(&dispositivo->last_scan) : "Nunca\n");
}

// ===============================================================================
// FUNCIÓN: limpiar_monitor_usb
// Libera todos los recursos utilizados por el sistema
// ===============================================================================

void limpiar_monitor_usb(void) {
    /*
     * ¿Qué hace esta función?
     * Es como "cerrar y limpiar" todas las instalaciones de seguridad.
     * Libera la memoria y marca el sistema como no inicializado.
     */
    
    printf("🧹 Limpiando sistema de monitoreo USB...\n");
    
    // Limpiar la memoria
    memset(dispositivos_monitoreados, 0, sizeof(dispositivos_monitoreados));
    numero_dispositivos = 0;
    sistema_inicializado = 0;
    
    printf("✅ Sistema de monitoreo USB limpiado.\n");
}

/*
 * ===============================================================================
 * RESUMEN DE ESTE ARCHIVO:
 * ===============================================================================
 * 
 * Este archivo implementa un sistema completo de monitoreo de seguridad para
 * dispositivos USB. Sus características principales son:
 * 
 * 1. DETECCIÓN AUTOMÁTICA: Encuentra automáticamente dispositivos USB conectados
 * 2. BASELINE CREATION: Crea una "fotografía inicial" de cada dispositivo
 * 3. DETECCIÓN DE CAMBIOS: Compara el estado actual vs. la fotografía inicial
 * 4. ANÁLISIS DE AMENAZAS: Detecta tipos específicos de cambios sospechosos
 * 5. SISTEMA DE ALERTAS: Genera alertas cuando detecta amenazas
 * 6. REPORTES DETALLADOS: Proporciona información completa sobre cada escaneo
 * 
 * El sistema funciona como un "guardia de seguridad digital" que:
 * - Vigila constantemente los dispositivos USB
 * - Recuerda cómo estaba cada dispositivo cuando se conectó
 * - Detecta cualquier cambio sospechoso
 * - Alerta inmediatamente sobre posibles amenazas
 * 
 * Cumple con todos los requisitos funcionales especificados:
 * ✅ Monitoreo periódico de dispositivos
 * ✅ Escaneo recursivo con hash SHA-256
 * ✅ Detección de cambios sospechosos
 * ✅ Alertas en tiempo real
 * ✅ Umbral configurable de cambios
 */