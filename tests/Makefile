# Makefile para programas de prueba
CC = gcc
CFLAGS = -Wall -Wextra -g -O2

# Programas de prueba
TESTS = high_cpu_process memory_leak_process normal_process whitelisted_process

all: $(TESTS)

high_cpu_process: high_cpu_process.c
	$(CC) $(CFLAGS) -o $@ $<

memory_leak_process: memory_leak_process.c
	$(CC) $(CFLAGS) -o $@ $<

normal_process: normal_process.c
	$(CC) $(CFLAGS) -o $@ $<

whitelisted_process: whitelisted_process.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TESTS)

.PHONY: all clean

# Reglas de ayuda
help:
	@echo "=== PROGRAMAS DE PRUEBA MATCOM GUARD ==="
	@echo ""
	@echo "Compilar todos: make"
	@echo "Limpiar:       make clean"
	@echo ""
	@echo "PROGRAMAS DISPONIBLES:"
	@echo ""
	@echo "1. high_cpu_process [duración_seg]"
	@echo "   - Simula proceso con alto uso de CPU"
	@echo "   - Ejemplo: ./high_cpu_process 30"
	@echo ""
	@echo "2. memory_leak_process [max_mb] [delay_seg]"
	@echo "   - Simula proceso con fuga de memoria"
	@echo "   - Ejemplo: ./memory_leak_process 80 3"
	@echo ""
	@echo "3. normal_process [duración_seg] [intervalo_reporte]"
	@echo "   - Proceso normal de larga duración"
	@echo "   - Ejemplo: ./normal_process 180 15"
	@echo ""
	@echo "4. whitelisted_process [duración_seg]"
	@echo "   - Proceso stress en lista blanca"
	@echo "   - Ejemplo: ./whitelisted_process 45"
	@echo ""
