#ifndef COMMON_H  
#define COMMON_H  

// Constantes globales
#define LOG_FILE "/var/log/matcom-guard.log"

// Enumeraciones
typedef enum {
    ALERT_USB,
    ALERT_CPU,
    ALERT_PORT
} AlertType;

// Funciones de utilidad
void log_alert(AlertType type, const char *message);

#endif  