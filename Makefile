CC = gcc
CFLAGS = -Wall -Wextra `pkg-config --cflags gtk+-3.0`
LIBS = `pkg-config --libs gtk+-3.0` -ludev

all: matcom-guard

matcom-guard: src/main.c src/device_monitor.c src/process_monitor.c src/port_scanner.c src/gui.c
    $(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
    rm -f matcom-guard