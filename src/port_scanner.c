#include "port_scanner.h"
#include "gui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>

#define MAX_TRACKED 64
#define MAX_HASHES 8192
#define CHANGE_THRESHOLD 0.1
#define MONITOR_INTERVAL 5  // segundos
#define MAX_DEVICES 32
#define LOG_FILE "/var/log/matcom_usb.log"

typedef struct {
    char path[512];
    int tracked;
    time_t first_seen;
    time_t last_scan;
} TrackedDevice;

typedef struct {
    char filepath[1024];
    unsigned char hash[SHA256_DIGEST_LENGTH];
    time_t mtime;
    mode_t permissions;
    uid_t owner;
    gid_t group;
    off_t size;
} FileHash;

typedef struct {
    char device_path[512];
    char mount_point[512];
    int file_count;
    int suspicious_changes;
    time_t last_scan;
    int is_suspicious;
} USBDeviceInfo;

// Variables globales
static TrackedDevice tracked[MAX_TRACKED];
static int tracked_count = 0;
static FileHash baseline[MAX_HASHES];
static int baseline_count = 0;
static USBDeviceInfo detected_devices[MAX_DEVICES];
static int device_count = 0;
static pthread_t monitor_thread;
static int monitoring_active = 0;
static pthread_mutex_t device_mutex = PTHREAD_MUTEX_INITIALIZER;

// Funciones de logging y alertas
void log_event(const char *level, const char *message) {
    FILE *log = fopen(LOG_FILE, "a");
    if (!log) log = fopen("/tmp/matcom_usb.log", "a"); // fallback
    
    if (log) {
        time_t now = time(NULL);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(log, "[%s] %s: %s\n", timestamp, level, message);
        fclose(log);
    }
    
    // También enviar a GUI si está disponible
    gui_add_log_entry("USB_MONITOR", level, message);
}

void emit_alert(const char *device_path, const char *alert_type, const char *details) {
    char alert_msg[1024];
    snprintf(alert_msg, sizeof(alert_msg), 
        "ALERTA en dispositivo %s - %s: %s", device_path, alert_type, details);
    
    log_event("ALERT", alert_msg);
    printf("\n🚨 %s\n", alert_msg);
}
    for (int i = 0; i < tracked_count; ++i)
        if (strcmp(tracked[i].path, path) == 0) return 1;
    return 0;
}

void mark_as_tracked(const char *path) {
    if (tracked_count < MAX_TRACKED) {
        strcpy(tracked[tracked_count].path, path);
        tracked[tracked_count].first_seen = time(NULL);
        tracked[tracked_count].last_scan = 0;
        tracked_count++;
        
        char msg[512];
        snprintf(msg, sizeof(msg), "Nuevo dispositivo detectado y agregado al seguimiento: %s", path);
        log_event("INFO", msg);
    }
}

// Función mejorada para calcular hash con metadatos adicionales
int compute_file_hash_extended(const char *filepath, FileHash *file_hash) {
    FILE *file = fopen(filepath, "rb");
    if (!file) return -1;

    // Obtener metadatos del archivo
    struct stat st;
    if (stat(filepath, &st) != 0) {
        fclose(file);
        return -1;
    }

    // Calcular hash SHA-256
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    unsigned char buffer[4096];
    size_t bytes;

    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) != 0)
        SHA256_Update(&ctx, buffer, bytes);

    SHA256_Final(file_hash->hash, &ctx);
    fclose(file);

    // Guardar metadatos
    strcpy(file_hash->filepath, filepath);
    file_hash->mtime = st.st_mtime;
    file_hash->permissions = st.st_mode;
    file_hash->owner = st.st_uid;
    file_hash->group = st.st_gid;
    file_hash->size = st.st_size;

    return 0;
}

// Función para detectar tipos específicos de cambios sospechosos
int analyze_file_changes(const FileHash *baseline_file, const FileHash *current_file) {
    int suspicious_flags = 0;
    char details[512] = "";
    
    // 1. Crecimiento inusual de tamaño (más de 100x el tamaño original)
    if (current_file->size > baseline_file->size * 100) {
        suspicious_flags |= 1;
        snprintf(details + strlen(details), sizeof(details) - strlen(details), 
            "Crecimiento inusual: %ld -> %ld bytes; ", baseline_file->size, current_file->size);
    }
    
    // 2. Cambios de permisos sospechosos (777, ejecución añadida)
    if ((current_file->permissions & 0777) == 0777 && (baseline_file->permissions & 0777) != 0777) {
        suspicious_flags |= 2;
        strcat(details, "Permisos cambiados a 777; ");
    }
    
    if ((current_file->permissions & S_IXUSR) && !(baseline_file->permissions & S_IXUSR)) {
        suspicious_flags |= 4;
        strcat(details, "Permisos de ejecución añadidos; ");
    }
    
    // 3. Cambio de propietario (especialmente a root o nobody)
    if (current_file->owner != baseline_file->owner) {
        suspicious_flags |= 8;
        snprintf(details + strlen(details), sizeof(details) - strlen(details), 
            "Propietario cambiado: %d -> %d; ", baseline_file->owner, current_file->owner);
    }
    
    // 4. Timestamp anómalo (futuro o muy antiguo)
    time_t now = time(NULL);
    if (current_file->mtime > now + 3600 || current_file->mtime < baseline_file->mtime - 86400) {
        suspicious_flags |= 16;
        strcat(details, "Timestamp anómalo; ");
    }
    
    if (suspicious_flags && strlen(details) > 0) {
        emit_alert(current_file->filepath, "CAMBIO_SOSPECHOSO", details);
    }
    
    return suspicious_flags;
}

void recurse_hash_extended(const char *dir) {
    DIR *dp = opendir(dir);
    if (!dp) return;

    struct dirent *entry;
    while ((entry = readdir(dp))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);

        struct stat st;
        if (stat(path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            recurse_hash_extended(path);
        } else {
            if (baseline_count < MAX_HASHES) {
                if (compute_file_hash_extended(path, &baseline[baseline_count]) == 0) {
                    baseline_count++;
                }
            }
        }
    }

    closedir(dp);
}

// Función mejorada para comparar hashes con análisis detallado
int compare_hashes_detailed(const char *root_path, USBDeviceInfo *device_info) {
    int changed = 0;
    int scanned = 0;
    int suspicious_changes = 0;
    int new_files = 0;
    int deleted_files = 0;

    // Contar archivos actuales para detectar replicación
    DIR *dp = opendir(root_path);
    int current_file_count = 0;
    if (dp) {
        struct dirent *entry;
        while ((entry = readdir(dp))) {
            if (entry->d_type == DT_REG) current_file_count++;
        }
        closedir(dp);
    }

    for (int i = 0; i < baseline_count; ++i) {
        FileHash current_file;
        if (compute_file_hash_extended(baseline[i].filepath, &current_file) == 0) {
            scanned++;
            
            // Comparar hashes
            if (memcmp(baseline[i].hash, current_file.hash, SHA256_DIGEST_LENGTH) != 0) {
                changed++;
                
                // Análisis detallado de cambios
                int flags = analyze_file_changes(&baseline[i], &current_file);
                if (flags > 0) suspicious_changes++;
            }
        } else {
            // Archivo eliminado
            deleted_files++;
            char msg[512];
            snprintf(msg, sizeof(msg), "Archivo eliminado: %s", baseline[i].filepath);
            emit_alert(root_path, "ARCHIVO_ELIMINADO", msg);
        }
    }
    
    // Detectar replicación de archivos (más archivos que en baseline)
    if (current_file_count > baseline_count * 1.5) {
        new_files = current_file_count - baseline_count;
        char msg[128];
        snprintf(msg, sizeof(msg), "%d archivos nuevos detectados (posible replicación)", new_files);
        emit_alert(root_path, "REPLICACION_ARCHIVOS", msg);
        suspicious_changes++;
    }

    // Actualizar información del dispositivo
    if (device_info) {
        device_info->file_count = scanned;
        device_info->suspicious_changes = suspicious_changes;
        device_info->last_scan = time(NULL);
        device_info->is_suspicious = (suspicious_changes > 0) || 
                                   ((double)changed / scanned > CHANGE_THRESHOLD);
    }

    if (scanned == 0) return 0;
    double ratio = (double)changed / scanned;
    
    // Alertar si hay muchos cambios
    if (ratio > CHANGE_THRESHOLD) {
        char msg[256];
        snprintf(msg, sizeof(msg), 
            "%.1f%% de archivos modificados (%d/%d) - umbral: %.1f%%", 
            ratio * 100, changed, scanned, CHANGE_THRESHOLD * 100);
        emit_alert(root_path, "UMBRAL_CAMBIOS_EXCEDIDO", msg);
    }
    
    return ratio > CHANGE_THRESHOLD;
}

// Función para escanear dispositivos montados y detectar nuevos
int scan_mounts_enhanced(const char *mount_dir) {
    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp) return 0;

    char line[1024];
    int new_devices = 0;

    pthread_mutex_lock(&device_mutex);
    
    while (fgets(line, sizeof(line), fp)) {
        char device[256], mount_point[256], filesystem[64];
        if (sscanf(line, "%255s %255s %63s", device, mount_point, filesystem) == 3) {
            // Buscar dispositivos USB o removibles
            if (strstr(device, "/dev/sd") || strstr(mount_point, "/media") || 
                strstr(mount_point, "/mnt") || strstr(mount_point, mount_dir)) {
                
                // Verificar si ya está siendo monitoreado
                int already_tracked = 0;
                for (int i = 0; i < device_count; i++) {
                    if (strcmp(detected_devices[i].mount_point, mount_point) == 0) {
                        already_tracked = 1;
                        break;
                    }
                }
                
                if (!already_tracked && device_count < MAX_DEVICES) {
                    // Nuevo dispositivo detectado
                    strcpy(detected_devices[device_count].device_path, device);
                    strcpy(detected_devices[device_count].mount_point, mount_point);
                    detected_devices[device_count].file_count = 0;
                    detected_devices[device_count].suspicious_changes = 0;
                    detected_devices[device_count].last_scan = 0;
                    detected_devices[device_count].is_suspicious = 0;
                    
                    char msg[512];
                    snprintf(msg, sizeof(msg), 
                        "Nuevo dispositivo USB detectado: %s montado en %s (%s)", 
                        device, mount_point, filesystem);
                    log_event("INFO", msg);
                    
                    // Actualizar GUI
                    GUIUSBDevice gui_device = {0};
                    strncpy(gui_device.device_name, device, sizeof(gui_device.device_name) - 1);
                    strncpy(gui_device.mount_point, mount_point, sizeof(gui_device.mount_point) - 1);
                    strncpy(gui_device.status, "DETECTADO", sizeof(gui_device.status) - 1);
                    gui_device.last_scan = time(NULL);
                    gui_update_usb_device(&gui_device);
                    
                    device_count++;
                    new_devices++;
                    
                    // Marcar como rastreado y crear baseline
                    mark_as_tracked(mount_point);
                }
            }
        }
    }
    
    pthread_mutex_unlock(&device_mutex);
    fclose(fp);
    return new_devices;
}

// Función de monitoreo periódico en hilo separado
void* usb_monitor_thread(void* arg) {
    log_event("INFO", "Monitor de dispositivos USB iniciado");
    
    while (monitoring_active) {
        // 1. Escanear nuevos dispositivos
        int new_devices = scan_mounts_enhanced("/media");
        
        // 2. Escanear dispositivos existentes
        pthread_mutex_lock(&device_mutex);
        for (int i = 0; i < device_count; i++) {
            USBDeviceInfo *device = &detected_devices[i];
            
            // Verificar si el dispositivo sigue montado
            if (access(device->mount_point, R_OK) != 0) {
                char msg[512];
                snprintf(msg, sizeof(msg), "Dispositivo removido: %s", device->mount_point);
                log_event("INFO", msg);
                
                // Actualizar GUI
                GUIUSBDevice gui_device = {0};
                strncpy(gui_device.device_name, device->device_path, sizeof(gui_device.device_name) - 1);
                strncpy(gui_device.mount_point, device->mount_point, sizeof(gui_device.mount_point) - 1);
                strncpy(gui_device.status, "REMOVIDO", sizeof(gui_device.status) - 1);
                gui_update_usb_device(&gui_device);
                
                // Remover de la lista (simplificado - mover último elemento)
                if (i < device_count - 1) {
                    detected_devices[i] = detected_devices[device_count - 1];
                }
                device_count--;
                i--; // Reajustar índice
                continue;
            }
            
            // Escanear dispositivo para cambios
            baseline_count = 0; // Reset baseline para este dispositivo
            recurse_hash_extended(device->mount_point);
            sleep(1); // Pausa breve para detectar cambios
            
            int is_suspicious = compare_hashes_detailed(device->mount_point, device);
            
            // Actualizar GUI con información del escaneo
            GUIUSBDevice gui_device = {0};
            strncpy(gui_device.device_name, device->device_path, sizeof(gui_device.device_name) - 1);
            strncpy(gui_device.mount_point, device->mount_point, sizeof(gui_device.mount_point) - 1);
            snprintf(gui_device.status, sizeof(gui_device.status), 
                is_suspicious ? "SOSPECHOSO" : "LIMPIO");
            gui_device.files_changed = device->suspicious_changes;
            gui_device.total_files = device->file_count;
            gui_device.is_suspicious = device->is_suspicious;
            gui_device.last_scan = device->last_scan;
            gui_update_usb_device(&gui_device);
        }
        pthread_mutex_unlock(&device_mutex);
        
        // Esperar antes del próximo ciclo
        sleep(MONITOR_INTERVAL);
    }
    
    log_event("INFO", "Monitor de dispositivos USB detenido");
    return NULL;
}

// Funciones de control del monitor
int start_usb_monitoring(void) {
    if (monitoring_active) return 1; // Ya está activo
    
    monitoring_active = 1;
    if (pthread_create(&monitor_thread, NULL, usb_monitor_thread, NULL) != 0) {
        monitoring_active = 0;
        log_event("ERROR", "No se pudo iniciar el hilo de monitoreo USB");
        return 0;
    }
    
    log_event("INFO", "Monitoreo automático de USB iniciado");
    return 1;
}

int stop_usb_monitoring(void) {
    if (!monitoring_active) return 1;
    
    monitoring_active = 0;
    pthread_join(monitor_thread, NULL);
    log_event("INFO", "Monitoreo automático de USB detenido");
    return 1;
}

// Función para obtener estadísticas de dispositivos
void get_usb_statistics(int *total_devices, int *suspicious_devices, int *total_files) {
    pthread_mutex_lock(&device_mutex);
    *total_devices = device_count;
    *suspicious_devices = 0;
    *total_files = 0;
    
    for (int i = 0; i < device_count; i++) {
        if (detected_devices[i].is_suspicious) (*suspicious_devices)++;
        *total_files += detected_devices[i].file_count;
    }
    pthread_mutex_unlock(&device_mutex);
}

// Funciones de compatibilidad (para mantener API existente)
int scan_mounts(const char *mount_dir, char **devices) {
    // Wrapper para compatibilidad - usar scan_mounts_enhanced en su lugar
    return scan_mounts_enhanced(mount_dir);
}

int scan_device(const char *device_path) {
    baseline_count = 0;
    recurse_hash_extended(device_path);
    sleep(1);  // espera mínima para detectar cambios
    return compare_hashes_detailed(device_path, NULL);
}

// Función para escaneo manual de un dispositivo específico
int manual_device_scan(const char *device_path) {
    USBDeviceInfo temp_device = {0};
    strcpy(temp_device.mount_point, device_path);
    
    baseline_count = 0;
    recurse_hash_extended(device_path);
    sleep(1);
    
    int result = compare_hashes_detailed(device_path, &temp_device);
    
    char msg[512];
    snprintf(msg, sizeof(msg), 
        "Escaneo manual completado en %s - Archivos: %d, Cambios sospechosos: %d, Estado: %s",
        device_path, temp_device.file_count, temp_device.suspicious_changes,
        temp_device.is_suspicious ? "SOSPECHOSO" : "LIMPIO");
    log_event("INFO", msg);
    
    return result;
}