#ifndef TEST_PORT_SCANNER_H
#define TEST_PORT_SCANNER_H

#include <time.h>

// Estructura mínima para GUI (sin dependencias GTK)
typedef struct {
    char device_name[256];
    char mount_point[512];
    char status[64];
    int files_changed;
    int total_files;
    int is_suspicious;
    time_t last_scan;
} GUIUSBDevice;

// Declaraciones de funciones del port_scanner.c
void recurse_hash_extended(const char *dir);
int manual_device_scan(const char *device_path);
int analyze_file_changes(void *baseline_file, void *current_file);
int scan_mounts_enhanced(const char *mount_dir);
void log_event(const char *level, const char *message);
void emit_alert(const char *device_path, const char *alert_type, const char *details);
void mark_as_tracked(const char *path);
int is_tracked(const char *path);
void get_usb_statistics(int *total_devices, int *suspicious_devices, int *total_files);

// Variables externas (declaradas en simple_port_scanner.c)
extern int baseline_count;

// Mock functions para evitar dependencias GUI
void gui_add_log_entry(const char *module, const char *level, const char *message);
void gui_update_usb_device(const GUIUSBDevice *device);

#endif
