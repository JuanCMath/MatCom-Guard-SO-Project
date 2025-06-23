#ifndef GUI_INTERNAL_H
#define GUI_INTERNAL_H

#include <gtk/gtk.h>
#include <stdbool.h>
#include <time.h>
#include "gui.h"

// Variables principales de la ventana (definidas en gui_main.c)
extern GtkWidget *main_window;
extern GtkWidget *main_container;
extern GtkWidget *header_bar;
extern GtkWidget *status_bar;
extern GtkWidget *notebook;

// Variables de callbacks del backend (definidas en gui_main.c)
extern ScanUSBCallback usb_callback;
extern ScanProcessesCallback processes_callback;
extern ScanPortsCallback ports_callback;
extern ExportReportCallback report_callback;

// Variables específicas del sistema de logging (definidas en gui_logging.c)
extern GtkWidget *log_text_view;
extern GtkTextBuffer *log_buffer;
extern GtkTextTag *info_tag;
extern GtkTextTag *warning_tag;
extern GtkTextTag *error_tag;
extern GtkTextTag *alert_tag;

// Variables específicas del panel de estadísticas (definidas en gui_stats.c)
extern GtkWidget *stats_usb_count;
extern GtkWidget *stats_usb_suspicious;
extern GtkWidget *stats_process_count;
extern GtkWidget *stats_ports_open;
extern GtkWidget *stats_system_status;
extern GtkWidget *stats_last_scan;

// Variables específicas de la barra de estado (definidas en gui_status.c)
extern GtkWidget *status_system_indicator;
extern GtkWidget *status_last_scan_time;
extern GtkWidget *status_datetime;
extern GtkWidget *status_scanning_indicator;
extern gboolean is_scanning;

// Funciones del sistema de logging (implementadas en gui_logging.c)
GtkWidget* create_log_area(void);

// Funciones del panel de estadísticas (implementadas en gui_stats.c)
GtkWidget* create_statistics_panel(void);

// Funciones de la barra de estado (implementadas en gui_status.c)
GtkWidget* create_status_bar(void);
gboolean update_datetime_status(gpointer user_data);

// Funciones del panel USB (implementadas en gui_usb_panel.c)
GtkWidget* create_usb_panel(void);

// Funciones del panel de procesos (implementadas en gui_process_panel.c)
GtkWidget* create_process_panel(void);

// Funciones del panel de puertos (implementadas en gui_ports_panel.c)
GtkWidget* create_ports_panel(void);

// Funciones del diálogo de configuración (implementadas en gui_config_dialog.c)
void show_config_dialog(GtkWindow *parent);
gdouble get_cpu_threshold(void);
gdouble get_mem_threshold(void);
gint get_usb_scan_interval(void);
gint get_process_scan_interval(void);
gint get_port_scan_interval(void);
gboolean is_auto_scan_usb_enabled(void);
gboolean is_auto_scan_processes_enabled(void);
gboolean is_auto_scan_ports_enabled(void);
gboolean is_sound_alerts_enabled(void);
gboolean is_notifications_enabled(void);
gboolean is_process_whitelisted(const char *process_name);

#endif