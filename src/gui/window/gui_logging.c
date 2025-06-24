#include "gui_internal.h"
#include "gui.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

//Variables de logging
GtkWidget *log_text_view = NULL;
GtkTextBuffer *log_buffer = NULL;
GtkTextTag *info_tag = NULL;
GtkTextTag *warning_tag = NULL;
GtkTextTag *error_tag = NULL;
GtkTextTag *alert_tag = NULL;

GtkWidget* create_log_area() {
   GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_widget_set_size_request(scrolled_window, -1, 200);

   log_text_view = gtk_text_view_new();
   gtk_text_view_set_editable(GTK_TEXT_VIEW(log_text_view), FALSE);
   gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(log_text_view), FALSE);
   gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(log_text_view), GTK_WRAP_WORD);

   // Obtener el buffer de texto para manipular el contenido
   log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_text_view));

   // Crear etiquetas de formato para diferentes tipos de mensaje
   info_tag = gtk_text_buffer_create_tag(log_buffer, "info",
                                         "foreground", "#2196F3",
                                         "weight", PANGO_WEIGHT_NORMAL,
                                         NULL);

   warning_tag = gtk_text_buffer_create_tag(log_buffer, "warning",
                                            "foreground", "#FF9800",
                                            "weight", PANGO_WEIGHT_BOLD,
                                            NULL);

   error_tag = gtk_text_buffer_create_tag(log_buffer, "error",
                                          "foreground", "#F44336",
                                          "weight", PANGO_WEIGHT_BOLD,
                                          NULL);

   alert_tag = gtk_text_buffer_create_tag(log_buffer, "alert",
                                          "foreground", "#FFFFFF",
                                          "background", "#F44336",
                                          "weight", PANGO_WEIGHT_BOLD,
                                          NULL);

   gtk_container_add(GTK_CONTAINER(scrolled_window), log_text_view);

   gtk_text_buffer_set_text(log_buffer, 
       "🛡️ MatCom Guard - Sistema de Protección Iniciado\n"
       "Monitoreando dispositivos USB, procesos y puertos de red...\n\n", -1);

   return scrolled_window;
}

// Función helper para duplicar strings de forma segura
static char* safe_strdup(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char* copy = malloc(len);
    if (copy) {
        memcpy(copy, str, len);
    }
    return copy;
}

// Estructura para pasar datos entre hilos de forma segura
typedef struct {
    char *module;
    char *level;
    char *message;
} LogEntryData;

// Función que se ejecuta en el hilo principal de GTK
static gboolean add_log_entry_main_thread(gpointer user_data) {
    LogEntryData *data = (LogEntryData *)user_data;
    
    if (!log_buffer || !data) {
        if (data) {
            free(data->module);
            free(data->level);
            free(data->message);
            free(data);
        }
        return FALSE;
    }
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);
    
    char full_message[1024];
    snprintf(full_message, sizeof(full_message), 
             "[%s] %s | %s: %s\n", 
             timestamp, data->level, data->module, data->message);
    
    // Obtener el iterador al final del buffer de forma segura
    GtkTextIter end_iter;
    gtk_text_buffer_get_end_iter(log_buffer, &end_iter);
    
    // Seleccionar el tag apropiado
    GtkTextTag *tag = NULL;
    if (strcmp(data->level, "INFO") == 0) {
        tag = info_tag;
    } else if (strcmp(data->level, "WARNING") == 0) {
        tag = warning_tag;
    } else if (strcmp(data->level, "ERROR") == 0) {
        tag = error_tag;
    } else if (strcmp(data->level, "ALERT") == 0) {
        tag = alert_tag;
    }

    // Insertar el texto de forma segura
    if (tag) {
        gtk_text_buffer_insert_with_tags(log_buffer, &end_iter, 
                                         full_message, -1, tag, NULL);
    } else {
        gtk_text_buffer_insert(log_buffer, &end_iter, full_message, -1);
    }
    
    // Auto-scroll al final usando una marca del final del buffer
    gtk_text_buffer_get_end_iter(log_buffer, &end_iter);
    GtkTextMark *end_mark = gtk_text_buffer_create_mark(log_buffer, NULL, &end_iter, FALSE);    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(log_text_view), end_mark);
    gtk_text_buffer_delete_mark(log_buffer, end_mark);
    
    // También imprimir en consola para monitoreo del sistema
    printf("%s", full_message);
    
    // Liberar memoria
    free(data->module);
    free(data->level);
    free(data->message);
    free(data);
    
    return FALSE; // Solo ejecutar una vez
}

void gui_add_log_entry(const char *module, const char *level, const char *message) {
    if (!log_buffer || !module || !level || !message) return;
    
    // Crear una copia de los datos para pasarlos al hilo principal de forma segura
    LogEntryData *data = malloc(sizeof(LogEntryData));
    if (!data) return;
      data->module = safe_strdup(module);
    data->level = safe_strdup(level);
    data->message = safe_strdup(message);
    
    if (!data->module || !data->level || !data->message) {
        free(data->module);
        free(data->level);
        free(data->message);
        free(data);
        return;
    }
    
    // Programar la actualización del log en el hilo principal de GTK
    g_idle_add(add_log_entry_main_thread, data);
}