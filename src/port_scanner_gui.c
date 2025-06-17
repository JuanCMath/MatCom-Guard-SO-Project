#include "port_scanner.h"
#include "gui.h"
#include <unistd.h>
#include <stdio.h>
#include <glib.h>

#define MOUNT_DIR "/media/"
#define SLEEP_TIME 5

static gboolean gui_log_idle_func(gpointer data) {
    update_gui_log((const char *)data);
    g_free(data); // Liberar la cadena duplicada
    return FALSE; // No repetir
}

void run_port_scanner_gui(void) {
    char *connected_devices[64];
    //int device_count = 0;

    while (1) {
        int new_count = scan_mounts(MOUNT_DIR, connected_devices);

        for (int i = 0; i < new_count; ++i) {
            if (!is_tracked(connected_devices[i])) {
                char msg[256];
                snprintf(msg, sizeof(msg), "📦 Nuevo dispositivo detectado: %s", connected_devices[i]);
                g_idle_add((GSourceFunc)update_gui_log, g_strdup(msg));

                if (scan_device(connected_devices[i]) == 0) {
                    g_idle_add((GSourceFunc)update_gui_log, g_strdup("🔍 Dispositivo escaneado sin amenazas."));
                } else {
                    snprintf(msg, sizeof(msg), "🚨 ¡ALERTA! Cambios sospechosos detectados en: %s", connected_devices[i]);
                    g_idle_add((GSourceFunc)update_gui_log, g_strdup(msg));
                }
                mark_as_tracked(connected_devices[i]);
            }
        }
        sleep(SLEEP_TIME);
    }
}