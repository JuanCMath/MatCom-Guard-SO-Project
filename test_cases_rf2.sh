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

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Funciones de logging
log_info() { echo -e "${BLUE}[INFO]${NC} $1" | tee -a "$INFORME_FILE"; }
log_success() { echo -e "${GREEN}[✓]${NC} $1" | tee -a "$INFORME_FILE"; }
log_warning() { echo -e "${YELLOW}[⚠]${NC} $1" | tee -a "$INFORME_FILE"; }
log_error() { echo -e "${RED}[✗]${NC} $1" | tee -a "$INFORME_FILE"; }
log_test() { echo -e "\n${YELLOW}=== $1 ===${NC}" | tee -a "$INFORME_FILE"; }

# Configurar entorno de prueba
setup_test_config() {
    log_info "Configurando umbrales para pruebas..."
    # Crear config en el directorio local y copiarlo a /etc/
    cat > "$CONFIG_FILE" << EOF
UMBRAL_CPU=70.0
UMBRAL_RAM=50.0
INTERVALO=5
DURACION_ALERTA=10
WHITELIST=systemd,kthreadd,ksoftirqd,migration,rcu_gp,rcu_par_gp,watchdog,stress,yes
EOF
    # Copiar a /etc/ con sudo para que el monitor lo encuentre
    sudo cp "$CONFIG_FILE" /etc/matcomguard.conf
    log_success "Config: CPU=70%, RAM=50%, Whitelist incluye 'yes'"
}

start_monitor() {
    cd "$SCRIPT_DIR"
    nohup "$MONITOR_EXEC" > "$LOG_FILE" 2>&1 &
    MONITOR_PID=$!
    sleep 3
    if ps -p $MONITOR_PID > /dev/null 2>&1; then
        log_success "Monitor iniciado (PID: $MONITOR_PID)"
        return 0
    else
        log_error "Error al iniciar monitor"
        return 1
    fi
}

stop_monitor() {
    if [ ! -z "$MONITOR_PID" ] && ps -p $MONITOR_PID > /dev/null 2>&1; then
        kill $MONITOR_PID 2>/dev/null
        sleep 2
        ps -p $MONITOR_PID > /dev/null 2>&1 && kill -9 $MONITOR_PID 2>/dev/null
        log_success "Monitor detenido"
    fi
}

check_alerts() {
    local process="$1"
    local type="$2"
    local should_alert="$3"
    
    sleep 12  # Esperar alertas
    
    local alerts=$(grep -i "alerta" "$LOG_FILE" | grep -i "$process" | grep -i "$type" | wc -l)
    
    if [ "$should_alert" = "SI" ]; then
        if [ $alerts -gt 0 ]; then
            log_success "✓ Alerta detectada para '$process' ($type)"
            grep -i "alerta" "$LOG_FILE" | grep -i "$process" | grep -i "$type" | tail -1 >> "$INFORME_FILE"
            return 0
        else
            log_error "✗ Alerta esperada no detectada para '$process' ($type)"
            return 1
        fi
    else
        if [ $alerts -eq 0 ]; then
            log_success "✓ Sin alertas (correcto para lista blanca)"
            return 0
        else
            log_warning "⚠ Alerta inesperada para proceso en lista blanca"
            return 1
        fi
    fi
}

# ================================================================================
# FUNCIÓN PRINCIPAL
# ================================================================================

main() {
    echo "=================================================================================="
    echo "                    SCRIPT DE PRUEBAS RF2 - MATCOM GUARD"
    echo "=================================================================================="
    echo "Fecha: $(date)"
    echo "=================================================================================="
    
    # Inicializar informe
    cat > "$INFORME_FILE" << EOF
================================================================================
INFORME DE PRUEBAS RF2 - MATCOM GUARD  
================================================================================
Fecha: $(date)
Sistema: $(uname -a)
================================================================================

EOF

    # Verificaciones previas
    if [ ! -f "$MONITOR_EXEC" ]; then
        log_info "Compilando monitor..."
        cd "$SCRIPT_DIR"
        make clean && make process_monitor_gui
        [ ! -f "$MONITOR_EXEC" ] && { log_error "Error compilando"; exit 1; }
    fi
    
    setup_test_config
    rm -f /tmp/procstat_*.dat "$LOG_FILE"
    
    local tests_passed=0
    local tests_total=4
    
    # ============================================================================
    # CASO 1: PROCESO LEGÍTIMO (LISTA BLANCA)
    # ============================================================================
    log_test "CASO 1: PROCESO LEGÍTIMO (LISTA BLANCA)"
    log_info "Acción: Ejecutar 'yes' (alto CPU pero en lista blanca)"
    log_info "Esperado: Sin alertas"
    
    start_monitor || exit 1
    
    yes > /dev/null &
    YES_PID=$!
    log_info "Proceso 'yes' iniciado (PID: $YES_PID)"
    
    if check_alerts "yes" "CPU" "NO"; then
        tests_passed=$((tests_passed + 1))
    fi
    
    kill $YES_PID 2>/dev/null
    stop_monitor
    
    # ============================================================================
    # CASO 2: PROCESO MALICIOSO (ALTO CPU)
    # ============================================================================
    log_test "CASO 2: PROCESO MALICIOSO (ALTO CPU)"
    log_info "Acción: Ejecutar bucle infinito bash"
    log_info "Esperado: Alerta de alto CPU"
    
    start_monitor
    
    bash -c 'while true; do :; done' &
    MALICIOUS_PID=$!
    log_info "Proceso malicioso iniciado (PID: $MALICIOUS_PID)"
    
    if check_alerts "bash" "CPU" "SI"; then
        tests_passed=$((tests_passed + 1))
    fi
    
    kill $MALICIOUS_PID 2>/dev/null
    stop_monitor
    
    # ============================================================================
    # CASO 3: FUGA DE MEMORIA (ALTO RAM)
    # ============================================================================
    log_test "CASO 3: FUGA DE MEMORIA (ALTO RAM)"
    log_info "Acción: Consumir memoria progresivamente"
    log_info "Esperado: Alerta de alto RAM"
    
    start_monitor
    
    # Script consumidor de memoria mejorado
    cat > /tmp/memory_test.sh << 'EOF'
#!/bin/bash
# Consumir memoria hasta ~100MB para asegurar alertas
data=""
for i in {1..50}; do
    # Crear bloques de 2MB cada uno
    data="$data$(dd if=/dev/zero bs=2097152 count=1 2>/dev/null)"
    sleep 0.3
done
# Mantener la memoria ocupada
sleep 30
EOF
    chmod +x /tmp/memory_test.sh
    
    /tmp/memory_test.sh &
    MEMORY_PID=$!
    log_info "Consumidor memoria iniciado (PID: $MEMORY_PID)"
    
    if check_alerts "memory_test" "RAM" "SI"; then
        tests_passed=$((tests_passed + 1))
    fi
    
    kill $MEMORY_PID 2>/dev/null
    rm -f /tmp/memory_test.sh
    stop_monitor
    
    # ============================================================================
    # CASO 4: PROCESO FANTASMA (LIMPIEZA)
    # ============================================================================
    log_test "CASO 4: PROCESO FANTASMA (LIMPIEZA)"
    log_info "Acción: Terminar proceso y verificar limpieza"
    log_info "Esperado: Sistema detecta terminación"
    
    start_monitor
    
    sleep 300 &  # Proceso de larga duración
    SLEEP_PID=$!
    log_info "Proceso sleep iniciado (PID: $SLEEP_PID)"
    
    sleep 8  # Esperar detección
    
    local before_count=$(grep -c "PID: $SLEEP_PID" "$LOG_FILE" 2>/dev/null || echo 0)
    log_info "Proceso detectado $before_count veces antes de terminación"
    
    kill $SLEEP_PID
    log_info "Proceso terminado"
    
    sleep 10  # Esperar limpieza
    
    local cleanup=$(grep -c "TERMINADO.*$SLEEP_PID" "$LOG_FILE" 2>/dev/null || echo 0)
    
    if [ $cleanup -gt 0 ]; then
        log_success "✓ Limpieza detectada correctamente"
        tests_passed=$((tests_passed + 1))
    else
        log_error "✗ Limpieza no detectada"
    fi
    
    stop_monitor
    
    # ============================================================================
    # RESUMEN FINAL
    # ============================================================================
    log_test "RESUMEN DE PRUEBAS"
    
    echo "" >> "$INFORME_FILE"
    echo "ESTADÍSTICAS:" >> "$INFORME_FILE"
    echo "- Tests pasados: $tests_passed/$tests_total" >> "$INFORME_FILE"
    echo "- Alertas totales: $(grep -c "ALERTA" "$LOG_FILE" 2>/dev/null || echo 0)" >> "$INFORME_FILE"
    echo "- Procesos nuevos: $(grep -c "NUEVO PROCESO" "$LOG_FILE" 2>/dev/null || echo 0)" >> "$INFORME_FILE"
    echo "- Procesos terminados: $(grep -c "TERMINADO" "$LOG_FILE" 2>/dev/null || echo 0)" >> "$INFORME_FILE"
    
    if [ $tests_passed -eq $tests_total ]; then
        log_success "🎉 TODAS LAS PRUEBAS PASARON ($tests_passed/$tests_total)"
    else
        log_warning "⚠️ ALGUNAS PRUEBAS FALLARON ($tests_passed/$tests_total)"
    fi
    
    echo "" >> "$INFORME_FILE"
    echo "ARCHIVOS GENERADOS:" >> "$INFORME_FILE"
    echo "- Informe: $INFORME_FILE" >> "$INFORME_FILE"
    echo "- Log monitor: $LOG_FILE" >> "$INFORME_FILE"
    
    log_info "=================================================================================="
    log_info "Informe completo: $INFORME_FILE"
    log_info "Log del monitor: $LOG_FILE"
    log_info "=================================================================================="
    
    # Cleanup
    rm -f /tmp/procstat_*.dat
    sudo rm -f /etc/matcomguard.conf  # Limpiar archivo de config de sistema
    
    return $((tests_total - tests_passed))
}

# Ejecutar pruebas
main "$@"
exit $?
