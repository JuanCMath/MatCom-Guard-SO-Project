/**
 * @file whitelisted_process.c
 * @brief Programa que simula un proceso en lista blanca con alto uso de CPU
 * 
 * Este programa simula un proceso legítimo (como stress, yes, etc.) que
 * debe estar en la lista blanca y no generar alertas a pesar del alto CPU.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>

volatile int running = 1;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\n[STRESS] Recibida señal %d, terminando...\n", sig);
        running = 0;
    }
}

// Simular trabajo de stress testing
void stress_cpu() {
    volatile double result = 0.0;
    for (volatile int i = 0; i < 1000000; i++) {
        result += i * 3.14159;
        result = result * 0.99999;
    }
}

int main(int argc, char *argv[]) {
    int duration = 60; // Duración por defecto: 60 segundos
    
    // Configurar manejadores de señales
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Permitir especificar duración como argumento
    if (argc > 1) {
        duration = atoi(argv[1]);
        if (duration <= 0) duration = 60;
    }
    
    printf("=== PROCESO STRESS (LISTA BLANCA) ===\n");
    printf("PID: %d\n", getpid());
    printf("Nombre del proceso: stress (debería estar en lista blanca)\n");
    printf("Duración: %d segundos\n", duration);
    printf("Este proceso debe generar alto CPU pero NO alertas\n");
    printf("Presiona Ctrl+C para terminar\n\n");
    
    // Cambiar el nombre del proceso para que aparezca como "stress"
    if (argc > 0) {
        strncpy(argv[0], "stress", strlen(argv[0]));
    }
    
    time_t start_time = time(NULL);
    int cycle = 0;
    
    // Bucle intensivo que simula stress testing
    while (running && (time(NULL) - start_time) < duration) {
        stress_cpu();
        cycle++;
        
        // Reportar cada 1000 ciclos
        if (cycle % 1000 == 0) {
            printf("[STRESS] Ciclo %d, Tiempo: %ld segundos (CPU intensivo)\n", 
                   cycle, time(NULL) - start_time);
        }
    }
    
    printf("\n[STRESS] Finalizando después de %ld segundos\n", time(NULL) - start_time);
    printf("[STRESS] Ciclos completados: %d\n", cycle);
    
    return 0;
}
