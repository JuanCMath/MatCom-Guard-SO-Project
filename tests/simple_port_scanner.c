// Versión simplificada del port_scanner.c para tests sin dependencias GUI
#include "test_port_scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define MAX_TRACKED 64
#define MAX_HASHES 8192
#define CHANGE_THRESHOLD 0.1
#define MAX_DEVICES 32

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

// Variables globales
static TrackedDevice tracked[MAX_TRACKED];
static int tracked_count = 0;
static FileHash baseline[MAX_HASHES];
int baseline_count = 0;  // Exportado para tests

// Mock de gui_add_log_entry
void gui_add_log_entry(const char *module, const char *level, const char *message) {
    printf("[GUI-LOG] %s - %s: %s\n", module, level, message);
}

// Mock de gui_update_usb_device
void gui_update_usb_device(const GUIUSBDevice *device) {
    printf("[GUI-UPDATE] Device: %s | Status: %s | Files: %d/%d\n", 
           device->device_name, device->status, device->files_changed, device->total_files);
}

// Función de logging
void log_event(const char *level, const char *message) {
    printf("[%s] %s\n", level, message);
}

// Función de alertas
void emit_alert(const char *device_path, const char *alert_type, const char *details) {
    printf("🚨 ALERTA en %s - %s: %s\n", device_path, alert_type, details);
}

// Función para verificar si está tracked
int is_tracked(const char *path) {
    for (int i = 0; i < tracked_count; ++i)
        if (strcmp(tracked[i].path, path) == 0) return 1;
    return 0;
}

// Función para marcar como tracked
void mark_as_tracked(const char *path) {
    if (tracked_count < MAX_TRACKED) {
        strcpy(tracked[tracked_count].path, path);
        tracked[tracked_count].first_seen = time(NULL);
        tracked[tracked_count].last_scan = 0;
        tracked_count++;
        printf("Dispositivo marcado como tracked: %s\n", path);
    }
}

// Función para calcular hash con metadatos
int compute_file_hash_extended(const char *filepath, FileHash *file_hash) {
    FILE *file = fopen(filepath, "rb");
    if (!file) return -1;

    struct stat st;
    if (stat(filepath, &st) != 0) {
        fclose(file);
        return -1;
    }

    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    unsigned char buffer[4096];
    size_t bytes;

    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) != 0)
        SHA256_Update(&ctx, buffer, bytes);

    SHA256_Final(file_hash->hash, &ctx);
    fclose(file);

    strcpy(file_hash->filepath, filepath);
    file_hash->mtime = st.st_mtime;
    file_hash->permissions = st.st_mode;
    file_hash->owner = st.st_uid;
    file_hash->group = st.st_gid;
    file_hash->size = st.st_size;

    return 0;
}

// Función para analizar cambios sospechosos
int analyze_file_changes(void *baseline_ptr, void *current_ptr) {
    FileHash *baseline_file = (FileHash *)baseline_ptr;
    FileHash *current_file = (FileHash *)current_ptr;
    
    int suspicious_flags = 0;
    char details[512] = "";
    
    // Crecimiento inusual
    if (current_file->size > baseline_file->size * 100) {
        suspicious_flags |= 1;
        snprintf(details + strlen(details), sizeof(details) - strlen(details), 
            "Crecimiento inusual: %ld -> %ld bytes; ", baseline_file->size, current_file->size);
    }
    
    // Cambios de permisos sospechosos
    if ((current_file->permissions & 0777) == 0777 && (baseline_file->permissions & 0777) != 0777) {
        suspicious_flags |= 2;
        strcat(details, "Permisos cambiados a 777; ");
    }
    
    // Permisos de ejecución añadidos
    if ((current_file->permissions & S_IXUSR) && !(baseline_file->permissions & S_IXUSR)) {
        suspicious_flags |= 4;
        strcat(details, "Permisos de ejecución añadidos; ");
    }
    
    if (suspicious_flags && strlen(details) > 0) {
        emit_alert(current_file->filepath, "CAMBIO_SOSPECHOSO", details);
    }
    
    return suspicious_flags;
}

// Función recursiva para crear baseline
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

// Función para comparar hashes con análisis detallado
int compare_hashes_detailed(const char *root_path) {
    int changed = 0;
    int scanned = 0;
    int suspicious_changes = 0;

    for (int i = 0; i < baseline_count; ++i) {
        FileHash current_file;
        if (compute_file_hash_extended(baseline[i].filepath, &current_file) == 0) {
            scanned++;
            
            if (memcmp(baseline[i].hash, current_file.hash, SHA256_DIGEST_LENGTH) != 0) {
                changed++;
                
                int flags = analyze_file_changes(&baseline[i], &current_file);
                if (flags > 0) suspicious_changes++;
            }
        }
    }

    if (scanned == 0) return 0;
    double ratio = (double)changed / scanned;
    
    if (ratio > CHANGE_THRESHOLD) {
        char msg[256];
        snprintf(msg, sizeof(msg), 
            "%.1f%% de archivos modificados (%d/%d) - umbral: %.1f%%", 
            ratio * 100, changed, scanned, CHANGE_THRESHOLD * 100);
        emit_alert(root_path, "UMBRAL_CAMBIOS_EXCEDIDO", msg);
    }
    
    return ratio > CHANGE_THRESHOLD || suspicious_changes > 0;
}

// Función para escaneo manual
int manual_device_scan(const char *device_path) {
    baseline_count = 0;
    recurse_hash_extended(device_path);
    sleep(1);
    
    int result = compare_hashes_detailed(device_path);
    
    char msg[512];
    snprintf(msg, sizeof(msg), 
        "Escaneo manual completado en %s - Archivos: %d, Estado: %s",
        device_path, baseline_count, result ? "SOSPECHOSO" : "LIMPIO");
    log_event("INFO", msg);
    
    return result;
}

// Función para escanear montajes
int scan_mounts_enhanced(const char *mount_dir) {
    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp) return 0;

    char line[1024];
    int new_devices = 0;

    while (fgets(line, sizeof(line), fp)) {
        char device[256], mount_point[256], filesystem[64];
        if (sscanf(line, "%255s %255s %63s", device, mount_point, filesystem) == 3) {
            if (strstr(device, "/dev/sd") || strstr(mount_point, "/media") || 
                strstr(mount_point, "/mnt") || strstr(mount_point, mount_dir)) {
                
                if (!is_tracked(mount_point)) {
                    printf("Nuevo dispositivo USB detectado: %s montado en %s (%s)\n", 
                           device, mount_point, filesystem);
                    mark_as_tracked(mount_point);
                    new_devices++;
                }
            }
        }
    }
    
    fclose(fp);
    return new_devices;
}

// Función para obtener estadísticas de dispositivos (mock para tests)
void get_usb_statistics(int *total_devices, int *suspicious_devices, int *total_files) {
    *total_devices = tracked_count;
    *suspicious_devices = 0;  // Simplificado para tests
    *total_files = baseline_count;
}
