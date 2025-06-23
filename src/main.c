#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <gui.h>

#define MOUNT_DIR "/media/"
#define SLEEP_TIME 5

int main(int argc, char **argv) {
    printf("🛡️ MatCom Guard - Sistema de Protección Digital\n");
    printf("================================================\n\n");
    
    // Inicializar la interfaz gráfica
    init_gui(argc, argv);
    
    return 0;
}