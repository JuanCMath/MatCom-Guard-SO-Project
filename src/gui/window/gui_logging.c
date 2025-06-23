#include "gui_internal.h"
#include "gui.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

void gui_add_log_entry(const char *module, const char *level, const char *message) {
    if (!log_buffer) return;
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);
    
    char full_message[1024];
    snprintf(full_message, sizeof(full_message), 
             "[%s] %s | %s: %s\n", 
             timestamp, level, module, message);
    
    // Iterador al final del buffer de texto
    GtkTextIter end_iter;
    gtk_text_buffer_get_end_iter(log_buffer, &end_iter);
    
    GtkTextTag *tag = NULL;
    if (strcmp(level, "INFO") == 0) {
        tag = info_tag;
    } else if (strcmp(level, "WARNING") == 0) {
        tag = warning_tag;
    } else if (strcmp(level, "ERROR") == 0) {
        tag = error_tag;
    } else if (strcmp(level, "ALERT") == 0) {
        tag = alert_tag;
    }

    if (tag) {
        gtk_text_buffer_insert_with_tags(log_buffer, &end_iter, 
                                         full_message, -1, tag, NULL);
    } else {
        gtk_text_buffer_insert(log_buffer, &end_iter, full_message, -1);
    }
    
    // Auto-scroll al final para mostrar siempre el mensaje más reciente
    GtkTextMark *end_mark = gtk_text_buffer_get_insert(log_buffer);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(log_text_view), end_mark);
    
    // Imprimir también en consola para debugging
    printf("%s", full_message);
}