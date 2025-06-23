#define _GNU_SOURCE  // Para strdup y strnlen
#include <device_monitor.h>

// ============================================================================
// FUNCIONES DE MONITOREO DE DISPOSITIVOS
// ============================================================================

/**
 * Función que monitorea periódicamente el directorio /media para detectar
 * nuevos dispositivos conectados (puertas de entrada del reino)
 * 
 * @param interval_seconds: Intervalo en segundos entre cada verificación
 * @return DeviceList*: Puntero a estructura con lista de dispositivos conectados
 */
DeviceList* monitor_connected_devices() {
    // Inicializar la estructura de lista de dispositivos
    DeviceList *device_list = malloc(sizeof(DeviceList));
    device_list->devices = malloc(10 * sizeof(char*)); // Capacidad inicial de 10 dispositivos
    device_list->count = 0;
    device_list->capacity = 10;
    
    // Directorio típico donde se montan dispositivos USB en Linux
    const char *mount_dir = "/media";
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char full_path[512];
    
    printf("Iniciando monitoreo de dispositivos conectados...\n");
    
    // Limpiar la lista anterior de dispositivos
        for (int i = 0; i < device_list->count; i++) {
            free(device_list->devices[i]);
        }
        device_list->count = 0;
        
        // Abrir el directorio de montaje
        dir = opendir(mount_dir);
        if (dir == NULL) {
            perror("Error al abrir directorio de montaje");
            return NULL;
        }
        
        // Leer todas las entradas del directorio
        while ((entry = readdir(dir)) != NULL) {
            // Saltar directorios especiales . y ..
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            // Construir la ruta completa del dispositivo
            snprintf(full_path, sizeof(full_path), "%s/%s", mount_dir, entry->d_name);
            
            // Verificar si es un directorio (dispositivo montado)
            if (stat(full_path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
                // Verificar si necesitamos expandir el array
                if (device_list->count >= device_list->capacity) {
                    device_list->capacity *= 2;
                    device_list->devices = realloc(device_list->devices, 
                                                 device_list->capacity * sizeof(char*));
                }
                
                // Agregar el dispositivo a la lista
                device_list->devices[device_list->count] = malloc(strlen(entry->d_name) + 1);
                strcpy(device_list->devices[device_list->count], entry->d_name);
                device_list->count++;
                
                printf("Dispositivo detectado: %s (Ruta: %s)\n", entry->d_name, full_path);
            }
        }
        
        // Cerrar el directorio
        closedir(dir);
        
        // Mostrar resumen de dispositivos conectados
        printf("Total de dispositivos conectados: %d\n", device_list->count);
        for (int i = 0; i < device_list->count; i++) {
            printf("  - %s\n", device_list->devices[i]);
        }
        printf("---\n");
    
    return device_list;
}

/**
 * Función para liberar la memoria de la lista de dispositivos
 * 
 * @param device_list: Puntero a la estructura DeviceList a liberar
 */
void free_device_list(DeviceList *device_list) {
    if (device_list != NULL) {
        // Liberar cada nombre de dispositivo
        for (int i = 0; i < device_list->count; i++) {
            free(device_list->devices[i]);
        }
        // Liberar el array de dispositivos
        free(device_list->devices);
        // Liberar la estructura principal
        free(device_list);
    }
}

// ============================================================================
// FUNCIONES DE MANEJO DE ARCHIVOS Y SNAPSHOTS
// ============================================================================

/**
 * Calcula el hash SHA-256 de un archivo
 * 
 * @param filepath: Ruta del archivo
 * @param hash_output: Buffer donde se almacenará el hash (debe ser de al menos 65 bytes)
 * @return int: 0 si es exitoso, -1 si hay error
 */
int calculate_sha256(const char *filepath, char *hash_output) {
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        return -1;
    }
    
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        fclose(file);
        return -1;
    }
    
    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(mdctx);
        fclose(file);
        return -1;
    }
    
    unsigned char buffer[8192];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (EVP_DigestUpdate(mdctx, buffer, bytes_read) != 1) {
            EVP_MD_CTX_free(mdctx);
            fclose(file);
            return -1;
        }
    }
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    
    if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(mdctx);
        fclose(file);
        return -1;
    }
    
    // Convertir a string hexadecimal
    for (unsigned int i = 0; i < hash_len; i++) {
        sprintf(hash_output + (i * 2), "%02x", hash[i]);
    }
    hash_output[hash_len * 2] = '\0';
    
    EVP_MD_CTX_free(mdctx);
    fclose(file);
    return 0;
}

/**
 * Obtiene la extensión de un archivo
 * 
 * @param filename: Nombre del archivo
 * @return char*: Extensión del archivo (debe ser liberada)
 */
char* get_file_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) {
        return strdup(""); // Sin extensión
    }
    return strdup(dot + 1);
}

/**
 * Escanea recursivamente un directorio y almacena información de archivos
 * 
 * @param snapshot: Puntero al snapshot donde almacenar la información
 * @param dir_path: Ruta del directorio a escanear
 * @return int: 0 si es exitoso, -1 si hay error
 */
int scan_directory_recursive(DeviceSnapshot *snapshot, const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        return -1;
    }
    
    struct dirent *entry;
    struct stat file_stat;
    char full_path[1024];
    
    while ((entry = readdir(dir)) != NULL) {
        // Saltar directorios especiales
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        if (stat(full_path, &file_stat) != 0) {
            continue; // Error al obtener información del archivo
        }
        
        if (S_ISDIR(file_stat.st_mode)) {
            // Es un directorio, escanear recursivamente
            scan_directory_recursive(snapshot, full_path);
        } else if (S_ISREG(file_stat.st_mode)) {
            // Es un archivo regular, almacenar información
            
            // Verificar si necesitamos expandir el array
            if (snapshot->file_count >= snapshot->capacity) {
                snapshot->capacity *= 2;
                snapshot->files = realloc(snapshot->files, 
                                        snapshot->capacity * sizeof(FileInfo*));
            }
            
            // Crear nueva entrada de archivo
            FileInfo *file_info = malloc(sizeof(FileInfo));
            
            // Almacenar información básica
            file_info->path = strdup(full_path);
            file_info->name = strdup(entry->d_name);
            file_info->extension = get_file_extension(entry->d_name);
            file_info->size = file_stat.st_size;
            file_info->permissions = file_stat.st_mode;
            file_info->last_modified = file_stat.st_mtime;
            file_info->last_accessed = file_stat.st_atime;
            
            // Calcular hash SHA-256
            if (calculate_sha256(full_path, file_info->sha256_hash) != 0) {
                strcpy(file_info->sha256_hash, "ERROR_CALCULATING_HASH");
            }
            
            // Agregar a la lista
            snapshot->files[snapshot->file_count] = file_info;
            snapshot->file_count++;
            
            printf("Archivo escaneado: %s (Tamaño: %ld bytes, Hash: %.16s...)\n", 
                   entry->d_name, file_stat.st_size, file_info->sha256_hash);
        }
    }
    
    closedir(dir);
    return 0;
}

/**
 * Crea un snapshot completo de un dispositivo
 * 
 * @param device_name: Nombre del dispositivo
 * @return DeviceSnapshot*: Puntero al snapshot creado
 */
DeviceSnapshot* create_device_snapshot(const char *device_name) {
    if (!device_name) {
        printf("Error: device_name es NULL\n");
        return NULL;
    }
    
    // Validar que device_name sea una cadena válida antes de proceder
    size_t name_len = strnlen(device_name, 256);
    if (name_len == 0 || name_len >= 256) {
        printf("Error: device_name inválido (longitud: %zu)\n", name_len);
        return NULL;
    }
    
    // Validar que device_name contenga solo caracteres válidos para nombres de dispositivos
    for (size_t i = 0; i < name_len; i++) {
        char c = device_name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) {
            printf("Error: device_name contiene caracteres inválidos\n");
            return NULL;
        }
    }
    
    DeviceSnapshot *snapshot = malloc(sizeof(DeviceSnapshot));
    if (!snapshot) {
        printf("Error: No se pudo asignar memoria para snapshot\n");
        return NULL;
    }
    
    // Usar malloc + strncpy en lugar de strdup para mayor control
    snapshot->device_name = malloc(name_len + 1);
    if (!snapshot->device_name) {
        printf("Error: No se pudo asignar memoria para device_name\n");
        free(snapshot);
        return NULL;
    }
    
    // Copiar de forma segura y asegurar terminación nula
    strncpy(snapshot->device_name, device_name, name_len);
    snapshot->device_name[name_len] = '\0';
    
    snapshot->files = malloc(100 * sizeof(FileInfo*)); // Capacidad inicial
    if (!snapshot->files) {
        printf("Error: No se pudo asignar memoria para files array\n");
        free(snapshot->device_name);
        free(snapshot);
        return NULL;
    }
    
    snapshot->file_count = 0;
    snapshot->capacity = 100;
    snapshot->snapshot_time = time(NULL);
    
    // Construir la ruta del dispositivo
    char device_path[512];
    snprintf(device_path, sizeof(device_path), "/media/%s", device_name);
    
    printf("Creando snapshot del dispositivo: %s\n", device_name);
    printf("Escaneando directorio: %s\n", device_path);
    
    // Escanear el dispositivo
    if (scan_directory_recursive(snapshot, device_path) != 0) {
        printf("Error al escanear el dispositivo %s\n", device_name);
    }
    
    printf("Snapshot completado: %d archivos encontrados\n", snapshot->file_count);
    printf("---\n");
    
    return snapshot;
}

/**
 * Libera la memoria de un snapshot de dispositivo
 * 
 * @param snapshot: Puntero al snapshot a liberar
 */
void free_device_snapshot(DeviceSnapshot *snapshot) {
    if (snapshot == NULL) {
        return;
    }
    
    // Verificar que el puntero esté en un rango de memoria razonable
    uintptr_t addr = (uintptr_t)snapshot;
    if (addr < 0x1000 || addr > 0x7fffffffffff) {
        printf("Warning: Intentando liberar snapshot con dirección sospechosa: %p\n", 
               (void*)snapshot);
        return;
    }
    
    // Liberar información de cada archivo de forma segura
    if (snapshot->files != NULL && snapshot->file_count > 0) {
        for (int i = 0; i < snapshot->file_count; i++) {
            FileInfo *file = snapshot->files[i];
            if (file != NULL) {
                // Verificar punteros antes de liberar
                if (file->path != NULL) {
                    free(file->path);
                    file->path = NULL;
                }
                if (file->name != NULL) {
                    free(file->name);
                    file->name = NULL;
                }
                if (file->extension != NULL) {
                    free(file->extension);
                    file->extension = NULL;
                }
                free(file);
                snapshot->files[i] = NULL;
            }
        }
    }
    
    // Liberar arrays y estructura
    if (snapshot->files != NULL) {
        free(snapshot->files);
        snapshot->files = NULL;
    }
    
    if (snapshot->device_name != NULL) {
        free(snapshot->device_name);
        snapshot->device_name = NULL;
    }
    
    // Limpiar campos para detectar uso después de liberación
    snapshot->file_count = -1;
    snapshot->capacity = -1;
    snapshot->snapshot_time = 0;
    
    free(snapshot);
}

/**
 * Valida la integridad de un snapshot de dispositivo
 * 
 * @param snapshot: Puntero al snapshot a validar
 * @return int: 0 si es válido, -1 si es inválido
 */
int validate_device_snapshot(const DeviceSnapshot *snapshot) {
    if (!snapshot) {
        printf("Error: snapshot es NULL\n");
        return -1;
    }
    
    // Validar device_name
    if (!snapshot->device_name) {
        printf("Error: device_name en snapshot es NULL\n");
        return -1;
    }
    
    // Validar que device_name sea una cadena válida
    size_t name_len = strnlen(snapshot->device_name, 256);
    if (name_len == 0 || name_len >= 256) {
        printf("Error: device_name en snapshot tiene longitud inválida: %zu\n", name_len);
        return -1;
    }
    
    // Validar puntero de archivos
    if (snapshot->file_count > 0 && !snapshot->files) {
        printf("Error: snapshot indica archivos pero files es NULL\n");
        return -1;
    }
    
    // Validar capacidad vs count
    if (snapshot->file_count > snapshot->capacity) {
        printf("Error: file_count (%d) excede capacity (%d)\n", 
               snapshot->file_count, snapshot->capacity);
        return -1;
    }
    
    // Validar cada archivo si existen
    for (int i = 0; i < snapshot->file_count; i++) {
        FileInfo *file = snapshot->files[i];
        if (!file) {
            printf("Error: archivo %d es NULL en snapshot\n", i);
            return -1;
        }
        
        if (!file->path || !file->name) {
            printf("Error: archivo %d tiene path o name NULL\n", i);
            return -1;
        }
        
        // Validar longitudes de cadenas
        if (strnlen(file->path, 4096) >= 4096 || 
            strnlen(file->name, 256) >= 256) {
            printf("Error: archivo %d tiene path o name demasiado largo\n", i);
            return -1;
        }
    }
    
    printf("Validación de snapshot exitosa: %s (%d archivos)\n", 
           snapshot->device_name, snapshot->file_count);
    
    return 0;
}