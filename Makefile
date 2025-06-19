CC = gcc
CFLAGS = -Wall -Wextra -Iinclude $(shell pkg-config --cflags gtk+-3.0)
LIBS = $(shell pkg-config --libs gtk+-3.0) -ludev -lssl -lcrypto

SRC = src/main.c src/device_monitor.c src/process_monitor.c src/auxiliar_methods.c src/port_scanner.c src/gui.c src/port_scanner_gui.c

all: matcom-guard

# Compilar el programa principal
matcom-guard: $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Compilar ejemplo de GUI para monitoreo de procesos (sin GTK si no está disponible)
process_monitor_gui: src/process_monitor_gui.c src/process_monitor.c src/auxiliar_methods.c
	$(CC) -Wall -Wextra -Iinclude -o $@ $^ -lpthread

test-monitoring: example
	./process_monitor_gui

clean:
	rm -f matcom-guard process_monitor_gui