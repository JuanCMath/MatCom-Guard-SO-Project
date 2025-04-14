#include <gtk/gtk.h>
#include "gui.h"

static void on_button_click(GtkWidget *widget, gpointer data) {
    g_print("Iniciando escaneo...\n");
    // Llamar a funciones de monitoreo aquí
}

void init_gui(int argc, char **argv) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "MatCom Guard");
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *button = gtk_button_new_with_label("Escanear");
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_click), NULL);

    gtk_container_add(GTK_CONTAINER(window), button);
    gtk_widget_show_all(window);
    gtk_main();
}