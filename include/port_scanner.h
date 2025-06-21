#ifndef SCANNER_H
#define SCANNER_H

// Funciones principales de escaneo
int scan_mounts(const char *mount_dir, char **devices);
int scan_device(const char *device_path);
int manual_device_scan(const char *device_path);

// Funciones de seguimiento de dispositivos
void mark_as_tracked(const char *path);
int is_tracked(const char *path);

// Funciones de monitoreo automático
int start_usb_monitoring(void);
int stop_usb_monitoring(void);
int scan_mounts_enhanced(const char *mount_dir);

// Funciones de estadísticas y logging
void get_usb_statistics(int *total_devices, int *suspicious_devices, int *total_files);
void log_event(const char *level, const char *message);
void emit_alert(const char *device_path, const char *alert_type, const char *details);

#endif
