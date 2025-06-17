#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "scanner.h"

#define MOUNT_DIR "/media/"  // Cambia según tu distro: puede ser /run/media/$USER/
#define SLEEP_TIME 5         // Intervalo entre escaneos (segundos)

int main() {
    init_gui(argc, argv);
    printf("🛡️  Iniciando patrullas fronterizas USB...\n");

    char *connected_devices[64];
    int device_count = 0;

    while (1) {
        int new_count = scan_mounts(MOUNT_DIR, connected_devices);

        for (int i = 0; i < new_count; ++i) {
            if (!is_tracked(connected_devices[i])) {
                printf("📦 Nuevo dispositivo detectado: %s\n", connected_devices[i]);
                if (scan_device(connected_devices[i]) == 0) {
                    printf("🔍 Dispositivo escaneado sin amenazas.\n");
                } else {
                    printf("🚨 ¡ALERTA! Cambios sospechosos detectados en: %s\n", connected_devices[i]);
                }
                mark_as_tracked(connected_devices[i]);
            }
        }

        sleep(SLEEP_TIME);
    }

    return 0;
}