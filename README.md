# 🛡️ MatCom Guard - Sistema de Monitoreo y Seguridad

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)]()
[![Platform](https://img.shields.io/badge/platform-Linux-blue)]()
[![License](https://img.shields.io/badge/license-Open%20Source-orange)]()
[![Version](https://img.shields.io/badge/version-1.0-red)]()

> *En este vasto reino digital, los virus y amenazas informáticas son como plagas y ejércitos invasores que buscan corromper tus tierras y saquear tus recursos. MatCom Guard es tu muralla y tu guardia real, un sistema diseñado para vigilar y proteger tu reino (máquina virtual) basado en UNIX de cualquier intruso o actividad sospechosa.*

## 📋 Tabla de Contenidos

- [Características Principales](#-características-principales)
- [Arquitectura del Sistema](#-arquitectura-del-sistema)
- [Instalación y Compilación](#-instalación-y-compilación)
- [Guía de Uso](#-guía-de-uso)
- [Funcionalidades Avanzadas](#-funcionalidades-avanzadas)
- [Documentación Técnica](#-documentación-técnica)
- [Troubleshooting](#-troubleshooting)
- [Contribución](#-contribución)

## ✨ Características Principales

### 🔍 **Monitoreo Integral Multi-Módulo**
- **Escaneo de Puertos**: Detección de puertos abiertos con análisis de amenazas
- **Monitor de Procesos**: Vigilancia en tiempo real de CPU, memoria y actividad
- **Monitor USB**: Análisis forense de dispositivos con sistema de snapshots

### 🎯 **Funcionalidades Diferenciadas USB**
- **Botón "Actualizar"**: Único capaz de retomar snapshots de referencia
- **Botón "Escaneo Profundo"**: Análisis comparativo sin alterar línea base
- **Detección de Actividad Sospechosa**: Heurísticas avanzadas de seguridad

### 🖥️ **Interfaz Gráfica Intuitiva**
- **Dashboard Centralizado**: Vista unificada de todos los módulos
- **Logs en Tiempo Real**: Sistema de logging categorizado y filtrable
- **Exportación PDF**: Reportes profesionales con gráficos integrados
- **Estados Visuales**: Indicadores claros del estado del sistema

### 🔒 **Seguridad y Robustez**
- **Thread-Safe**: Arquitectura multi-hilo segura con mutex
- **Sin Bloqueos**: Timeouts inteligentes para terminación limpia
- **Recuperación de Errores**: Manejo robusto de situaciones anómalas
- **Cache Inteligente**: Sistema optimizado de snapshots USB

## 🏗️ Arquitectura del Sistema

```
MatCom Guard
├── Frontend (GUI)
│   ├── Panel Principal (Dashboard)
│   ├── Panel de Puertos
│   ├── Panel de Procesos
│   └── Panel USB (Funcionalidad Diferenciada)
│
├── Backend Modules
│   ├── Port Scanner
│   ├── Process Monitor
│   └── USB Device Monitor
│
├── Integration Layer
│   ├── GUI-Backend Adapters
│   ├── System Coordinator
│   └── Thread Management
│
└── Core Systems
    ├── Logging Engine
    ├── Configuration Manager
    └── PDF Export Engine
```

### 🧩 **Componentes Clave**

#### **Sistema de Snapshots USB**
```c
typedef struct {
    char *device_name;          // Nombre del dispositivo
    FileInfo **files;           // Array de archivos
    int file_count;             // Número de archivos
    time_t snapshot_time;       // Tiempo del snapshot
} DeviceSnapshot;
```

#### **Estados Thread-Safe**
- Mutex para protección de concurrencia
- Variables volátiles para señales de parada
- Timeouts robustos para evitar bloqueos

## 🛠️ Instalación y Compilación

### **Requisitos del Sistema**

#### **Dependencias Esenciales**
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential pkg-config
sudo apt-get install libgtk-3-dev libcairo2-dev
sudo apt-get install libssl-dev libudev-dev
sudo apt-get install pthread

# CentOS/RHEL
sudo yum groupinstall "Development Tools"
sudo yum install gtk3-devel cairo-devel
sudo yum install openssl-devel libudev-devel
```

#### **Bibliotecas Utilizadas**
- **GTK+ 3.0**: Interfaz gráfica moderna y responsiva
- **Cairo/Cairo-PDF**: Renderizado gráfico y exportación PDF
- **OpenSSL**: Criptografía para hashes SHA-256
- **libudev**: Monitoreo de dispositivos USB en Linux
- **pthreads**: Multi-threading para monitoreo en tiempo real

### **Proceso de Compilación**

```bash
# Clonar el repositorio
git clone [repository-url]
cd MatCom-Guard-SO-Project

# Compilar el proyecto
make clean && make

# Ejecutar la aplicación
./matcom-guard

# Instalación en el sistema (opcional)
sudo make install
```

### **Makefile Inteligente**
```makefile
# Flags optimizados para desarrollo y producción
CFLAGS = -Wall -Wextra -std=c99 -g -Iinclude -DDEBUG
GTK_FLAGS = `pkg-config --cflags --libs gtk+-3.0`
CAIRO_FLAGS = `pkg-config --cflags --libs cairo cairo-pdf`
LIBS = -lcrypto -lpthread -ludev
```

## 📖 Guía de Uso

### **🚀 Inicio Rápido**

1. **Lanzar la Aplicación**
   ```bash
   ./matcom-guard
   ```

2. **Dashboard Principal**
   - Vista general del estado del sistema
   - Estadísticas en tiempo real
   - Acceso rápido a todos los módulos

3. **Escaneo Básico**
   - **Puertos**: Botones "Escaneo Rápido" y "Escaneo Completo"
   - **Procesos**: Monitoreo automático con alertas
   - **USB**: Funcionalidad diferenciada única

### **🔌 Monitoreo de Dispositivos USB**

#### **Funcionalidad Diferenciada Única**

**🔄 Botón "Actualizar"**
- **Función Exclusiva**: Único capaz de retomar snapshots
- **Uso**: Después de cambios legítimos en dispositivos
- **Resultado**: Establece nueva línea base de referencia
- **Estado GUI**: Marca dispositivos como "ACTUALIZADO"

**🔍 Botón "Escaneo Profundo"**
- **Función No Destructiva**: Compara sin alterar snapshots
- **Uso**: Verificación de seguridad periódica
- **Resultado**: Detecta cambios sin modificar línea base
- **Estados GUI**: "LIMPIO", "CAMBIOS", "SOSPECHOSO"

#### **Criterios de Detección de Amenazas**
```
Actividad Sospechosa:
├── Eliminación Masiva: >10% archivos eliminados
├── Modificación Masiva: >20% archivos modificados
├── Actividad Alta: >30% cambios totales
└── Inyección: Muchos archivos nuevos en dispositivos pequeños
```

### **📊 Monitoreo de Procesos**

- **Tiempo Real**: Actualización continua de CPU y memoria
- **Alertas Inteligentes**: Detección de procesos sospechosos
- **Información Detallada**: PID, nombre, usuario, estado
- **Acciones**: Terminación segura de procesos problemáticos

### **🔍 Escaneo de Puertos**

- **Escaneo Rápido**: Puertos comunes (21, 22, 23, 25, 53, 80, 110, 443, 993, 995)
- **Escaneo Completo**: Rango amplio de puertos (1-65535)
- **Detección de Servicios**: Identificación automática de servicios
- **Análisis de Amenazas**: Evaluación de riesgos de seguridad

## 🔧 Funcionalidades Avanzadas

### **📄 Exportación de Reportes PDF**

```c
// Sistema profesional de reportes
- Gráficos estadísticos integrados
- Formato profesional con logos
- Información detallada de todos los módulos
- Timestamps y metadatos completos
```

### **🔄 Sistema de Logging Avanzado**

```c
// Categorías de logging
- INFO: Información general
- WARNING: Advertencias importantes
- ERROR: Errores del sistema
- ALERT: Amenazas detectadas
```

### **⚙️ Configuración Flexible**

- **Intervalos de Escaneo**: Configurables por módulo
- **Umbrales de Alerta**: Personalizables para CPU/memoria
- **Notificaciones**: Alertas sonoras y visuales
- **Filtros**: Personalización de logs y reportes

### **🛡️ Seguridad Thread-Safe**

```c
// Protección robusta contra race conditions
pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int should_stop_monitoring = 0;

// Timeout inteligente para evitar bloqueos
int timeout_seconds = 3;
// Verificación periódica cada segundo
```

## 📚 Documentación Técnica

### **🔍 APIs Principales**

#### **Integración USB**
```c
int init_usb_integration(void);           // Inicialización
int start_usb_monitoring(int interval);   // Inicio de monitoreo
int refresh_usb_snapshots(void);          // Actualización exclusiva
int deep_scan_usb_devices(void);          // Análisis no destructivo
void cleanup_usb_integration(void);       // Limpieza robusta
```

#### **Monitor de Procesos**
```c
int init_process_monitoring(void);        // Inicialización
int start_process_monitoring(void);       // Inicio de monitoreo
ProcessInfo* get_process_list(void);      // Obtener lista de procesos
void cleanup_process_monitoring(void);    // Limpieza
```

#### **Escáner de Puertos**
```c
ScanResult* scan_ports_range(int start, int end);  // Escaneo por rango
int is_port_open(const char *host, int port);      // Verificar puerto
void free_scan_result(ScanResult *result);         // Liberar memoria
```

### **📁 Estructura de Archivos**

```
MatCom-Guard-SO-Project/
├── src/
│   ├── main.c                          # Punto de entrada
│   ├── port_scanner.c                  # Escáner de puertos
│   ├── process_monitor.c               # Monitor de procesos
│   ├── device_monitor.c                # Monitor de dispositivos
│   └── gui/
│       ├── gui_main.c                  # GUI principal
│       ├── window/                     # Ventanas de la GUI
│       └── integration/                # Capa de integración
├── include/                            # Headers
├── docs/                              # Documentación
├── tests/                             # Pruebas del sistema
└── README.md                          # Este archivo
```

### **🧪 Casos de Prueba**

#### **Pruebas de Funcionalidad USB**
```bash
# Prueba 1: Funcionalidad diferenciada
1. Conectar dispositivo USB
2. Presionar "Actualizar" → Verificar estado "ACTUALIZADO"
3. Modificar archivos en dispositivo
4. Presionar "Escaneo Profundo" → Verificar detección de cambios
5. Verificar que snapshot no cambió

# Prueba 2: Detección de amenazas
1. Eliminar >10% de archivos → Verificar estado "SOSPECHOSO"
2. Modificar >20% de archivos → Verificar alerta de seguridad
```

#### **Pruebas de Robustez**
```bash
# Prueba de cierre limpio
1. Ejecutar monitoreo completo
2. Cerrar aplicación → Verificar terminación en <5 segundos
3. Verificar que no quedan procesos zombie

# Prueba de concurrencia
1. Ejecutar múltiples escaneos simultáneos
2. Verificar protección contra race conditions
3. Verificar limpieza correcta de recursos
```

## 🔧 Troubleshooting

### **❌ Problemas Comunes**

#### **Error de Compilación**
```bash
# Error: pkg-config no encontrado
sudo apt-get install pkg-config

# Error: headers GTK+ no encontrados
sudo apt-get install libgtk-3-dev

# Error: libcrypto no encontrada
sudo apt-get install libssl-dev
```

#### **Problemas de Ejecución**
```bash
# Error: No se puede acceder a dispositivos USB
sudo usermod -a -G plugdev $USER
# Reiniciar sesión después

# Error: Permisos insuficientes para procesos
sudo chmod +s ./matcom-guard
# O ejecutar con sudo para funcionalidad completa
```

#### **Problemas de Rendimiento**
```bash
# Alta carga de CPU
- Ajustar intervalos de escaneo en configuración
- Usar escaneo rápido en lugar de completo

# Uso excesivo de memoria
- Verificar limpieza de snapshots USB
- Revisar logs para memory leaks
```

### **🔍 Debugging**

```bash
# Compilación con información de debug
make clean
CFLAGS="-g -DDEBUG -O0" make

# Ejecutar con gdb
gdb ./matcom-guard

# Verificar memory leaks
valgrind --leak-check=full ./matcom-guard
```

### **📞 Soporte**

Para problemas no resueltos:

1. **Revisar logs** en la interfaz gráfica
2. **Consultar documentación** en `/docs`
3. **Ejecutar pruebas** en `/tests`
4. **Reportar issues** con información completa del sistema

## 🤝 Contribución

### **🌟 Cómo Contribuir**

1. **Fork** el repositorio
2. **Crear branch** para nueva funcionalidad
3. **Implementar** con documentación completa
4. **Ejecutar pruebas** de regresión
5. **Enviar Pull Request** con descripción detallada

### **📋 Estándares de Código**

- **Estilo**: C99 estándar con comentarios JSDoc
- **Naming**: snake_case para funciones, UPPER_CASE para constantes
- **Documentación**: Doxygen-style para todas las funciones públicas
- **Testing**: Casos de prueba para toda nueva funcionalidad

### **🏆 Áreas de Mejora**

- **Soporte Multi-Plataforma**: Extensión a Windows/macOS
- **Análisis ML**: Detección de anomalías con machine learning
- **API REST**: Interfaz web para monitoreo remoto
- **Base de Datos**: Persistencia de logs y estadísticas

---

## 📜 Créditos y Licencia

**Desarrollado para el proyecto de Sistemas Operativos - MatCom**

### **🔗 Recursos Utilizados**
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [GTK+ Documentation](https://www.gtk.org/docs/)
- [Linux Man Pages - proc(5)](https://man7.org/linux/man-pages/man5/proc.5.html)

### **⚖️ Licencia**
Este proyecto es open source y está disponible bajo licencia educativa para fines académicos.

---

*MatCom Guard v1.0 - Tu guardia digital confiable* 🛡️
