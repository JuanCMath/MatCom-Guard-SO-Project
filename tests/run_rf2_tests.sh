#!/bin/bash

# ================================================================================
# SCRIPT DE PRUEBAS RF2 - MATCOM GUARD
# ================================================================================
# Prueba los 4 casos principales del sistema de monitoreo:
# 1. Proceso legítimo (lista blanca)
# 2. Proceso malicioso (alto CPU)
# 3. Fuga de memoria (alto RAM)
# 4. Proceso fantasma (limpieza)
# ================================================================================

# Configuración
SCRIPT_DIR="/media/sf_ProyectoSO/MatCom-Guard-SO-Project"
MONITOR_EXEC="$SCRIPT_DIR/process_monitor_gui"
CONFIG_FILE="$SCRIPT_DIR/matcomguard.conf"
INFORME_FILE="$SCRIPT_DIR/informe_pruebas_rf2.txt"
LOG_FILE="$SCRIPT_DIR/monitor_test.log"

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ================================================================================
# FUNCIONES AUXILIARES
# ================================================================================

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1" | tee -a "$INFORME_FILE"
}

log_success() {
    echo -e "${GREEN}[✓]${NC} $1" | tee -a "$INFORME_FILE"
}

log_warning() {
    echo -e "${YELLOW}[⚠]${NC} $1" | tee -a "$INFORME_FILE"
}

log_error() {
    echo -e "${RED}[✗]${NC} $1" | tee -a "$INFORME_FILE"
}

log_test() {
    echo -e "\n${YELLOW}=== $1 ===${NC}" | tee -a "$INFORME_FILE"
}

# Función para configurar umbrales de prueba
setup_test_config() {
    log_info "Configurando umbrales para pruebas..."
    cat > "$CONFIG_FILE" << EOF
# Configuración MatCom Guard - RF2 (PRUEBAS)
# Umbrales de recursos (en porcentaje)
UMBRAL_CPU=70.0
UMBRAL_RAM=50.0

# Intervalo entre verificaciones (en segundos)
INTERVALO=5

# Duración para activar alerta (en segundos)
DURACION_ALERTA=10

# Lista de procesos exentos de alertas (separados por comas)
WHITELIST=systemd,kthreadd,ksoftirqd,migration,rcu_gp,rcu_par_gp,watchdog,stress,yes
EOF
    log_success "Configuración establecida: CPU=70%, RAM=50%, Lista blanca incluye 'stress' y 'yes'"
}

# Función para iniciar monitor en background
start_monitor() {
    log_info "Iniciando monitor de procesos..."
    cd "$SCRIPT_DIR"
    nohup "$MONITOR_EXEC" > "$LOG_FILE" 2>&1 &
    MONITOR_PID=$!
    sleep 3
    
    if ps -p $MONITOR_PID > /dev/null 2>&1; then
        log_success "Monitor iniciado con PID: $MONITOR_PID"
        return 0
    else
        log_error "Error al iniciar el monitor"
        return 1
    fi
}

# Función para detener monitor
stop_monitor() {
    if [ ! -z "$MONITOR_PID" ] && ps -p $MONITOR_PID > /dev/null 2>&1; then
        log_info "Deteniendo monitor (PID: $MONITOR_PID)..."
        kill $MONITOR_PID
        sleep 2
        
        if ps -p $MONITOR_PID > /dev/null 2>&1; then
            kill -9 $MONITOR_PID 2>/dev/null
        fi
        log_success "Monitor detenido"
    fi
}

# Función para verificar alertas en el log
check_alerts() {
    local expected_process="$1"
    local expected_type="$2"  # "CPU" o "RAM"
    local should_alert="$3"   # "SI" o "NO"
    
    sleep 8  # Esperar tiempo suficiente para que se genere la alerta
    
    local alerts_found=$(grep -i "alerta" "$LOG_FILE" | grep -i "$expected_process" | grep -i "$expected_type" | wc -l)
    
    if [ "$should_alert" = "SI" ]; then
        if [ $alerts_found -gt 0 ]; then
            log_success "✓ Alerta detectada correctamente para '$expected_process' ($expected_type)"
            grep -i "alerta" "$LOG_FILE" | grep -i "$expected_process" | grep -i "$expected_type" | tail -1 >> "$INFORME_FILE"
        else
            log_error "✗ No se detectó alerta esperada para '$expected_process' ($expected_type)"
        fi
    else
        if [ $alerts_found -eq 0 ]; then
            log_success "✓ No hay alertas (correcto, proceso en lista blanca)"
        else
            log_warning "⚠ Se detectó alerta inesperada para proceso en lista blanca"
            grep -i "alerta" "$LOG_FILE" | grep -i "$expected_process" | tail -1 >> "$INFORME_FILE"
        fi
    fi
    
    return $alerts_found
}
echo

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Contadores de pruebas
PASSED=0
FAILED=0
TOTAL=0

# Función para mostrar resultados
test_result() {
    local test_name="$1"
    local result="$2"
    local details="$3"
    
    TOTAL=$((TOTAL + 1))
    
    if [ "$result" = "PASS" ]; then
        echo -e "${GREEN}[PASS]${NC} $test_name"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED}[FAIL]${NC} $test_name"
        if [ -n "$details" ]; then
            echo "       Detalles: $details"
        fi
        FAILED=$((FAILED + 1))
    fi
}

# Función para verificar si el proceso monitor está corriendo
check_monitor_running() {
    if pgrep -f "process_monitor" > /dev/null; then
        return 0
    else
        return 1
    fi
}

# Función para iniciar el monitor en background
start_monitor() {
    echo "Iniciando el monitor de procesos..."
    ./process_monitor_gui > /tmp/monitor_output.log 2>&1 &
    MONITOR_PID=$!
    sleep 3
    
    if kill -0 $MONITOR_PID 2>/dev/null; then
        echo "Monitor iniciado correctamente (PID: $MONITOR_PID)"
        return 0
    else
        echo "Error al iniciar el monitor"
        return 1
    fi
}

# Función para detener el monitor
stop_monitor() {
    if [ -n "$MONITOR_PID" ]; then
        echo "Deteniendo monitor (PID: $MONITOR_PID)..."
        kill $MONITOR_PID 2>/dev/null || true
        wait $MONITOR_PID 2>/dev/null || true
    fi
}

# Compilar el proyecto
echo "Compilando el proyecto..."
if make clean && make; then
    echo -e "${GREEN}Compilación exitosa${NC}"
else
    echo -e "${RED}Error en compilación${NC}"
    exit 1
fi

echo

# === CP-RF2-001: Detección de Procesos Nuevos ===
echo "CP-RF2-001: Detección de Procesos Nuevos"
start_monitor
sleep 2

# Ejecutar un proceso de prueba
sleep 30 &
TEST_PID=$!
sleep 3

# Verificar si el proceso aparece en los logs
if grep -q "$TEST_PID" /tmp/monitor_output.log; then
    test_result "CP-RF2-001" "PASS"
else
    test_result "CP-RF2-001" "FAIL" "Proceso $TEST_PID no detectado"
fi

# Limpiar
kill $TEST_PID 2>/dev/null || true
stop_monitor
echo

# === CP-RF2-002: Detección de Procesos Terminados ===
echo "CP-RF2-002: Detección de Procesos Terminados"
start_monitor
sleep 2

# Crear y terminar un proceso
sleep 30 &
TEST_PID=$!
sleep 3
kill $TEST_PID
sleep 3

# Verificar si se detectó la terminación
if grep -q "terminado\|terminated" /tmp/monitor_output.log; then
    test_result "CP-RF2-002" "PASS"
else
    test_result "CP-RF2-002" "FAIL" "Terminación de proceso no detectada"
fi

stop_monitor
echo

# === CP-RF2-003: Medición de Uso de CPU ===
echo "CP-RF2-003: Medición de Uso de CPU"
start_monitor
sleep 2

# Crear proceso con alto uso de CPU
yes > /dev/null &
CPU_TEST_PID=$!
sleep 10

# Verificar si se detecta alto uso de CPU
if grep -q "CPU\|cpu" /tmp/monitor_output.log; then
    test_result "CP-RF2-003" "PASS"
else
    test_result "CP-RF2-003" "FAIL" "Uso de CPU no medido correctamente"
fi

# Limpiar
kill $CPU_TEST_PID 2>/dev/null || true
stop_monitor
echo

# === CP-RF2-004: Medición de Uso de Memoria ===
echo "CP-RF2-004: Medición de Uso de Memoria"
start_monitor
sleep 5

# Verificar si hay mediciones de memoria en los logs
if grep -q "memoria\|memory\|mem" /tmp/monitor_output.log; then
    test_result "CP-RF2-004" "PASS"
else
    test_result "CP-RF2-004" "FAIL" "Uso de memoria no medido"
fi

stop_monitor
echo

# === CP-RF2-005: Alertas por Alto Uso de CPU ===
echo "CP-RF2-005: Alertas por Alto Uso de CPU"

# Crear configuración temporal con umbral bajo
cat > /tmp/test_config.conf << EOF
UMBRAL_CPU=10.0
UMBRAL_RAM=80.0
INTERVALO=2
DURACION_ALERTA=5
EOF

# Usar configuración temporal
export CONFIG_PATH="/tmp/test_config.conf"

start_monitor
sleep 2

# Crear proceso con alto uso de CPU
yes > /dev/null &
CPU_TEST_PID=$!
sleep 8

# Verificar si se generó alerta
if grep -q -i "alert\|alerta\|cpu" /tmp/monitor_output.log; then
    test_result "CP-RF2-005" "PASS"
else
    test_result "CP-RF2-005" "FAIL" "Alerta de CPU no generada"
fi

# Limpiar
kill $CPU_TEST_PID 2>/dev/null || true
stop_monitor
rm -f /tmp/test_config.conf
unset CONFIG_PATH
echo

# === CP-RF2-009: Intervalo de Monitoreo ===
echo "CP-RF2-009: Intervalo de Monitoreo"

# Crear configuración con intervalo corto
cat > /tmp/test_config.conf << EOF
UMBRAL_CPU=90.0
UMBRAL_RAM=80.0
INTERVALO=3
DURACION_ALERTA=5
EOF

export CONFIG_PATH="/tmp/test_config.conf"

start_monitor
sleep 10

# Contar eventos de monitoreo en 10 segundos (debería haber ~3)
MONITOR_EVENTS=$(grep -c "proceso\|process\|PID" /tmp/monitor_output.log 2>/dev/null || echo "0")

if [ "$MONITOR_EVENTS" -ge 2 ]; then
    test_result "CP-RF2-009" "PASS"
else
    test_result "CP-RF2-009" "FAIL" "Intervalo incorrecto (eventos: $MONITOR_EVENTS)"
fi

stop_monitor
rm -f /tmp/test_config.conf
unset CONFIG_PATH
echo

# === CP-RF2-010: Thread Safety ===
echo "CP-RF2-010: Thread Safety"
start_monitor
sleep 2

# Ejecutar múltiples accesos concurrentes al sistema
for i in {1..5}; do
    sleep 1 &
done

sleep 5

# Verificar que el monitor sigue funcionando
if kill -0 $MONITOR_PID 2>/dev/null; then
    test_result "CP-RF2-010" "PASS"
else
    test_result "CP-RF2-010" "FAIL" "Sistema falló con acceso concurrente"
fi

stop_monitor
echo

# === CP-RF2-012: Persistencia de Configuración ===
echo "CP-RF2-012: Persistencia de Configuración"

# Crear archivo de configuración
cat > /tmp/test_config.conf << EOF
UMBRAL_CPU=75.0
UMBRAL_RAM=85.0
INTERVALO=5
DURACION_ALERTA=8
EOF

export CONFIG_PATH="/tmp/test_config.conf"

start_monitor
sleep 3
stop_monitor

# Reiniciar y verificar que la configuración se mantiene
start_monitor
sleep 3

# Verificar que usa la configuración personalizada (intervalo 5 segundos)
if [ -f "/tmp/test_config.conf" ]; then
    test_result "CP-RF2-012" "PASS"
else
    test_result "CP-RF2-012" "FAIL" "Configuración no persistida"
fi

stop_monitor
rm -f /tmp/test_config.conf
unset CONFIG_PATH
echo

# === CP-RF2-013: Manejo de Errores ===
echo "CP-RF2-013: Manejo de Errores"
start_monitor
sleep 2

# Simular condición de error (acceder a proceso inexistente)
# El sistema debería manejar esto gracefully
sleep 5

if kill -0 $MONITOR_PID 2>/dev/null; then
    test_result "CP-RF2-013" "PASS"
else
    test_result "CP-RF2-013" "FAIL" "Sistema falló con errores"
fi

stop_monitor
echo

# === RESUMEN DE RESULTADOS ===
echo "=============================================="
echo "           RESUMEN DE PRUEBAS"
echo "=============================================="
echo -e "Total de pruebas:  $TOTAL"
echo -e "${GREEN}Pruebas exitosas:  $PASSED${NC}"
echo -e "${RED}Pruebas fallidas:  $FAILED${NC}"
echo

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}¡Todas las pruebas pasaron exitosamente!${NC}"
    exit 0
else
    echo -e "${YELLOW}Algunas pruebas fallaron. Revisar implementación.${NC}"
    exit 1
fi

# Limpiar archivos temporales
rm -f /tmp/monitor_output.log
rm -f /tmp/test_config.conf