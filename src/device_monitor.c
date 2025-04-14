#include <stdio.h>
#include <stdlib.h>
#include <libudev.h>
#include "device_monitor.h"

void monitor_usb_devices() {
    struct udev *udev = udev_new();
    if (!udev) {
        fprintf(stderr, "Error al inicializar udev\n");
        return;
    }

    // Monitorear dispositivos USB
    struct udev_enumerate *enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "usb");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry *entry;

    printf("Dispositivos USB conectados:\n");
    udev_list_entry_foreach(entry, devices) {
        struct udev_device *dev = udev_device_new_from_syspath(udev, udev_list_entry_get_name(entry));
        printf("- %s\n", udev_device_get_sysattr_value(dev, "product"));
        udev_device_unref(dev);
    }

    udev_enumerate_unref(enumerate);
    udev_unref(udev);
}