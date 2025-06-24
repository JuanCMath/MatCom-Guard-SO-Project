#include "gui_internal.h"
#include "gui.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Variables específicas para la barra de estado
GtkWidget *status_system_indicator = NULL;
GtkWidget *status_last_scan_time = NULL;
GtkWidget *status_datetime = NULL;
GtkWidget *status_scanning_indicator = NULL;
gboolean is_scanning = FALSE;

GtkWidget* create_status_bar() {
    GtkWidget *status_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_widget_set_margin_start(status_container, 15);
    gtk_widget_set_margin_end(status_container, 15);
    gtk_widget_set_margin_top(status_container, 8);
    gtk_widget_set_margin_bottom(status_container, 8);
    
    gtk_style_context_add_class(gtk_widget_get_style_context(status_container), "status-bar");
    
    // Indicador de estado del sistema
    status_system_indicator = gtk_label_new("🟢 Sistema Operativo");
    gtk_widget_set_tooltip_text(status_system_indicator, 
        "Indica el estado general del sistema de protección MatCom Guard");
    gtk_box_pack_start(GTK_BOX(status_container), status_system_indicator, FALSE, FALSE, 0);
    
    GtkWidget *separator1 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(status_container), separator1, FALSE, FALSE, 0);
    
    // Indicador de escaneo en progreso
    status_scanning_indicator = gtk_label_new("⏹️ Inactivo");
    gtk_widget_set_tooltip_text(status_scanning_indicator, 
        "Muestra si hay un escaneo de seguridad en progreso");
    gtk_box_pack_start(GTK_BOX(status_container), status_scanning_indicator, FALSE, FALSE, 0);
    
    GtkWidget *separator2 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(status_container), separator2, FALSE, FALSE, 0);
    
    // Tiempo del último escaneo completado
    status_last_scan_time = gtk_label_new("🕐 Último escaneo: Nunca");
    gtk_widget_set_tooltip_text(status_last_scan_time, 
        "Muestra cuándo se completó el último escaneo de seguridad");
    gtk_box_pack_start(GTK_BOX(status_container), status_last_scan_time, FALSE, FALSE, 0);
    
    GtkWidget *spacer = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(status_container), spacer, TRUE, TRUE, 0);
    
    // Fecha y hora actual del sistema
    status_datetime = gtk_label_new("📅 Cargando fecha...");
    gtk_widget_set_tooltip_text(status_datetime, 
        "Fecha y hora actual del sistema");
    gtk_box_pack_end(GTK_BOX(status_container), status_datetime, FALSE, FALSE, 0);
    
    // Actualización automática de fecha y hora cada segundo
    g_timeout_add_seconds(1, update_datetime_status, NULL);
    
    update_datetime_status(NULL);
    
    return status_container;
}

gboolean update_datetime_status(gpointer user_data __attribute__((unused))) {
    // Verificar que el widget existe antes de intentar actualizarlo
    if (!status_datetime) return G_SOURCE_REMOVE;
    
    // Obtener la fecha y hora actual del sistema
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    // Formatear la fecha y hora en un formato legible y profesional
    char datetime_str[128];
    strftime(datetime_str, sizeof(datetime_str), "📅 %A, %d %b %Y - %H:%M:%S", tm_info);
    
    // Actualizar el widget con la nueva información temporal
    gtk_label_set_text(GTK_LABEL(status_datetime), datetime_str);
    
    // Retornar TRUE para que la función se siga ejecutando periódicamente
    return G_SOURCE_CONTINUE;
}

void gui_set_scanning_status(gboolean scanning) {
    // Verificar que el widget de la barra de estado esté inicializado
    if (!status_scanning_indicator) return;
    
    // Actualizar el estado global de escaneo
    is_scanning = scanning;
    
    if (scanning) {
        // Mostrar indicador de escaneo activo con animación visual
        gtk_label_set_text(GTK_LABEL(status_scanning_indicator), "🔄 Escaneando...");
        gtk_widget_set_tooltip_text(status_scanning_indicator, 
            "MatCom Guard está realizando un escaneo completo del sistema");
        
        // Agregar registro de inicio de escaneo
        gui_add_log_entry("SCANNER", "INFO", "Iniciando escaneo completo del sistema");
    } else {
        // Mostrar estado inactivo
        gtk_label_set_text(GTK_LABEL(status_scanning_indicator), "⏹️ Inactivo");
        gtk_widget_set_tooltip_text(status_scanning_indicator, 
            "El sistema está en modo de monitoreo pasivo");
        
        // Actualizar el tiempo del último escaneo cuando termina
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char scan_time[128];
        strftime(scan_time, sizeof(scan_time), "🕐 Último escaneo: %H:%M:%S", tm_info);
        gtk_label_set_text(GTK_LABEL(status_last_scan_time), scan_time);
        
        // Agregar registro de finalización de escaneo
        gui_add_log_entry("SCANNER", "INFO", "Escaneo completo finalizado");
    }
}

void gui_update_system_status(const char *status, gboolean is_healthy) {
    // Verificar que el widget existe antes de intentar actualizarlo
    if (!status_system_indicator) return;
    
    // Configurar el indicador visual según el estado de salud del sistema
    if (is_healthy) {
        // Estado saludable: indicador verde
        char status_text[256];
        snprintf(status_text, sizeof(status_text), "🟢 %s", status);
        gtk_label_set_text(GTK_LABEL(status_system_indicator), status_text);
        gtk_widget_set_tooltip_text(status_system_indicator, 
            "Sistema funcionando correctamente - Todos los módulos operativos");
    } else {
        // Estado problemático: indicador rojo con advertencia
        char status_text[256];
        snprintf(status_text, sizeof(status_text), "🔴 %s", status);
        gtk_label_set_text(GTK_LABEL(status_system_indicator), status_text);
        gtk_widget_set_tooltip_text(status_system_indicator, 
            "Atención: Se han detectado problemas en el sistema");
        
        // Registrar el cambio de estado en el log para auditoría
        char log_message[512];
        snprintf(log_message, sizeof(log_message), "Estado del sistema cambió: %s", status);
        gui_add_log_entry("SISTEMA", "WARNING", log_message);
    }
}