#include "device_monitor.h"
#include "process_monitor.h"
#include "port_scanner.h"
#include "gui.h"

int main(int argc, char **argv) {
    // Ejemplo de uso sin GUI:
    monitor_usb_devices();
    monitor_processes();
    scan_ports();

    // O con GUI:
    // init_gui(argc, argv);
    return 0;
}