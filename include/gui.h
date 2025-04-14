#ifndef GUI_H  
#define GUI_H  

#include <gtk/gtk.h>

void init_gui(int argc, char **argv);
void update_gui_log(const char *message);  // Para mostrar alertas en la interfaz

#endif  