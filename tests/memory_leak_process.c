/**
 * @file memory_leak_process.c
 * @brief Programa que simula un proceso con fuga de memoria
 * 
 * Este programa aloca memoria progresivamente sin liberarla
 * para probar las alertas de alto uso de RAM del monitor.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#define CHUNK_SIZE (1024 * 1024)  // 1MB por chunk
#define MAX_CHUNKS 512            // Máximo 512MB

volatile int running = 1;
void **memory_chunks = NULL;
int chunk_count = 0;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\n[MEMORY_LEAK] Recibida señal %d, liberando memoria y terminando...\n", sig);
        running = 0;
    }
}

void cleanup_memory() {
    if (memory_chunks) {
        for (int i = 0; i < chunk_count; i++) {
            if (memory_chunks[i]) {
                free(memory_chunks[i]);
            }
        }
        free(memory_chunks);
        printf("[MEMORY_LEAK] Liberados %d chunks de memoria\n", chunk_count);
    }
}

int main(int argc, char *argv[]) {
    int max_memory_mb = 100; // Memoria máxima por defecto: 100MB
    int delay_seconds = 2;   // Delay entre allocaciones
    
    // Configurar manejadores de señales
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    atexit(cleanup_memory);
    
    // Permitir especificar memoria máxima como argumento
    if (argc > 1) {
        max_memory_mb = atoi(argv[1]);
        if (max_memory_mb <= 0 || max_memory_mb > 1024) max_memory_mb = 100;
    }
    if (argc > 2) {
        delay_seconds = atoi(argv[2]);
        if (delay_seconds < 1) delay_seconds = 2;
    }
    
    int max_chunks = max_memory_mb;
    
    printf("=== SIMULADOR DE FUGA DE MEMORIA ===\n");
    printf("PID: %d\n", getpid());
    printf("Memoria objetivo: %d MB (%d chunks de 1MB)\n", max_memory_mb, max_chunks);
    printf("Delay entre allocaciones: %d segundos\n", delay_seconds);
    printf("Presiona Ctrl+C para terminar y liberar memoria\n\n");
    
    // Allocar array para guardar punteros
    memory_chunks = malloc(max_chunks * sizeof(void*));
    if (!memory_chunks) {
        fprintf(stderr, "[MEMORY_LEAK] Error allocando array de punteros\n");
        return 1;
    }
    
    // Inicializar punteros a NULL
    for (int i = 0; i < max_chunks; i++) {
        memory_chunks[i] = NULL;
    }
    
    time_t start_time = time(NULL);
    
    // Bucle de allocación progresiva
    while (running && chunk_count < max_chunks) {
        // Allocar un chunk de 1MB
        void *chunk = malloc(CHUNK_SIZE);
        if (!chunk) {
            printf("[MEMORY_LEAK] Error allocando chunk %d\n", chunk_count);
            break;
        }
        
        // Llenar el chunk con datos para asegurar que se use la memoria
        memset(chunk, chunk_count % 256, CHUNK_SIZE);
        
        memory_chunks[chunk_count] = chunk;
        chunk_count++;
        
        printf("[MEMORY_LEAK] Chunk %d allocado (%d MB total) - Tiempo: %ld segundos\n", 
               chunk_count, chunk_count, time(NULL) - start_time);
        
        // Esperar antes de la siguiente allocación
        for (int i = 0; i < delay_seconds && running; i++) {
            sleep(1);
        }
    }
    
    if (chunk_count >= max_chunks) {
        printf("\n[MEMORY_LEAK] Objetivo alcanzado: %d MB allocados\n", chunk_count);
        printf("[MEMORY_LEAK] Manteniendo memoria allocada. Presiona Ctrl+C para liberar.\n");
        
        // Mantener la memoria allocada hasta recibir señal
        while (running) {
            sleep(1);
        }
    }
    
    printf("\n[MEMORY_LEAK] Finalizando después de %ld segundos\n", time(NULL) - start_time);
    
    return 0;
}
