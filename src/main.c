#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <gui.h>

#define MOUNT_DIR "/media/"  // Cambia según tu distro: puede ser /run/media/$USER/
#define SLEEP_TIME 5         // Intervalo entre escaneos (segundos)

int main(int argc, char **argv) {
    init_gui(argc, argv);
    return 0;
}