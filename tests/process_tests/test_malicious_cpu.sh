#!/bin/bash

# Test Case 2: Proceso malicioso (alto CPU)
# Ejecuta un script que consume CPU excesivamente
# MEJORA: Cleanup automático para evitar procesos residuales

echo "=== Test Malicious CPU Process ==="
echo "Ejecutando proceso con consumo excesivo de CPU..."

echo "⚠️  ADVERTENCIA: Este test consumirá CPU al 100% por unos segundos"
echo "   Presiona Ctrl+C si el sistema se vuelve demasiado lento"

# MODIFICACIÓN: Función de limpieza para evitar procesos huérfanos
cleanup_test() {
    echo "🧹 Limpiando procesos y archivos temporales..."
    if [ ! -z "$MALICIOUS_PID" ]; then
        kill -9 $MALICIOUS_PID 2>/dev/null
        wait $MALICIOUS_PID 2>/dev/null
    fi
    
    # Limpiar archivo temporal
    rm -f /tmp/malicious_cpu.sh
    
    # Matar cualquier proceso cpuUsage residual
    pkill -f "cpuUsage" 2>/dev/null
    pkill -f "malicious_cpu" 2>/dev/null
    
    echo "✅ Limpieza completada"
}

# MEJORA: Configurar trap para cleanup automático
trap cleanup_test EXIT INT TERM

echo ""
echo "🦠 Iniciando proceso malicioso con bucle infinito..."
echo "   Comando: while true; do :; done"

# Crear script malicioso temporal
cat > /tmp/malicious_cpu.sh << 'EOF'
#!/bin/bash
# Proceso que consume CPU al 100%
while true; do
    :  # comando nulo que se ejecuta infinitamente
done
EOF

chmod +x /tmp/malicious_cpu.sh

# Ejecutar el proceso malicioso
bash /tmp/malicious_cpu.sh &
MALICIOUS_PID=$!

echo "✓ Proceso malicioso iniciado (PID: $MALICIOUS_PID)"
echo "📊 Monitoreando uso de CPU..."

# Monitorear por 15 segundos
for i in {1..5}; do
    if ps -p $MALICIOUS_PID > /dev/null 2>&1; then
        CPU_USAGE=$(ps -p $MALICIOUS_PID -o %cpu --no-headers 2>/dev/null | tr -d ' ')
        MEM_USAGE=$(ps -p $MALICIOUS_PID -o %mem --no-headers 2>/dev/null | tr -d ' ')
        echo "   Segundo $((i*3)): CPU = ${CPU_USAGE}%, MEM = ${MEM_USAGE}%"
        sleep 3
    else
        echo "   Proceso terminado inesperadamente"
        break
    fi
done

echo ""
echo "🛑 Terminando proceso malicioso..."
kill -9 $MALICIOUS_PID 2>/dev/null
sleep 1

# Verificar que se terminó
if ps -p $MALICIOUS_PID > /dev/null 2>&1; then
    echo "❌ Proceso aún ejecutándose, intentando kill más agresivo..."
    sudo kill -9 $MALICIOUS_PID 2>/dev/null
else
    echo "✓ Proceso malicioso terminado correctamente"
fi

# MEJORA: Limpieza más exhaustiva
rm -f /tmp/malicious_cpu.sh

echo ""
echo "🦠 Iniciando segundo test: múltiples procesos CPU-intensivos..."
echo "   NOTA: Estos procesos también se limpiarán automáticamente"

# Crear múltiples procesos pequeños con cleanup automático
for i in {1..3}; do
    bash -c 'while true; do echo $RANDOM > /dev/null; done' &
    PIDS[$i]=$!
    # MEJORA: Registrar PIDs para cleanup
    echo "   Proceso $i iniciado (PID: ${PIDS[$i]})"
done

sleep 5

echo "🛑 Terminando procesos múltiples..."
for pid in "${PIDS[@]}"; do
    if [ ! -z "$pid" ]; then
        kill -9 $pid 2>/dev/null
        # MEJORA: Verificar terminación
        if ps -p $pid > /dev/null 2>&1; then
            echo "   ⚠️  Proceso $pid requiere terminación forzada"
            sudo kill -KILL $pid 2>/dev/null
        fi
    fi
done

# MEJORA: Actualizar función de limpieza para incluir estos PIDs
cleanup_test() {
    echo "🧹 Limpieza final de todos los procesos de prueba..."
    
    # Limpiar proceso principal si existe
    if [ ! -z "$MALICIOUS_PID" ]; then
        kill -9 $MALICIOUS_PID 2>/dev/null
    fi
    
    # Limpiar procesos múltiples
    for pid in "${PIDS[@]}"; do
        if [ ! -z "$pid" ]; then
            kill -9 $pid 2>/dev/null
        fi
    done
    
    # Limpiar archivos temporales
    rm -f /tmp/malicious_cpu.sh
    
    # Matar cualquier proceso residual por nombre
    pkill -f "cpuUsage" 2>/dev/null
    pkill -f "malicious_cpu" 2>/dev/null
    
    echo "✅ Limpieza completada - todos los procesos de prueba terminados"
}

echo ""
echo "RESULTADO ESPERADO en MatCom Guard:"
echo "- Alerta: 'Proceso 'bash' usa 99% CPU (umbral: 70%)'"
echo "- Alerta similar para los procesos múltiples si superan el umbral"
echo "- Los procesos deben aparecer en rojo o marcados como sospechosos"
echo "- El sistema debe detectar el uso anómalo de CPU rápidamente"
echo ""
echo "✅ Test completado. Los procesos se limpiarán automáticamente al salir."

echo "Test completado."
