CC = gcc
CFLAGS = -Wall -Wextra -Iinclude $(shell pkg-config --cflags gtk+-3.0)
LIBS = $(shell pkg-config --libs gtk+-3.0) -ludev -lssl -lcrypto -lpthread

SRC = src/main.c src/device_monitor.c src/process_monitor.c src/port_scanner.c src/gui.c

all: matcom-guard

matcom-guard: $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Ejemplo de monitoreo de procesos
process_monitor_example: src/process_monitor_gui.c src/process_monitor.c
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

clean:
	rm -f matcom-guard process_monitor_example