/**
 * @file high_cpu_process.c
 * @brief Programa que simula un proceso con alto uso de CPU (bucle infinito)
 * 
 * Este programa crea un bucle infinito que consume CPU intensivamente
 * para probar las alertas de alto uso de CPU del monitor.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

volatile int running = 1;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\n[HIGH_CPU] Recibida señal %d, terminando...\n", sig);
        running = 0;
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
    
    printf("=== SIMULADOR DE ALTO USO DE CPU ===\n");
    printf("PID: %d\n", getpid());
    printf("Duración: %d segundos (o hasta recibir SIGINT/SIGTERM)\n", duration);
    printf("Iniciando bucle intensivo de CPU...\n");
    printf("Presiona Ctrl+C para terminar\n\n");
    
    time_t start_time = time(NULL);
    unsigned long long counter = 0;
    
    // Bucle intensivo que consume CPU
    while (running && (time(NULL) - start_time) < duration) {
        // Operaciones matemáticas intensivas
        for (volatile int i = 0; i < 1000000 && running; i++) {
            counter += i * i;
            counter %= 1000000000;
        }
        
        // Mostrar progreso cada 5 segundos
        if (counter % 500000000 == 0) {
            printf("[HIGH_CPU] Funcionando... Contador: %llu, Tiempo: %ld segundos\n", 
                   counter, time(NULL) - start_time);
        }
    }
    
    printf("\n[HIGH_CPU] Finalizando después de %ld segundos\n", time(NULL) - start_time);
    printf("[HIGH_CPU] Contador final: %llu\n", counter);
    
    return 0;
}
