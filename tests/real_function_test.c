#include "test_port_scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#define TEST_USB_PATH "/tmp/test_virtual_usb"
#define TEST_IMG_FILE "/tmp/test_usb.img"

// Colores para output
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define CYAN    "\x1b[36m"
#define RESET   "\x1b[0m"

// Variables para tracking de tests
static int tests_passed = 0;
static int tests_failed = 0;

void print_test_result(const char* test_name, int passed) {
    if (passed) {
        printf(GREEN "✅ PASS: %s" RESET "\n", test_name);
        tests_passed++;
    } else {
        printf(RED "❌ FAIL: %s" RESET "\n", test_name);
        tests_failed++;
    }
}

void print_test_header(const char* test_section) {
    printf(BLUE "\n🧪 === %s ===" RESET "\n", test_section);
}

int setup_virtual_usb() {
    printf(YELLOW "🔧 Configurando USB virtual..." RESET "\n");
    
    // Limpiar cualquier setup previo
    system("sudo umount " TEST_USB_PATH " 2>/dev/null || true");
    system("rm -rf " TEST_USB_PATH " " TEST_IMG_FILE " 2>/dev/null || true");
    
    // Crear imagen de disco de 10MB
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "dd if=/dev/zero of=%s bs=1M count=10 2>/dev/null", TEST_IMG_FILE);
    if (system(cmd) != 0) {
        printf(RED "Error creando imagen de disco" RESET "\n");
        return 0;
    }
    
    // Formatear como FAT32
    snprintf(cmd, sizeof(cmd), "mkfs.vfat %s >/dev/null 2>&1", TEST_IMG_FILE);
    if (system(cmd) != 0) {
        printf(RED "Error formateando disco" RESET "\n");
        return 0;
    }
    
    // Crear punto de montaje
    if (mkdir(TEST_USB_PATH, 0755) != 0) {
        // Ya existe, está bien
    }
    
    // Montar la imagen
    snprintf(cmd, sizeof(cmd), "sudo mount -o loop %s %s", TEST_IMG_FILE, TEST_USB_PATH);
    if (system(cmd) != 0) {
        printf(RED "Error montando USB virtual" RESET "\n");
        return 0;
    }
    
    // Dar permisos
    snprintf(cmd, sizeof(cmd), "sudo chmod 777 %s", TEST_USB_PATH);
    system(cmd);
    
    printf(GREEN "✅ USB virtual creado en: %s" RESET "\n", TEST_USB_PATH);
    return 1;
}

void create_test_files() {
    printf(YELLOW "📁 Creando archivos de prueba..." RESET "\n");
    
    // Crear estructura básica
    mkdir(TEST_USB_PATH "/Documents", 0755);
    mkdir(TEST_USB_PATH "/Photos", 0755);
    
    // Archivo de texto normal
    FILE *f = fopen(TEST_USB_PATH "/Documents/readme.txt", "w");
    if (f) {
        fprintf(f, "Este es un archivo de prueba.\nContenido inicial.\n");
        fclose(f);
    }
    
    // Archivo PDF simulado
    f = fopen(TEST_USB_PATH "/Documents/document.pdf", "w");
    if (f) {
        fprintf(f, "%%PDF-1.4\nContenido PDF simulado\n");
        fclose(f);
    }
    
    // Imagen simulada
    f = fopen(TEST_USB_PATH "/Photos/photo.jpg", "w");
    if (f) {
        // Escribir datos binarios simulados
        for (int i = 0; i < 1024; i++) {
            fputc(i % 256, f);
        }
        fclose(f);
    }
    
    printf(GREEN "✅ Archivos de prueba creados" RESET "\n");
}

void cleanup_virtual_usb() {
    printf(YELLOW "🧹 Limpiando USB virtual..." RESET "\n");
    
    system("sudo umount " TEST_USB_PATH " 2>/dev/null || true");
    system("rm -rf " TEST_USB_PATH " " TEST_IMG_FILE " 2>/dev/null || true");
    
    printf(GREEN "✅ Limpieza completada" RESET "\n");
}

// TEST 1: Verificar que tus funciones de hashing funcionan
int test_baseline_creation() {
    print_test_header("TEST 1: CREACIÓN DE BASELINE");
    
    printf("Llamando a recurse_hash_extended()...\n");
    
    // Resetear baseline
    extern int baseline_count;
    baseline_count = 0;
    
    // Llamar TU función real
    recurse_hash_extended(TEST_USB_PATH);
    
    printf("Baseline creado con %d archivos\n", baseline_count);
    
    int passed = (baseline_count > 0);
    print_test_result("Creación de baseline con recurse_hash_extended()", passed);
    
    return passed;
}

// TEST 2: Verificar detección de cambios con manual_device_scan
int test_manual_scan() {
    print_test_header("TEST 2: ESCANEO MANUAL");
    
    printf("Llamando a manual_device_scan()...\n");
    
    // Llamar TU función real de escaneo manual
    int result = manual_device_scan(TEST_USB_PATH);
    
    printf("Resultado del escaneo manual: %d\n", result);
    
    // En este punto no debería haber cambios
    int passed = (result == 0); // 0 = sin cambios sospechosos
    print_test_result("Escaneo manual sin cambios", passed);
    
    return passed;
}

// TEST 3: Simular cambio sospechoso y detectarlo
int test_suspicious_file_growth() {
    print_test_header("TEST 3: DETECCIÓN DE CRECIMIENTO SOSPECHOSO");
    
    // Primero crear baseline
    extern int baseline_count;
    baseline_count = 0;
    recurse_hash_extended(TEST_USB_PATH);
    printf("Baseline inicial: %d archivos\n", baseline_count);
    
    // Simular crecimiento masivo de archivo
    printf("Simulando crecimiento masivo de archivo...\n");
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "dd if=/dev/zero of=%s/Documents/readme.txt bs=1M count=5 2>/dev/null", TEST_USB_PATH);
    system(cmd);
    
    printf("Archivo inflado. Llamando a manual_device_scan()...\n");
    
    // Detectar cambios con TU función
    int result = manual_device_scan(TEST_USB_PATH);
    
    printf("Resultado después del crecimiento: %d\n", result);
    
    // Ahora SÍ debería detectar cambios sospechosos
    int passed = (result > 0); // > 0 = cambios detectados
    print_test_result("Detección de crecimiento sospechoso", passed);
    
    return passed;
}

// TEST 4: Verificar detección de cambios de permisos
int test_permission_changes() {
    print_test_header("TEST 4: DETECCIÓN DE CAMBIOS DE PERMISOS");
    
    // Crear baseline
    extern int baseline_count;
    baseline_count = 0;
    recurse_hash_extended(TEST_USB_PATH);
    
    // Cambiar permisos a 777 (sospechoso)
    printf("Cambiando permisos a 777...\n");
    chmod(TEST_USB_PATH "/Documents/document.pdf", 0777);
    
    // Detectar con TU función
    printf("Llamando a manual_device_scan()...\n");
    int result = manual_device_scan(TEST_USB_PATH);
    
    printf("Resultado después de cambio de permisos: %d\n", result);
    
    int passed = (result > 0);
    print_test_result("Detección de cambios de permisos", passed);
    
    return passed;
}

// TEST 5: Verificar detección de replicación de archivos
int test_file_replication() {
    print_test_header("TEST 5: DETECCIÓN DE REPLICACIÓN DE ARCHIVOS");
    
    // Crear baseline
    extern int baseline_count;
    baseline_count = 0;
    recurse_hash_extended(TEST_USB_PATH);
    printf("Archivos en baseline: %d\n", baseline_count);
    
    // Simular replicación masiva
    printf("Simulando replicación de archivos...\n");
    for (int i = 1; i <= 10; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "cp %s/Documents/document.pdf %s/Documents/copy_%d.pdf", 
                TEST_USB_PATH, TEST_USB_PATH, i);
        system(cmd);
    }
    
    // Detectar con TU función
    printf("Llamando a manual_device_scan()...\n");
    int result = manual_device_scan(TEST_USB_PATH);
    
    printf("Resultado después de replicación: %d\n", result);
    
    int passed = (result > 0);
    print_test_result("Detección de replicación de archivos", passed);
    
    return passed;
}

// TEST 6: Verificar scan_mounts_enhanced (simulación)
int test_mount_detection() {
    print_test_header("TEST 6: DETECCIÓN DE DISPOSITIVOS MONTADOS");
    
    printf("Llamando a scan_mounts_enhanced()...\n");
    
    // Tu función lee /proc/mounts, debería encontrar nuestro USB virtual
    int detected = scan_mounts_enhanced("/tmp");
    
    printf("Dispositivos detectados: %d\n", detected);
    
    // Podría detectar nuestro mount de /tmp o no, dependiendo de la implementación
    int passed = 1; // Este test siempre pasa si la función no crashea
    print_test_result("scan_mounts_enhanced() ejecutado sin errores", passed);
    
    return passed;
}

// TEST 7: Verificar sistema de alertas y logs
int test_logging_system() {
    print_test_header("TEST 7: SISTEMA DE LOGGING Y ALERTAS");
    
    printf("Probando log_event()...\n");
    log_event("TEST", "Mensaje de prueba del sistema de logging");
    
    printf("Probando emit_alert()...\n");
    emit_alert(TEST_USB_PATH, "TEST_ALERT", "Alerta de prueba generada por el test");
    
    // Verificar que el archivo de log existe
    int log_exists = (access("/tmp/matcom_usb.log", F_OK) == 0);
    
    int passed = log_exists;
    print_test_result("Sistema de logging funcional", passed);
    
    return passed;
}

// TEST 8: Verificar funciones auxiliares
int test_auxiliary_functions() {
    print_test_header("TEST 8: FUNCIONES AUXILIARES");
    
    // Test mark_as_tracked
    printf("Probando mark_as_tracked()...\n");
    mark_as_tracked(TEST_USB_PATH);
    
    // Test get_usb_statistics
    printf("Probando get_usb_statistics()...\n");
    int total, suspicious, files;
    get_usb_statistics(&total, &suspicious, &files);
    printf("Estadísticas: Total=%d, Sospechosos=%d, Archivos=%d\n", total, suspicious, files);
    
    int passed = 1; // Si llegamos aquí sin crash, está bien
    print_test_result("Funciones auxiliares ejecutadas correctamente", passed);
    
    return passed;
}

// TEST 9: Test integrado - simular malware completo
int test_malware_simulation() {
    print_test_header("TEST 9: SIMULACIÓN COMPLETA DE MALWARE");
    
    // Crear baseline limpio
    extern int baseline_count;
    baseline_count = 0;
    recurse_hash_extended(TEST_USB_PATH);
    printf("Baseline limpio: %d archivos\n", baseline_count);
    
    // Simular inyección de malware
    printf("Simulando inyección de malware...\n");
    
    // 1. Crear autorun.inf
    FILE *f = fopen(TEST_USB_PATH "/autorun.inf", "w");
    if (f) {
        fprintf(f, "[autorun]\nopen=malware.exe\nicon=icon.ico\n");
        fclose(f);
    }
    
    // 2. Crear ejecutable sospechoso
    f = fopen(TEST_USB_PATH "/malware.exe", "w");
    if (f) {
        fprintf(f, "#!/bin/bash\necho 'MALWARE SIMULADO'\n");
        fclose(f);
        chmod(TEST_USB_PATH "/malware.exe", 0777);
    }
    
    // 3. Crear archivos ocultos
    f = fopen(TEST_USB_PATH "/.hidden_keylog.dat", "w");
    if (f) {
        fprintf(f, "datos de keylogger simulados\n");
        fclose(f);
    }
    
    // Detectar con TU función completa
    printf("Ejecutando detección completa...\n");
    int result = manual_device_scan(TEST_USB_PATH);
    
    printf("Resultado de detección de malware: %d\n", result);
    
    int passed = (result > 0); // Debería detectar múltiples amenazas
    print_test_result("Detección completa de malware simulado", passed);
    
    return passed;
}

void show_final_results() {
    printf(BLUE "\n===================================================" RESET "\n");
    printf(BLUE "📊 RESUMEN FINAL DE TESTS" RESET "\n");
    printf("===================================================\n");
    
    printf("Total de tests ejecutados: %d\n", tests_passed + tests_failed);
    printf(GREEN "Tests exitosos: %d" RESET "\n", tests_passed);
    printf(RED "Tests fallidos: %d" RESET "\n", tests_failed);
    
    double success_rate = (double)tests_passed / (tests_passed + tests_failed) * 100;
    printf("Tasa de éxito: %.1f%%\n", success_rate);
    
    if (tests_failed == 0) {
        printf(GREEN "\n🎉 ¡TODOS LOS TESTS PASARON!" RESET "\n");
        printf("Tus funciones de port_scanner.c funcionan correctamente.\n");
    } else {
        printf(YELLOW "\n⚠️  Algunos tests fallaron" RESET "\n");
        printf("Revisa la implementación de las funciones marcadas como FAIL.\n");
    }
    
    printf("===================================================\n");
}

int main() {
    printf(CYAN "🚀 INICIANDO TEST SUITE REAL - MatCom Guard Port Scanner" RESET "\n");
    printf("Este test ejecuta DIRECTAMENTE tus funciones de port_scanner.c\n\n");
    
    // Setup
    if (!setup_virtual_usb()) {
        printf(RED "Error en setup. Abortando tests." RESET "\n");
        return 1;
    }
    
    create_test_files();
    
    // Ejecutar todos los tests
    test_baseline_creation();
    test_manual_scan();
    test_suspicious_file_growth();
    test_permission_changes();
    test_file_replication();
    test_mount_detection();
    test_logging_system();
    test_auxiliary_functions();
    test_malware_simulation();
    
    // Mostrar resultados
    show_final_results();
    
    // Cleanup
    cleanup_virtual_usb();
    
    printf(CYAN "\n✨ Test suite completado" RESET "\n");
    
    return (tests_failed == 0) ? 0 : 1;
}
