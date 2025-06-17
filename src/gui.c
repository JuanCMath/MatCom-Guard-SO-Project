#include "gui.h"
#include <gtk/gtk.h>

static GtkWidget *log_view;

void update_gui_log(const char *message) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_view));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, message, -1);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);
}

static gpointer scan_thread_func(gpointer data) {
    extern void run_port_scanner_gui(void);
    run_port_scanner_gui();
    return NULL;
}

static void on_scan_button_clicked(GtkWidget *widget, gpointer data) {
    g_thread_new("scan_thread", scan_thread_func, NULL);
}

void init_gui(int argc, char **argv) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "MatCom Guard");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    log_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(log_view), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), log_view, TRUE, TRUE, 0);

    GtkWidget *button = gtk_button_new_with_label("Iniciar escaneo");
    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(window), vbox);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(button, "clicked", G_CALLBACK(on_scan_button_clicked), NULL);

    gtk_widget_show_all(window);
    gtk_main();
}