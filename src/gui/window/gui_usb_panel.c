#include "gui_internal.h"
#include "gui.h"
#include "gui_usb_integration.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Declaraciones de funciones privadas
static void format_files_column(GtkTreeViewColumn *column,
                               GtkCellRenderer *renderer,
                               GtkTreeModel *model,
                               GtkTreeIter *iter,
                               gpointer data);

// Variables del panel USB
static GtkWidget *usb_tree_view = NULL;
static GtkListStore *usb_list_store = NULL;
static GtkWidget *usb_info_label = NULL;
static GtkWidget *scan_usb_button = NULL;
static GtkWidget *refresh_usb_button = NULL; // Nueva variable para el botón actualizar
static GtkWidget *usb_panel_container = NULL;

// Columnas del TreeView
enum {
    COL_USB_ICON,
    COL_USB_DEVICE,
    COL_USB_MOUNT_POINT,
    COL_USB_STATUS,
    COL_USB_FILES_CHANGED,
    COL_USB_TOTAL_FILES,
    COL_USB_LAST_SCAN,
    COL_USB_STATUS_COLOR,
    NUM_USB_COLS
};

// Función para formatear el tiempo transcurrido
static char* format_time_ago(time_t scan_time) {
    static char buffer[64];
    if (scan_time == 0) {
        snprintf(buffer, sizeof(buffer), "Nunca");
        return buffer;
    }
    
    time_t now = time(NULL);
    int diff = (int)(now - scan_time);
    
    if (diff < 60) {
        snprintf(buffer, sizeof(buffer), "Hace %d seg", diff);
    } else if (diff < 3600) {
        snprintf(buffer, sizeof(buffer), "Hace %d min", diff / 60);
    } else {
        snprintf(buffer, sizeof(buffer), "Hace %d horas", diff / 3600);
    }
    
    return buffer;
}

// Callback para cuando se selecciona un dispositivo
static void on_usb_selection_changed(GtkTreeSelection *selection, gpointer data) {
    GtkTreeIter iter;
    GtkTreeModel *model;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gchar *device, *mount_point, *status;
        gint files_changed, total_files;
        
        gtk_tree_model_get(model, &iter,
                          COL_USB_DEVICE, &device,
                          COL_USB_MOUNT_POINT, &mount_point,
                          COL_USB_STATUS, &status,
                          COL_USB_FILES_CHANGED, &files_changed,
                          COL_USB_TOTAL_FILES, &total_files,
                          -1);
        
        // Actualizar el label de información
        char info_text[512];
        snprintf(info_text, sizeof(info_text),
                "<b>Dispositivo:</b> %s\n"
                "<b>Punto de montaje:</b> %s\n"
                "<b>Estado:</b> %s\n"
                "<b>Archivos modificados:</b> %d de %d",
                device, mount_point, status, files_changed, total_files);
        
        gtk_label_set_markup(GTK_LABEL(usb_info_label), info_text);
        
        g_free(device);
        g_free(mount_point);
        g_free(status);
    }
}

// Callback para re-habilitar botón de escaneo profundo después del escaneo
static gboolean re_enable_scan_button(gpointer data) {
    static gboolean cleanup_in_progress = FALSE;
    GtkButton *button = GTK_BUTTON(data);
    
    // Evitar múltiples ejecuciones simultáneas
    if (cleanup_in_progress) {
        return G_SOURCE_CONTINUE;
    }
    
    if (!is_gui_usb_scan_in_progress()) {
        cleanup_in_progress = TRUE;
        
        gtk_widget_set_sensitive(GTK_WIDGET(button), TRUE);
        gtk_button_set_label(button, "🔍 Escaneo Profundo");
        
        gui_add_log_entry("GUI_USB", "INFO", "✅ Botón Escaneo Profundo re-habilitado");
        
        cleanup_in_progress = FALSE;
        return G_SOURCE_REMOVE; // Detener el timeout
    }
    
    // Continuar verificando cada segundo
    return G_SOURCE_CONTINUE;
}

// Callback para re-habilitar botón de actualizar después del escaneo
static gboolean re_enable_refresh_button(gpointer data) {
    static gboolean cleanup_in_progress = FALSE;
    GtkButton *button = GTK_BUTTON(data);
    
    // Evitar múltiples ejecuciones simultáneas
    if (cleanup_in_progress) {
        return G_SOURCE_CONTINUE;
    }
    
    if (!is_gui_usb_scan_in_progress()) {
        cleanup_in_progress = TRUE;
        
        gtk_widget_set_sensitive(GTK_WIDGET(button), TRUE);
        gtk_button_set_label(button, "🔄 Actualizar");
        
        gui_add_log_entry("GUI_USB", "INFO", "✅ Botón Actualizar re-habilitado");
        
        cleanup_in_progress = FALSE;
        return G_SOURCE_REMOVE; // Detener el timeout
    }
    
    // Continuar verificando cada segundo
    return G_SOURCE_CONTINUE;
}

// Callback para el botón de escaneo USB
// Callback específico para el botón "Actualizar"
static void on_refresh_usb_clicked(GtkButton *button, gpointer data) {
    if (is_gui_usb_scan_in_progress()) {
        gui_add_log_entry("USB_SCANNER", "WARNING", "Escaneo USB ya en progreso");
        return;
    }
    
    gui_add_log_entry("USB_SCANNER", "INFO", "Actualizando snapshots de dispositivos USB");
    gui_set_scanning_status(TRUE);
    
    // Deshabilitar el botón durante la actualización
    gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
    gtk_button_set_label(button, "🔄 Actualizando...");
    
    // Iniciar actualización de snapshots
    refresh_usb_snapshots();
    
    // Verificar periódicamente si terminó la actualización
    g_timeout_add_seconds(1, re_enable_refresh_button, button);
}

// Callback específico para el botón "Escaneo Profundo"
static void on_scan_usb_clicked(GtkButton *button, gpointer data) {
    if (is_gui_usb_scan_in_progress()) {
        gui_add_log_entry("USB_SCANNER", "WARNING", "Escaneo USB ya en progreso");
        return;
    }
    
    gui_add_log_entry("USB_SCANNER", "INFO", "Iniciando escaneo profundo de dispositivos USB");
    gui_set_scanning_status(TRUE);
    
    // Deshabilitar el botón durante el escaneo
    gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
    gtk_button_set_label(button, "🔄 Escaneando...");
    
    // Iniciar escaneo profundo sin actualizar snapshots
    deep_scan_usb_devices();
    
    // Verificar periódicamente si terminó el escaneo
    g_timeout_add_seconds(1, re_enable_scan_button, button);
}

// Crear el panel principal de USB
GtkWidget* create_usb_panel() {
    // Contenedor principal
    usb_panel_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(usb_panel_container, 10);
    gtk_widget_set_margin_end(usb_panel_container, 10);
    gtk_widget_set_margin_top(usb_panel_container, 10);
    gtk_widget_set_margin_bottom(usb_panel_container, 10);
    
    // Barra de herramientas
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), 
        "<span size='large' weight='bold'>💾 Monitor de Dispositivos USB</span>");
    gtk_box_pack_start(GTK_BOX(toolbar), title, FALSE, FALSE, 0);
    
    // Espaciador
    GtkWidget *spacer = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(toolbar), spacer, TRUE, TRUE, 0);
      // Botón de actualizar
    refresh_usb_button = gtk_button_new_with_label("🔄 Actualizar");
    gtk_widget_set_tooltip_text(refresh_usb_button, "Actualizar lista de dispositivos y retomar snapshot");
    g_signal_connect(refresh_usb_button, "clicked", G_CALLBACK(on_refresh_usb_clicked), NULL);
    gtk_box_pack_end(GTK_BOX(toolbar), refresh_usb_button, FALSE, FALSE, 0);
    
    // Botón de escaneo
    scan_usb_button = gtk_button_new_with_label("🔍 Escaneo Profundo");
    gtk_widget_set_tooltip_text(scan_usb_button, "Realizar escaneo profundo comparando con snapshot anterior");
    g_signal_connect(scan_usb_button, "clicked", G_CALLBACK(on_scan_usb_clicked), NULL);
    gtk_box_pack_end(GTK_BOX(toolbar), scan_usb_button, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(usb_panel_container), toolbar, FALSE, FALSE, 0);
    
    // Separador
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(usb_panel_container), separator, FALSE, FALSE, 5);
    
    // Contenedor horizontal para la lista y la información
    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    
    // Crear el TreeView con scrolling
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled_window, 600, 300);
    
    // Crear el modelo de datos
    usb_list_store = gtk_list_store_new(NUM_USB_COLS,
                                        G_TYPE_STRING,  // Icon
                                        G_TYPE_STRING,  // Device
                                        G_TYPE_STRING,  // Mount point
                                        G_TYPE_STRING,  // Status
                                        G_TYPE_INT,     // Files changed
                                        G_TYPE_INT,     // Total files
                                        G_TYPE_STRING,  // Last scan
                                        G_TYPE_STRING); // Status color
    
    usb_tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(usb_list_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(usb_tree_view), TRUE);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(usb_tree_view), TRUE);
    
    // Crear las columnas
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    
    // Columna de icono
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("", renderer,
                                                     "text", COL_USB_ICON,
                                                     NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(usb_tree_view), column);
    
    // Columna de dispositivo
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Dispositivo", renderer,
                                                     "text", COL_USB_DEVICE,
                                                     NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_min_width(column, 150);
    gtk_tree_view_append_column(GTK_TREE_VIEW(usb_tree_view), column);
    
    // Columna de punto de montaje
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Punto de Montaje", renderer,
                                                     "text", COL_USB_MOUNT_POINT,
                                                     NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_min_width(column, 200);
    gtk_tree_view_append_column(GTK_TREE_VIEW(usb_tree_view), column);
    
    // Columna de estado con color
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Estado", renderer,
                                                     "text", COL_USB_STATUS,
                                                     "foreground", COL_USB_STATUS_COLOR,
                                                     NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(usb_tree_view), column);
    
    // Columna de archivos
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Archivos", renderer,
                                                     NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
        (GtkTreeCellDataFunc)format_files_column, NULL, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(usb_tree_view), column);
    
    // Columna de último escaneo
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Último Escaneo", renderer,
                                                     "text", COL_USB_LAST_SCAN,
                                                     NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(usb_tree_view), column);
    
    // Configurar selección
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(usb_tree_view));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    g_signal_connect(selection, "changed", G_CALLBACK(on_usb_selection_changed), NULL);
    
    gtk_container_add(GTK_CONTAINER(scrolled_window), usb_tree_view);
    gtk_box_pack_start(GTK_BOX(content_box), scrolled_window, TRUE, TRUE, 0);
    
    // Panel de información detallada
    GtkWidget *info_frame = gtk_frame_new("Información del Dispositivo");
    gtk_widget_set_size_request(info_frame, 250, -1);
    
    usb_info_label = gtk_label_new("Seleccione un dispositivo para ver detalles");
    gtk_label_set_line_wrap(GTK_LABEL(usb_info_label), TRUE);
    gtk_widget_set_margin_start(usb_info_label, 10);
    gtk_widget_set_margin_end(usb_info_label, 10);
    gtk_widget_set_margin_top(usb_info_label, 10);
    gtk_widget_set_margin_bottom(usb_info_label, 10);
    gtk_label_set_xalign(GTK_LABEL(usb_info_label), 0.0);
    
    gtk_container_add(GTK_CONTAINER(info_frame), usb_info_label);
    gtk_box_pack_start(GTK_BOX(content_box), info_frame, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(usb_panel_container), content_box, TRUE, TRUE, 0);
    
    // Barra de estado del panel USB
    GtkWidget *usb_status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(usb_status_bar, 5);
    
    GtkWidget *status_icon = gtk_label_new("ℹ️");
    gtk_box_pack_start(GTK_BOX(usb_status_bar), status_icon, FALSE, FALSE, 0);
    
    GtkWidget *status_text = gtk_label_new("Consejo: Los dispositivos marcados con ⚠️ tienen archivos modificados");
    gtk_widget_set_halign(status_text, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(usb_status_bar), status_text, TRUE, TRUE, 0);
    
    gtk_box_pack_end(GTK_BOX(usb_panel_container), usb_status_bar, FALSE, FALSE, 0);
    
    return usb_panel_container;
}

// Función auxiliar para formatear la columna de archivos
static void format_files_column(GtkTreeViewColumn *column,
                               GtkCellRenderer *renderer,
                               GtkTreeModel *model,
                               GtkTreeIter *iter,
                               gpointer data) {
    gint files_changed, total_files;
    gtk_tree_model_get(model, iter,
                      COL_USB_FILES_CHANGED, &files_changed,
                      COL_USB_TOTAL_FILES, &total_files,
                      -1);
    
    char text[64];
    if (files_changed > 0) {
        snprintf(text, sizeof(text), "%d/%d ⚠️", files_changed, total_files);
    } else {
        snprintf(text, sizeof(text), "%d/%d ✓", files_changed, total_files);
    }
    
    g_object_set(renderer, "text", text, NULL);
}

// Función pública para actualizar un dispositivo USB
void gui_update_usb_device(GUIUSBDevice *device) {
    if (!usb_list_store || !device) return;
    
    GtkTreeIter iter;
    gboolean found = FALSE;
    
    // Buscar si el dispositivo ya existe
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(usb_list_store), &iter)) {
        do {
            gchar *mount_point;
            gtk_tree_model_get(GTK_TREE_MODEL(usb_list_store), &iter,
                             COL_USB_MOUNT_POINT, &mount_point, -1);
            
            if (strcmp(mount_point, device->mount_point) == 0) {
                found = TRUE;
                g_free(mount_point);
                break;
            }
            g_free(mount_point);
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(usb_list_store), &iter));
    }
    
    // Si no existe, crear nueva entrada
    if (!found) {
        gtk_list_store_append(usb_list_store, &iter);
    }
    
    // Determinar icono y color según estado
    const char *icon = "💾";
    const char *color = "#2196F3";
    
    if (device->is_suspicious) {
        icon = "⚠️";
        color = "#F44336";
    } else if (strcmp(device->status, "LIMPIO") == 0) {
        icon = "✅";
        color = "#4CAF50";
    } else if (strcmp(device->status, "ESCANEANDO") == 0) {
        icon = "🔄";
        color = "#FF9800";
    }
    
    // Actualizar los datos
    gtk_list_store_set(usb_list_store, &iter,
                      COL_USB_ICON, icon,
                      COL_USB_DEVICE, device->device_name,
                      COL_USB_MOUNT_POINT, device->mount_point,
                      COL_USB_STATUS, device->status,
                      COL_USB_FILES_CHANGED, device->files_changed,
                      COL_USB_TOTAL_FILES, device->total_files,
                      COL_USB_LAST_SCAN, format_time_ago(device->last_scan),
                      COL_USB_STATUS_COLOR, color,
                      -1);
    
    // Actualizar estadísticas globales
    int total_devices = 0;
    int suspicious_devices = 0;
    
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(usb_list_store), &iter)) {
        do {
            total_devices++;
            gchar *status;
            gtk_tree_model_get(GTK_TREE_MODEL(usb_list_store), &iter,
                             COL_USB_STATUS, &status, -1);
            if (strcmp(status, "SOSPECHOSO") == 0) {
                suspicious_devices++;
            }
            g_free(status);
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(usb_list_store), &iter));
    }
    
    // Log del evento
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Dispositivo %s: %s - %d archivos modificados",
            device->device_name, device->status, device->files_changed);
    gui_add_log_entry("USB_MONITOR", 
                     device->is_suspicious ? "WARNING" : "INFO", 
                     log_msg);
}