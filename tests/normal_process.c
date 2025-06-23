/**
 * @file normal_process.c
 * @brief Programa que simula un proceso normal de larga duración
 * 
 * Este programa simula un proceso normal que puede ser monitoreado
 * y posteriormente terminado para probar la limpieza de procesos fantasma.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>

volatile int running = 1;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\n[NORMAL_PROCESS] Recibida señal %d, terminando gracefully...\n", sig);
        running = 0;
    }
}

void print_status() {
    static time_t start_time = 0;
    if (start_time == 0) start_time = time(NULL);
    
    time_t current_time = time(NULL);
    printf("[NORMAL_PROCESS] PID: %d, Tiempo activo: %ld segundos\n", 
           getpid(), current_time - start_time);
}

int main(int argc, char *argv[]) {
    int duration = 300; // Duración por defecto: 5 minutos
    int report_interval = 10; // Reportar cada 10 segundos
    
    // Configurar manejadores de señales
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Permitir especificar duración como argumento
    if (argc > 1) {
        duration = atoi(argv[1]);
        if (duration <= 0) duration = 300;
    }
    if (argc > 2) {
        report_interval = atoi(argv[2]);
        if (report_interval <= 0) report_interval = 10;
    }
    
    printf("=== PROCESO NORMAL DE LARGA DURACIÓN ===\n");
    printf("PID: %d\n", getpid());
    printf("Duración: %d segundos\n", duration);
    printf("Reportando cada: %d segundos\n", report_interval);
    printf("Presiona Ctrl+C o envía SIGTERM para terminar\n\n");
    
    time_t start_time = time(NULL);
    time_t last_report = start_time;
    
    // Bucle principal del proceso
    while (running && (time(NULL) - start_time) < duration) {
        sleep(1);
        
        // Reportar estado periódicamente
        if (time(NULL) - last_report >= report_interval) {
            print_status();
            last_report = time(NULL);
        }
        
        // Simular trabajo ligero (bajo uso de CPU y memoria)
        static int counter = 0;
        counter++;
        if (counter > 1000000) counter = 0;
    }
    
    if ((time(NULL) - start_time) >= duration) {
        printf("\n[NORMAL_PROCESS] Tiempo de ejecución completado (%d segundos)\n", duration);
    }
    
    print_status();
    printf("[NORMAL_PROCESS] Terminando normalmente\n");
    
    return 0;
}
