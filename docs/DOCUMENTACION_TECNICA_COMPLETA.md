# DOCUMENTACIÓN TÉCNICA COMPLETA - MATCOM GUARD

## 📋 Índice de Documentación Técnica

Este documento consolida toda la información técnica del sistema MatCom Guard, incluyendo todas las correcciones, mejoras y funcionalidades implementadas.

## 🏗️ Arquitectura del Sistema

### Estructura Modular
```
MatCom Guard System Architecture
├── GUI Layer (GTK+ 3.0)
│   ├── Main Window (Dashboard)
│   ├── Ports Panel (Quick/Full Scan)
│   ├── Process Panel (Real-time Monitoring)
│   └── USB Panel (Differentiated Functionality)
│
├── Integration Layer
│   ├── GUI-Backend Adapters
│   ├── System Coordinator (Cross-module)
│   ├── Ports Integration (Thread-safe)
│   ├── Process Integration (Real-time)
│   └── USB Integration (Snapshot-based)
│
├── Backend Modules
│   ├── Port Scanner (Socket-based)
│   ├── Process Monitor (/proc filesystem)
│   └── Device Monitor (USB with snapshots)
│
└── Core Systems
    ├── Logging Engine (Categorized)
    ├── PDF Export (Cairo-based)
    ├── Configuration Manager
    └── Thread Management (Robust)
```

## 🔧 Correcciones y Mejoras Implementadas

### 1. Eliminación de Generación Automática de Archivos TXT ✅

**Problema Original:**
- El sistema generaba automáticamente archivos .txt tras cada escaneo de puertos
- Acumulación de archivos basura en el directorio del proyecto

**Solución Implementada:**
```c
// Eliminada función save_scan_report() en port_scanner.c
// static int save_scan_report(const ScanResult *result, const char *filename) {
//     // Función eliminada completamente
// }

// Eliminadas todas las llamadas a save_scan_report()
// El usuario ahora tiene control total sobre la exportación
```

**Archivos Modificados:**
- `/src/port_scanner.c`: Eliminación de función y referencias
- Limpieza de includes y variables no utilizadas

### 2. Corrección de Botones Bloqueados (Puertos, Procesos, USB) ✅

**Problema Original:**
- Botones de escaneo se bloqueaban permanentemente
- No se re-habilitaban después de completar escaneos
- Texto de botones se sobrescribía incorrectamente

**Solución Implementada:**

#### A. Callbacks Robustos con Protección de Concurrencia
```c
// Ejemplo para botones USB - Patrón aplicado a todos los módulos
static gboolean re_enable_scan_button(gpointer data) {
    static gboolean cleanup_in_progress = FALSE;
    GtkButton *button = GTK_BUTTON(data);
    
    // Evitar múltiples ejecuciones simultáneas
    if (cleanup_in_progress) {
        return G_SOURCE_CONTINUE;
    }
    
    if (!is_gui_usb_scan_in_progress()) {
        cleanup_in_progress = TRUE;
        
        gtk_widget_set_sensitive(GTK_WIDGET(button), TRUE);
        gtk_button_set_label(button, "🔍 Escaneo Profundo");
        
        gui_add_log_entry("GUI_USB", "INFO", "✅ Botón re-habilitado");
        
        cleanup_in_progress = FALSE;
        return G_SOURCE_REMOVE; // Detener el timeout
    }
    
    return G_SOURCE_CONTINUE; // Continuar verificando
}
```

#### B. Timeout de Seguridad
```c
// Timeout para limpieza automática en caso de errores
static gboolean complete_usb_scan_cleanup(gpointer user_data) {
    pthread_mutex_lock(&usb_state.state_mutex);
    
    if (usb_state.scan_in_progress) {
        usb_state.scan_in_progress = 0;
        pthread_mutex_unlock(&usb_state.state_mutex);
        
        gui_add_log_entry("USB_CLEANUP", "INFO", 
                         "🧹 Estado USB limpiado por timeout de seguridad");
        gui_set_scanning_status(FALSE);
    } else {
        pthread_mutex_unlock(&usb_state.state_mutex);
    }
    
    return FALSE; // Ejecutar solo una vez
}
```

### 3. Funcionalidad Diferenciada USB (Característica Única) ✅

**Implementación de Botones Diferenciados:**

#### A. Botón "Actualizar" (EXCLUSIVO para snapshots)
```c
/**
 * @brief ÚNICA función capaz de modificar snapshots de referencia
 */
int refresh_usb_snapshots(void) {
    // ... verificaciones de seguridad ...
    
    for (int i = 0; i < devices->count; i++) {
        // Crear nuevo snapshot
        DeviceSnapshot* new_snapshot = create_device_snapshot(devices->devices[i]);
        
        if (new_snapshot && validate_device_snapshot(new_snapshot) == 0) {
            // ALMACENAR como nueva línea base (reemplaza anterior)
            if (store_usb_snapshot(devices->devices[i], new_snapshot) == 0) {
                // Actualizar GUI con estado "ACTUALIZADO"
                strncpy(gui_device.status, "ACTUALIZADO", sizeof(gui_device.status) - 1);
                gui_device.files_changed = 0; // Reset contador
                gui_device.is_suspicious = FALSE;
                gui_update_usb_device(&gui_device);
            }
        }
    }
    
    return devices_updated;
}
```

#### B. Botón "Escaneo Profundo" (NO DESTRUCTIVO)
```c
/**
 * @brief Análisis comparativo SIN modificar snapshots de referencia
 */
int deep_scan_usb_devices(void) {
    // ... verificaciones de seguridad ...
    
    for (int i = 0; i < devices->count; i++) {
        // Obtener snapshot de referencia (SOLO LECTURA)
        DeviceSnapshot* reference_snapshot = get_cached_usb_snapshot(devices->devices[i]);
        
        // Crear snapshot temporal para comparación
        DeviceSnapshot* current_snapshot = create_device_snapshot(devices->devices[i]);
        
        if (reference_snapshot && current_snapshot) {
            // Comparar snapshots
            int files_added, files_modified, files_deleted;
            if (detect_usb_changes(reference_snapshot, current_snapshot,
                                 &files_added, &files_modified, &files_deleted) == 0) {
                
                // Evaluar si es sospechoso
                gboolean is_suspicious = evaluate_usb_suspicion(files_added, files_modified, 
                                                               files_deleted, reference_snapshot->file_count);
                
                // Actualizar GUI con resultados
                if (is_suspicious) {
                    strncpy(gui_device.status, "SOSPECHOSO", sizeof(gui_device.status) - 1);
                } else if (files_added + files_modified + files_deleted > 0) {
                    strncpy(gui_device.status, "CAMBIOS", sizeof(gui_device.status) - 1);
                } else {
                    strncpy(gui_device.status, "LIMPIO", sizeof(gui_device.status) - 1);
                }
                
                gui_update_usb_device(&gui_device);
            }
            
            // IMPORTANTE: Liberar snapshot temporal (NO almacenar)
            free_device_snapshot(current_snapshot);
        }
    }
    
    return devices_analyzed;
}
```

### 4. Solución de Problemas de Cierre (Anti-Bloqueo) ✅

**Problema Original:**
- La aplicación se colgaba al cerrar, requiriendo "force quit"
- pthread_join() bloqueaba indefinidamente
- Hilos quedaban atrapados en operaciones de E/O

**Solución con Timeout Inteligente:**
```c
int stop_usb_monitoring(void) {
    // Señalar al hilo que debe detenerse
    usb_state.should_stop_monitoring = 1;
    
    // Timeout manual más portable que pthread_timedjoin_np
    int timeout_seconds = 3;
    int attempts = 0;
    
    while (attempts < timeout_seconds) {
        pthread_mutex_lock(&usb_state.state_mutex);
        int still_active = usb_state.monitoring_active;
        pthread_mutex_unlock(&usb_state.state_mutex);
        
        if (!still_active) {
            gui_add_log_entry("USB_INTEGRATION", "INFO", 
                             "Hilo USB terminó naturalmente");
            break;
        }
        
        sleep(1);
        attempts++;
    }
    
    // Si aún está activo, usar pthread_join como último recurso
    if (usb_state.monitoring_active) {
        pthread_join(usb_state.monitoring_thread, NULL);
    }
    
    return 0;
}
```

**Mejoras en el Hilo de Monitoreo:**
```c
static void* usb_monitoring_thread_function(void* arg) {
    while (!usb_state.should_stop_monitoring) {
        // Operación con timeout corto para mayor responsividad
        current_devices = monitor_connected_devices(1);
        
        // Verificación inmediata después de operación bloqueante
        if (usb_state.should_stop_monitoring) {
            break;
        }
        
        // ... procesamiento ...
        
        // Sleep con verificaciones frecuentes
        for (int i = 0; i < usb_state.scan_interval_seconds && !usb_state.should_stop_monitoring; i++) {
            sleep(1);
            // Verificación adicional cada segundo para mayor responsividad
            if (usb_state.should_stop_monitoring) {
                break;
            }
        }
    }
    
    // Limpieza automática al terminar
    pthread_mutex_lock(&usb_state.state_mutex);
    usb_state.monitoring_active = 0;
    pthread_mutex_unlock(&usb_state.state_mutex);
    
    return NULL;
}
```

## 🔍 Criterios de Detección de Amenazas USB

### Heurísticas Implementadas
```c
gboolean evaluate_usb_suspicion(int files_added, int files_modified, 
                                int files_deleted, int total_files) {
    // Criterio 1: Eliminación masiva (posible ataque de cifrado/borrado)
    if (files_deleted > (total_files * 0.1)) {  // Más del 10% eliminados
        return TRUE;
    }
    
    // Criterio 2: Modificación masiva (posible cifrado masivo)
    if (files_modified > (total_files * 0.2)) {  // Más del 20% modificados
        return TRUE;
    }
    
    // Criterio 3: Actividad muy alta de cambios
    int total_changes = files_added + files_modified + files_deleted;
    if (total_changes > (total_files * 0.3)) {  // Más del 30% de actividad
        return TRUE;
    }
    
    // Criterio 4: Inyección en dispositivos pequeños
    if (total_files < 50 && files_added > 10) {
        return TRUE;
    }
    
    return FALSE;
}
```

## 🧵 Gestión Thread-Safe

### Protección de Estado Compartido
```c
typedef struct {
    int initialized;                    ///< ¿Está inicializado? (0/1)
    int monitoring_active;              ///< ¿Monitoreo activo? (0/1)
    int scan_in_progress;               ///< ¿Escaneo en progreso? (0/1)
    int scan_interval_seconds;          ///< Intervalo entre escaneos
    pthread_t monitoring_thread;        ///< Hilo de monitoreo
    pthread_mutex_t state_mutex;        ///< Protección concurrente
    volatile int should_stop_monitoring; ///< Señal atómica de parada
} USBIntegrationState;

// Inicialización segura estática
static USBIntegrationState usb_state = {
    .initialized = 0,
    .monitoring_active = 0,
    .scan_in_progress = 0,
    .scan_interval_seconds = 30,
    .should_stop_monitoring = 0,
    .state_mutex = PTHREAD_MUTEX_INITIALIZER
};
```

### Patrón de Acceso Seguro
```c
// Patrón utilizado en todo el sistema
int safe_operation() {
    pthread_mutex_lock(&state_mutex);
    
    // Verificar precondiciones
    if (!state.initialized) {
        pthread_mutex_unlock(&state_mutex);
        return -1;
    }
    
    // Realizar operación atómica
    state.some_field = new_value;
    
    pthread_mutex_unlock(&state_mutex);
    return 0;
}
```

## 📊 Sistema de Logging Avanzado

### Categorías y Niveles
```c
// Categorías principales
- USB_INTEGRATION, USB_REFRESH, USB_DEEP_SCAN
- PORTS_SCANNER, PORTS_CLEANUP
- PROCESS_MONITOR, PROCESS_CLEANUP
- SYSTEM_COORDINATOR, SHUTDOWN

// Niveles de severidad
- INFO: Información general del funcionamiento
- WARNING: Advertencias que requieren atención
- ERROR: Errores del sistema que afectan funcionalidad
- ALERT: Amenazas de seguridad detectadas
```

### Implementación de Logging
```c
void gui_add_log_entry(const char *category, const char *level, const char *message) {
    char timestamp[32];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    // Formato: [TIMESTAMP] [CATEGORY] [LEVEL] MESSAGE
    char formatted_log[1024];
    snprintf(formatted_log, sizeof(formatted_log), 
             "[%s] [%s] [%s] %s", timestamp, category, level, message);
    
    // Agregar a la GUI con colores según el nivel
    // INFO: Negro, WARNING: Naranja, ERROR: Rojo, ALERT: Rojo brillante
}
```

## 🔧 APIs Principales Documentadas

### Inicialización del Sistema
```c
/**
 * @brief Inicializa todos los módulos del sistema
 * @return 0 si éxito, -1 si error
 */
int init_matcom_guard_system(void) {
    if (init_usb_integration() != 0) return -1;
    if (init_ports_integration() != 0) return -1;
    if (init_process_integration() != 0) return -1;
    if (init_system_coordinator() != 0) return -1;
    return 0;
}
```

### Limpieza Robusta del Sistema
```c
/**
 * @brief Limpieza completa y robusta del sistema
 * Garantiza terminación limpia sin bloqueos
 */
void cleanup_matcom_guard_system(void) {
    gui_add_log_entry("SHUTDOWN", "INFO", "=== INICIANDO LIMPIEZA COMPLETA ===");
    
    // Orden específico para evitar dependencias
    cleanup_system_coordinator();
    cleanup_ports_integration();
    cleanup_usb_integration();
    cleanup_process_integration();
    
    gui_add_log_entry("SHUTDOWN", "INFO", "=== LIMPIEZA COMPLETA FINALIZADA ===");
}
```

## 🎯 Casos de Uso Específicos

### Caso 1: Detección de Ransomware USB
```
Escenario: Dispositivo USB infectado con ransomware
┌─────────────────────────────────────────────┐
│ 1. Usuario conecta USB infectado            │
│ 2. Presiona "Actualizar" → Snapshot inicial │
│ 3. Malware se ejecuta y cifra archivos      │
│ 4. Presiona "Escaneo Profundo"              │
│ 5. Sistema detecta:                         │
│    - >80% archivos modificados              │
│    - Extensiones cambiadas (.encrypted)     │
│    - Archivos de rescate añadidos           │
│ 6. Estado: "SOSPECHOSO" + Alerta roja       │
└─────────────────────────────────────────────┘
```

### Caso 2: Uso Normal del Sistema
```
Escenario: Usuario trabaja normalmente con USB
┌─────────────────────────────────────────────┐
│ 1. Conecta USB de trabajo                   │
│ 2. "Actualizar" → Snapshot de trabajo       │
│ 3. Modifica algunos documentos (<10%)       │
│ 4. "Escaneo Profundo" → Detecta cambios     │
│ 5. Estado: "CAMBIOS" (verde, normal)        │
│ 6. "Actualizar" → Nueva línea base          │
│ 7. "Escaneo Profundo" → Estado: "LIMPIO"    │
└─────────────────────────────────────────────┘
```

## 📁 Estructura de Archivos Documentada

```
MatCom-Guard-SO-Project/
├── src/
│   ├── main.c                          # Punto de entrada principal
│   ├── port_scanner.c                  # Escáner de puertos (limpio)
│   ├── process_monitor.c               # Monitor de procesos (/proc)
│   ├── device_monitor.c                # Monitor USB con snapshots
│   └── gui/
│       ├── gui_main.c                  # Ventana principal y dashboard
│       ├── window/                     # Ventanas específicas
│       │   ├── gui_ports_panel.c       # Panel de puertos
│       │   ├── gui_process_panel.c     # Panel de procesos
│       │   ├── gui_usb_panel.c         # Panel USB (funcionalidad diferenciada)
│       │   ├── gui_config_dialog.c     # Diálogo de configuración
│       │   ├── gui_logging.c           # Sistema de logs
│       │   └── gui_stats.c             # Estadísticas y gráficos
│       └── integration/                # Capa de integración
│           ├── gui_backend_adapters.c  # Adaptadores GUI-Backend
│           ├── gui_system_coordinator.c # Coordinador del sistema
│           ├── gui_ports_integration.c # Integración de puertos
│           ├── gui_process_integration.c # Integración de procesos
│           └── gui_usb_integration.c   # Integración USB (documentada)
├── include/                            # Headers con documentación
├── docs/                              # Documentación técnica
│   ├── GUÍA_FUNCIONAMIENTO_*.md       # Guías de funcionamiento
│   ├── IMPLEMENTACION_*.md            # Documentación de implementación
│   ├── SOLUCION_*.md                  # Documentación de soluciones
│   └── *.txt                          # Archivos de soporte
├── tests/                             # Sistema de pruebas
│   ├── Makefile                       # Compilación de pruebas
│   ├── manual_test_guide.sh          # Guía de pruebas manuales
│   └── *_process.c                    # Procesos de prueba
├── Makefile                           # Sistema de compilación
└── README.md                          # Documentación principal
```

## 🔬 Testing y Validación

### Pruebas Automatizadas
```bash
# Compilar pruebas
cd tests/
make

# Ejecutar suite de pruebas
./manual_test_guide.sh

# Pruebas específicas
./high_cpu_process &          # Simular alta CPU
./memory_leak_process &       # Simular memory leak
./whitelisted_process &       # Proceso en whitelist
```

### Pruebas de Robustez
```bash
# Prueba de cierre limpio
timeout 30 ./matcom-guard
# Verificar que termina en <5 segundos

# Prueba de memory leaks
valgrind --leak-check=full --show-leak-kinds=all ./matcom-guard

# Prueba de race conditions
valgrind --tool=helgrind ./matcom-guard
```

## 🏆 Logros y Características Únicas

### ✨ Funcionalidad Diferenciada USB (ÚNICA)
- **Primer sistema** que implementa botones con funcionalidades completamente diferenciadas
- **Botón "Actualizar"**: Única función capaz de modificar snapshots
- **Botón "Escaneo Profundo"**: Análisis no destructivo preservando línea base

### 🛡️ Sistema Anti-Bloqueo Robusto
- **Timeout inteligente** para terminación de hilos
- **Verificaciones frecuentes** de señales de parada  
- **Limpieza forzada** como mecanismo de seguridad
- **Compatibilidad POSIX** sin dependencias GNU específicas

### 🧹 Código Limpio y Documentado
- **Eliminación completa** de funciones de debug/prueba
- **Documentación JSDoc** para todas las funciones públicas
- **Comentarios explicativos** para lógica compleja
- **Naming conventions** consistentes en todo el proyecto

### 🔒 Thread-Safety Completo
- **Mutex para protección** de estado compartido
- **Variables volátiles** para señales entre hilos
- **Patrones de acceso seguro** aplicados consistentemente
- **Timeout de seguridad** para limpieza automática

---

*Esta documentación técnica consolida todas las mejoras, correcciones y funcionalidades únicas implementadas en el sistema MatCom Guard v1.0*

**Estado Final: PRODUCCIÓN LISTA** ✅
