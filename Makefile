CC = gcc
CFLAGS = -Wall -Wextra -Iinclude $(shell pkg-config --cflags gtk+-3.0)
LIBS = $(shell pkg-config --libs gtk+-3.0) -ludev

SRC = src/main.c src/device_monitor.c src/process_monitor.c src/port_scanner.c src/gui.c

all: matcom-guard

matcom-guard: $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f matcom-guard