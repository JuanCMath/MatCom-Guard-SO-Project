#ifndef PORT_SCANNER_H
#define PORT_SCANNER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>

#define SEPARATOR "=====================================\n"

// ============================================================================
// ESTRUCTURAS PÚBLICAS
// ============================================================================

/**
 * Estructura para información de puerto escaneado
 */
typedef struct {
    int port;                    // Número del puerto
    int is_open;                 // 1 si está abierto, 0 si cerrado
    char service_name[64];       // Nombre del servicio asociado
    int is_suspicious;           // 1 si es sospechoso, 0 si es normal
} PortInfo;

/**
 * Estructura para el resultado completo del escaneo
 */
typedef struct {
    PortInfo *ports;             // Array de puertos escaneados
    int total_ports;             // Total de puertos escaneados
    int open_ports;              // Cantidad de puertos abiertos
    int suspicious_ports;        // Cantidad de puertos sospechosos
} ScanResult;

/**
 * Estructura para mapear puertos con servicios conocidos
 */
typedef struct {
    int port;
    const char *service;
    int is_common;               // 1 si es común y esperado, 0 si no
} ServiceMapping;

// ============================================================================
// FUNCIONES PÚBLICAS DE ESCANEO DE PUERTOS
// ============================================================================

/**
 * Escanea puertos en un rango específico
 * @param start_port: Puerto inicial (1-65535)
 * @param end_port: Puerto final (1-65535)
 * @return int: 0 si es exitoso, -1 si hay error
 */
int scan_ports(int start_port, int end_port);

/**
 * Escanea puertos comunes (1-1024)
 * @return int: 0 si es exitoso, -1 si hay error
 */
int scan_common_ports(void);

/**
 * Escanea un puerto específico
 * @param port: Puerto a escanear (1-65535)
 * @return int: 1 si está abierto, 0 si cerrado, -1 si error
 */
int scan_specific_port(int port);

// ============================================================================
// FUNCIONES AUXILIARES
// ============================================================================

/**
 * Obtiene timestamp actual
 * @return char*: String con fecha y hora actual
 */
const char* get_current_timestamp(void);

#endif // PORT_SCANNER_H

