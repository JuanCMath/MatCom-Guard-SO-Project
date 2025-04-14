#ifndef DEVICE_MONITOR_H  
#define DEVICE_MONITOR_H  

#include <stdbool.h>

// Estructura para almacenar info de dispositivos
typedef struct {
    char dev_name[50];
    char mount_point[256];
    bool is_suspicious;
} USBDevice;

// Funciones
void monitor_usb_devices();
void scan_usb_filesystem(const char *mount_point);
bool detect_suspicious_changes(const char *path);

#endif  