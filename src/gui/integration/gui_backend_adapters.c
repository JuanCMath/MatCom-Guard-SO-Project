#include "gui_backend_adapters.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

// ============================================================================
// CACHE DE SNAPSHOTS USB
// ============================================================================

// Estructura para mantener cache de snapshots USB
typedef struct USBSnapshotCache {
    char device_name[256];
    DeviceSnapshot *snapshot;
    struct USBSnapshotCache *next;
} USBSnapshotCache;

static USBSnapshotCache *cache_head = NULL;

// ============================================================================
// ADAPTADORES DE ESTRUCTURAS DE DATOS
// ============================================================================

int adapt_process_info_to_gui(const ProcessInfo *backend_info, GUIProcess *gui_process) {
    if (!backend_info || !gui_process) {
        return -1;
    }
    
    // Copia directa de campos compatibles
    gui_process->pid = backend_info->pid;
    strncpy(gui_process->name, backend_info->name, sizeof(gui_process->name) - 1);
    gui_process->name[sizeof(gui_process->name) - 1] = '\0';
    
    gui_process->cpu_usage = backend_info->cpu_usage;
    gui_process->mem_usage = backend_info->mem_usage;
    
    // Lógica de conversión para determinar si es sospechoso
    // Un proceso es sospechoso si:
    // 1. Tiene alerta activa en el backend
    // 2. Excede umbrales y NO está en whitelist
    // 3. Tiene uso extremadamente alto (>95% CPU o >90% memoria)
    gui_process->is_suspicious = FALSE;
    
    if (backend_info->alerta_activa) {
        gui_process->is_suspicious = TRUE;
    } else if (backend_info->exceeds_thresholds && !backend_info->is_whitelisted) {
        gui_process->is_suspicious = TRUE;
    } else if (backend_info->cpu_usage > 95.0 || backend_info->mem_usage > 90.0) {
        gui_process->is_suspicious = TRUE;
    }
    
    return 0;
}

int adapt_device_snapshot_to_gui(const DeviceSnapshot *snapshot, 
                                 const DeviceSnapshot *previous_snapshot,
                                 GUIUSBDevice *gui_device) {
    printf("DEBUG: Entrando a adapt_device_snapshot_to_gui\n");
    printf("DEBUG: snapshot=%p, gui_device=%p\n", (void*)snapshot, (void*)gui_device);
    
    if (!snapshot || !gui_device) {
        fprintf(stderr, "Error: snapshot o gui_device es NULL\n");
        return -1;
    }
    
    printf("DEBUG: snapshot->device_name=%p\n", (void*)snapshot->device_name);
    
    // Inicializar estructura GUI
    memset(gui_device, 0, sizeof(GUIUSBDevice));
    
    // Validar que device_name no sea NULL antes de usarlo
    if (!snapshot->device_name) {
        fprintf(stderr, "Error: snapshot->device_name es NULL\n");
        return -1;
    }
    
    // Validar que la dirección de memory esté en un rango razonable
    uintptr_t addr = (uintptr_t)snapshot->device_name;
    if (addr < 0x1000 || addr > 0x7fffffffffff) {
        fprintf(stderr, "Error: snapshot->device_name tiene dirección sospechosa: %p\n", 
                (void*)snapshot->device_name);
        return -1;
    }
    
    // Intentar acceder al primer byte de forma segura
    char first_char;
    __builtin_memcpy(&first_char, snapshot->device_name, 1);
    
    // Validar que device_name sea una cadena válida (máximo 255 caracteres)
    size_t name_len = strnlen(snapshot->device_name, 256);
    if (name_len >= 256) {
        fprintf(stderr, "Error: snapshot->device_name demasiado largo o inválido\n");
        return -1;
    }
    
    printf("Device name: %s (length: %zu)\n", snapshot->device_name, name_len);
    
    // Copiar nombre del dispositivo de forma segura
    strncpy(gui_device->device_name, snapshot->device_name, sizeof(gui_device->device_name)-1);
    gui_device->device_name[sizeof(gui_device->device_name)-1] = '\0'; // Asegurar terminación

    printf("GUI device name: %s\n", gui_device->device_name);
    
    // Generar punto de montaje basado en el nombre del dispositivo
    snprintf(gui_device->mount_point, sizeof(gui_device->mount_point),
             "/media/%s", snapshot->device_name);
    
    // Establecer total de archivos
    gui_device->total_files = snapshot->file_count;
    
    // Establecer tiempo del último escaneo
    gui_device->last_scan = snapshot->snapshot_time;
    
    // Detectar cambios si hay snapshot anterior
    int files_added = 0, files_modified = 0, files_deleted = 0;
    
    if (previous_snapshot) {
        detect_usb_changes(previous_snapshot, snapshot, 
                          &files_added, &files_modified, &files_deleted);
        gui_device->files_changed = files_added + files_modified + files_deleted;
    } else {
        // Primera vez que se escanea el dispositivo
        gui_device->files_changed = 0;
    }
    
    // Evaluar si el dispositivo es sospechoso
    gui_device->is_suspicious = evaluate_usb_suspicion(files_added, files_modified,
                                                       files_deleted, snapshot->file_count);
    
    // Generar cadena de estado
    generate_usb_status_string(gui_device->files_changed, gui_device->is_suspicious,
                              FALSE, gui_device->status, sizeof(gui_device->status));
    
    return 0;
}

int adapt_port_info_to_gui(const PortInfo *backend_port, GUIPort *gui_port) {
    if (!backend_port || !gui_port) {
        return -1;
    }
    
    // Copia directa de campos compatibles
    gui_port->port = backend_port->port;
    gui_port->is_suspicious = backend_port->is_suspicious;
    
    // Copiar nombre del servicio
    strncpy(gui_port->service, backend_port->service_name, 
            sizeof(gui_port->service) - 1);
    gui_port->service[sizeof(gui_port->service) - 1] = '\0';
    
    // Convertir estado binario a cadena descriptiva
    generate_port_status_string(backend_port->is_open, 
                               gui_port->status, sizeof(gui_port->status));
    
    return 0;
}

int aggregate_port_statistics(const PortInfo *ports, int port_count,
                             int *total_open, int *total_suspicious) {
    if (!ports || !total_open || !total_suspicious) {
        return -1;
    }
    
    *total_open = 0;
    *total_suspicious = 0;
    
    for (int i = 0; i < port_count; i++) {
        if (ports[i].is_open) {
            (*total_open)++;
        }
        if (ports[i].is_suspicious) {
            (*total_suspicious)++;
        }
    }
    
    return 0;
}

// ============================================================================
// FUNCIONES DE DETECCIÓN DE CAMBIOS
// ============================================================================

int detect_usb_changes(const DeviceSnapshot *old_snapshot,
                      const DeviceSnapshot *new_snapshot,
                      int *files_added, int *files_modified, int *files_deleted) {
    if (!old_snapshot || !new_snapshot || !files_added || !files_modified || !files_deleted) {
        return -1;
    }
    
    *files_added = 0;
    *files_modified = 0;
    *files_deleted = 0;
    
    // Buscar archivos nuevos y modificados
    for (int i = 0; i < new_snapshot->file_count; i++) {
        FileInfo *new_file = new_snapshot->files[i];
        int found = 0;
        
        // Buscar este archivo en el snapshot anterior
        for (int j = 0; j < old_snapshot->file_count; j++) {
            FileInfo *old_file = old_snapshot->files[j];
            
            // Comparar por ruta completa
            if (strcmp(new_file->path, old_file->path) == 0) {
                found = 1;
                
                // Verificar si fue modificado comparando hash SHA-256
                if (strcmp(new_file->sha256_hash, old_file->sha256_hash) != 0) {
                    (*files_modified)++;
                }
                break;
            }
        }
        
        // Si no se encontró en el snapshot anterior, es un archivo nuevo
        if (!found) {
            (*files_added)++;
        }
    }
    
    // Buscar archivos eliminados (presentes en old pero no en new)
    for (int i = 0; i < old_snapshot->file_count; i++) {
        FileInfo *old_file = old_snapshot->files[i];
        int found = 0;
        
        for (int j = 0; j < new_snapshot->file_count; j++) {
            FileInfo *new_file = new_snapshot->files[j];
            
            if (strcmp(old_file->path, new_file->path) == 0) {
                found = 1;
                break;
            }
        }
        
        if (!found) {
            (*files_deleted)++;
        }
    }
    
    return 0;
}

gboolean evaluate_usb_suspicion(int files_added, int files_modified, 
                                int files_deleted, int total_files) {
    // Heurísticas para determinar comportamiento sospechoso
    
    // Criterio 1: Muchos archivos eliminados (posible ataque de cifrado/borrado)
    if (files_deleted > (total_files * 0.1)) {  // Más del 10% eliminados
        return TRUE;
    }
    
    // Criterio 2: Muchos archivos modificados (posible cifrado masivo)
    if (files_modified > (total_files * 0.2)) {  // Más del 20% modificados
        return TRUE;
    }
    
    // Criterio 3: Actividad muy alta de cambios
    int total_changes = files_added + files_modified + files_deleted;
    if (total_changes > (total_files * 0.3)) {  // Más del 30% de actividad
        return TRUE;
    }
    
    // Criterio 4: Muchos archivos nuevos en dispositivo pequeño (posible inyección)
    if (total_files < 50 && files_added > 10) {
        return TRUE;
    }
    
    return FALSE;
}

// ============================================================================
// GESTIÓN DE ESTADO Y CACHE
// ============================================================================

int init_usb_snapshot_cache(void) {
    // El cache se inicializa vacío (cache_head = NULL ya establecido)
    return 0;
}

int store_usb_snapshot(const char *device_name, const DeviceSnapshot *snapshot) {
    if (!device_name || !snapshot) {
        return -1;
    }
    
    // Buscar si ya existe una entrada para este dispositivo
    USBSnapshotCache *current = cache_head;
    while (current) {
        if (strcmp(current->device_name, device_name) == 0) {
            // Reemplazar snapshot existente
            if (current->snapshot) {
                free_device_snapshot(current->snapshot);
            }
            current->snapshot = (DeviceSnapshot*)snapshot;  // Cast para eliminar const
            return 0;
        }
        current = current->next;
    }
    
    // Crear nueva entrada en el cache
    USBSnapshotCache *new_entry = malloc(sizeof(USBSnapshotCache));
    if (!new_entry) {
        return -1;
    }
    
    strncpy(new_entry->device_name, device_name, sizeof(new_entry->device_name) - 1);
    new_entry->device_name[sizeof(new_entry->device_name) - 1] = '\0';
    new_entry->snapshot = (DeviceSnapshot*)snapshot;  // Cast para eliminar const
    new_entry->next = cache_head;
    cache_head = new_entry;
    
    return 0;
}

DeviceSnapshot* get_cached_usb_snapshot(const char *device_name) {
    if (!device_name) {
        return NULL;
    }
    
    USBSnapshotCache *current = cache_head;
    while (current) {
        if (strcmp(current->device_name, device_name) == 0) {
            return current->snapshot;
        }
        current = current->next;
    }
    
    return NULL;
}

void cleanup_usb_snapshot_cache(void) {
    USBSnapshotCache *current = cache_head;
    while (current) {
        USBSnapshotCache *next = current->next;
        if (current->snapshot) {
            free_device_snapshot(current->snapshot);
        }
        free(current);
        current = next;
    }
    cache_head = NULL;
}

// ============================================================================
// UTILIDADES DE CONVERSIÓN
// ============================================================================

int format_timestamp_for_gui(time_t timestamp, char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return -1;
    }
    
    if (timestamp == 0) {
        strncpy(buffer, "Nunca", buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return 0;
    }
    
    time_t now = time(NULL);
    int diff = (int)(now - timestamp);
    
    if (diff < 60) {
        snprintf(buffer, buffer_size, "Hace %d seg", diff);
    } else if (diff < 3600) {
        snprintf(buffer, buffer_size, "Hace %d min", diff / 60);
    } else if (diff < 86400) {
        snprintf(buffer, buffer_size, "Hace %d horas", diff / 3600);
    } else {
        snprintf(buffer, buffer_size, "Hace %d días", diff / 86400);
    }
    
    return 0;
}

int generate_usb_status_string(int files_changed, gboolean is_suspicious, 
                              gboolean is_scanning, char *status_buffer, 
                              size_t buffer_size) {
    if (!status_buffer || buffer_size == 0) {
        return -1;
    }
    
    if (is_scanning) {
        strncpy(status_buffer, "ESCANEANDO", buffer_size - 1);
    } else if (is_suspicious) {
        strncpy(status_buffer, "SOSPECHOSO", buffer_size - 1);
    } else if (files_changed > 0) {
        strncpy(status_buffer, "CAMBIOS DETECTADOS", buffer_size - 1);
    } else {
        strncpy(status_buffer, "LIMPIO", buffer_size - 1);
    }
    
    status_buffer[buffer_size - 1] = '\0';
    return 0;
}

int generate_port_status_string(int is_open, char *status_buffer, size_t buffer_size) {
    if (!status_buffer || buffer_size == 0) {
        return -1;
    }
    
    if (is_open) {
        strncpy(status_buffer, "Abierto", buffer_size - 1);
    } else {
        strncpy(status_buffer, "Cerrado", buffer_size - 1);
    }
    
    status_buffer[buffer_size - 1] = '\0';
    return 0;
}