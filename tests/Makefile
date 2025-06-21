# Makefile para tests de MatCom Guard
CC = gcc
CFLAGS = -Wall -Wextra -I../include -pthread
LIBS = -lssl -lcrypto -lpthread

# Directorio de fuentes
SRC_DIR = ../src

# Archivo fuente principal
PORT_SCANNER_SRC = $(SRC_DIR)/port_scanner.c

# Test executables
TEST_REAL = real_function_test

.PHONY: all clean test-real run-test help

all: test-real

# Compilar test real que ejecuta las funciones directamente
test-real: $(TEST_REAL)

$(TEST_REAL): real_function_test.c $(PORT_SCANNER_SRC)
	@echo "🔨 Compilando test real con funciones de port_scanner.c..."
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) \
		-DTEST_BUILD \
		-Wno-unused-parameter \
		-Wno-unused-variable

# Ejecutar el test
run-test: $(TEST_REAL)
	@echo "🚀 Ejecutando test real de funciones..."
	@sudo $(TEST_REAL)

# Limpiar archivos compilados
clean:
	@echo "🧹 Limpiando archivos de test..."
	rm -f $(TEST_REAL)
	rm -f /tmp/test_virtual_usb /tmp/test_usb.img
	sudo umount /tmp/test_virtual_usb 2>/dev/null || true

# Información de ayuda
help:
	@echo "Targets disponibles:"
	@echo "  all       - Compila todos los tests"
	@echo "  test-real - Compila el test de funciones reales"
	@echo "  run-test  - Ejecuta el test real"
	@echo "  clean     - Limpia archivos generados"
	@echo "  help      - Muestra esta ayuda"
