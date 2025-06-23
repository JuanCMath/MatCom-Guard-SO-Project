#include "gui_internal.h"
#include "gui.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Panel de estadísticas
GtkWidget *stats_usb_count = NULL;
GtkWidget *stats_usb_suspicious = NULL;
GtkWidget *stats_process_count = NULL;
GtkWidget *stats_ports_open = NULL;
GtkWidget *stats_system_status = NULL;
GtkWidget *stats_last_scan = NULL;

static GtkWidget* create_stat_widget(const char *icon_text, const char *label_text, const char *initial_value, GtkWidget **value_widget) {
    GtkWidget *stat_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(stat_box, 15);
    gtk_widget_set_margin_end(stat_box, 15);
    gtk_widget_set_margin_top(stat_box, 10);
    gtk_widget_set_margin_bottom(stat_box, 10);
    
    GtkWidget *icon = gtk_label_new(icon_text);
    char icon_markup[256];
    snprintf(icon_markup, sizeof(icon_markup), "<span size='24000'>%s</span>", icon_text);
    gtk_label_set_markup(GTK_LABEL(icon), icon_markup);
    gtk_widget_set_halign(icon, GTK_ALIGN_CENTER);
    
    GtkWidget *value = gtk_label_new(initial_value);
    char value_markup[256];
    snprintf(value_markup, sizeof(value_markup), "<span size='x-large' weight='bold'>%s</span>", initial_value);
    gtk_label_set_markup(GTK_LABEL(value), value_markup);
    gtk_widget_set_halign(value, GTK_ALIGN_CENTER);
    
    GtkWidget *label = gtk_label_new(label_text);
    char label_markup[256];
    snprintf(label_markup, sizeof(label_markup), "<span size='small' color='#666666'>%s</span>", label_text);
    gtk_label_set_markup(GTK_LABEL(label), label_markup);
    gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
    
    gtk_box_pack_start(GTK_BOX(stat_box), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(stat_box), value, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(stat_box), label, FALSE, FALSE, 0);
    
    gtk_style_context_add_class(gtk_widget_get_style_context(stat_box), "stat-widget");
    
    *value_widget = value;
    
    return stat_box;
}

GtkWidget* create_statistics_panel() {
    // Crear el contenedor principal con orientación vertical
    GtkWidget *main_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_widget_set_margin_start(main_container, 20);
    gtk_widget_set_margin_end(main_container, 20);
    gtk_widget_set_margin_top(main_container, 20);
    gtk_widget_set_margin_bottom(main_container, 20);
    
    // Crear el título principal del panel con formato prominente
    GtkWidget *title = gtk_label_new("Estado del Sistema de Protección");
    char title_markup[512];
    snprintf(title_markup, sizeof(title_markup), 
        "<span size='x-large' weight='bold' color='#2196F3'>🛡️ Estado del Sistema de Protección</span>");
    gtk_label_set_markup(GTK_LABEL(title), title_markup);
    gtk_widget_set_halign(title, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_bottom(title, 20);
    
    // Crear el contenedor horizontal para las estadísticas numéricas principales
    GtkWidget *stats_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_widget_set_halign(stats_row, GTK_ALIGN_CENTER);
    
    // Crear cada widget de estadística y almacenar las referencias para actualizaciones futuras
    // Estos widgets serán actualizados dinámicamente cuando cambie el estado del sistema
    GtkWidget *usb_container = create_stat_widget("💾", "Dispositivos USB", "0", &stats_usb_count);
    GtkWidget *suspicious_container = create_stat_widget("⚠️", "USB Sospechosos", "0", &stats_usb_suspicious);
    GtkWidget *process_container = create_stat_widget("⚡", "Procesos Monitoreados", "0", &stats_process_count);
    GtkWidget *ports_container = create_stat_widget("🔌", "Puertos Abiertos", "0", &stats_ports_open);
    
    // Agregar todos los contenedores a la fila principal de estadísticas
    gtk_box_pack_start(GTK_BOX(stats_row), usb_container, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(stats_row), suspicious_container, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(stats_row), process_container, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(stats_row), ports_container, TRUE, TRUE, 0);
    
    // Crear un separador visual para organizar la información
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top(separator, 15);
    gtk_widget_set_margin_bottom(separator, 15);
    
    // Crear la sección de estado del sistema (información cualitativa)
    GtkWidget *status_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 30);
    gtk_widget_set_halign(status_container, GTK_ALIGN_CENTER);
    
    // Widget para mostrar el estado general del sistema (Activo/Inactivo/Alerta)
    GtkWidget *status_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *status_icon = gtk_label_new("🟢");
    char status_icon_markup[128];
    snprintf(status_icon_markup, sizeof(status_icon_markup), "<span size='20000'>🟢</span>");
    gtk_label_set_markup(GTK_LABEL(status_icon), status_icon_markup);
    gtk_widget_set_halign(status_icon, GTK_ALIGN_CENTER);
    
    stats_system_status = gtk_label_new("Sistema Activo");
    char status_markup[256];
    snprintf(status_markup, sizeof(status_markup), "<span weight='bold' color='#4CAF50'>Sistema Activo</span>");
    gtk_label_set_markup(GTK_LABEL(stats_system_status), status_markup);
    gtk_widget_set_halign(stats_system_status, GTK_ALIGN_CENTER);
    
    GtkWidget *status_label = gtk_label_new("Estado General");
    char status_label_markup[128];
    snprintf(status_label_markup, sizeof(status_label_markup), "<span size='small' color='#666666'>Estado General</span>");
    gtk_label_set_markup(GTK_LABEL(status_label), status_label_markup);
    gtk_widget_set_halign(status_label, GTK_ALIGN_CENTER);
    
    gtk_box_pack_start(GTK_BOX(status_box), status_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(status_box), stats_system_status, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(status_box), status_label, FALSE, FALSE, 0);
    
    // Widget para mostrar información del último escaneo realizado
    GtkWidget *scan_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *scan_icon = gtk_label_new("🕐");
    char scan_icon_markup[128];
    snprintf(scan_icon_markup, sizeof(scan_icon_markup), "<span size='20000'>🕐</span>");
    gtk_label_set_markup(GTK_LABEL(scan_icon), scan_icon_markup);
    gtk_widget_set_halign(scan_icon, GTK_ALIGN_CENTER);
    
    stats_last_scan = gtk_label_new("Nunca");
    char scan_markup[256];
    snprintf(scan_markup, sizeof(scan_markup), "<span weight='bold'>Nunca</span>");
    gtk_label_set_markup(GTK_LABEL(stats_last_scan), scan_markup);
    gtk_widget_set_halign(stats_last_scan, GTK_ALIGN_CENTER);
    
    GtkWidget *scan_label = gtk_label_new("Último Escaneo");
    char scan_label_markup[128];
    snprintf(scan_label_markup, sizeof(scan_label_markup), "<span size='small' color='#666666'>Último Escaneo</span>");
    gtk_label_set_markup(GTK_LABEL(scan_label), scan_label_markup);
    gtk_widget_set_halign(scan_label, GTK_ALIGN_CENTER);
    
    gtk_box_pack_start(GTK_BOX(scan_box), scan_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(scan_box), stats_last_scan, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(scan_box), scan_label, FALSE, FALSE, 0);
    
    // Agregar las secciones de estado al contenedor
    gtk_box_pack_start(GTK_BOX(status_container), status_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(status_container), scan_box, FALSE, FALSE, 0);
    
    // Ensamblar todo el panel en el contenedor principal
    gtk_box_pack_start(GTK_BOX(main_container), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_container), stats_row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_container), separator, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_container), status_container, FALSE, FALSE, 0);
    
    return main_container;
}


void gui_update_statistics(int usb_devices, int processes, int open_ports) {
    if (!stats_usb_count || !stats_process_count || !stats_ports_open) {
        return;
    }
    
    // Actualizar contador de dispositivos USB con formato visual atractivo
    char usb_text[64];
    snprintf(usb_text, sizeof(usb_text), "<span size='large' weight='bold' color='%s'>%d</span>", 
             usb_devices > 5 ? "#FF9800" : "#2196F3", usb_devices);
    gtk_label_set_markup(GTK_LABEL(stats_usb_count), usb_text);
    
    // Actualizar contador de procesos monitoreados
    char process_text[64];
    snprintf(process_text, sizeof(process_text), "<span size='large' weight='bold' color='%s'>%d</span>", 
             processes > 50 ? "#FF5722" : "#4CAF50", processes);
    gtk_label_set_markup(GTK_LABEL(stats_process_count), process_text);
    
    // Actualizar contador de puertos abiertos
    char ports_text[64];
    const char *port_color;
    if (open_ports > 20) port_color = "#F44336"; 
    else if (open_ports > 10) port_color = "#FF9800"; 
    else port_color = "#4CAF50";                       
    
    snprintf(ports_text, sizeof(ports_text), "<span size='large' weight='bold' color='%s'>%d</span>", 
             port_color, open_ports);
    gtk_label_set_markup(GTK_LABEL(stats_ports_open), ports_text);
    
    // Calcular y mostrar dispositivos USB sospechosos
    int suspicious_usb = 0; // Por ahora 0, pero esto viene del backend
    char suspicious_text[64];
    snprintf(suspicious_text, sizeof(suspicious_text), "<span size='large' weight='bold' color='%s'>%d</span>", 
             suspicious_usb > 0 ? "#F44336" : "#4CAF50", suspicious_usb);
    gtk_label_set_markup(GTK_LABEL(stats_usb_suspicious), suspicious_text);
    
    // Actualizar timestamp del último escaneo
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);
    
    char scan_text[128];
    snprintf(scan_text, sizeof(scan_text), "<span weight='bold' color='#2196F3'>%s</span>", timestamp);
    gtk_label_set_markup(GTK_LABEL(stats_last_scan), scan_text);
    
    // Agregar entrada al log 
    char log_message[256];
    snprintf(log_message, sizeof(log_message), 
             "Estadísticas actualizadas - USB: %d, Procesos: %d, Puertos: %d", 
             usb_devices, processes, open_ports);
    gui_add_log_entry("ESTADISTICAS", "INFO", log_message);
}