#!/bin/bash

# Test Case 2: Proceso malicioso (alto CPU)
# Ejecuta un script que consume CPU excesivamente

echo "=== Test Malicious CPU Process ==="
echo "Ejecutando proceso con consumo excesivo de CPU..."

echo "⚠️  ADVERTENCIA: Este test consumirá CPU al 100% por unos segundos"
echo "   Presiona Ctrl+C si el sistema se vuelve demasiado lento"

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

# Limpiar
rm -f /tmp/malicious_cpu.sh

echo ""
echo "🦠 Iniciando segundo test: múltiples procesos CPU-intensivos..."

# Crear múltiples procesos pequeños
for i in {1..3}; do
    bash -c 'while true; do echo $RANDOM > /dev/null; done' &
    PIDS[$i]=$!
    echo "   Proceso $i iniciado (PID: ${PIDS[$i]})"
done

sleep 5

echo "🛑 Terminando procesos múltiples..."
for pid in "${PIDS[@]}"; do
    kill -9 $pid 2>/dev/null
done

echo ""
echo "RESULTADO ESPERADO en MatCom Guard:"
echo "- Alerta: 'Proceso 'bash' usa 99% CPU (umbral: 70%)'"
echo "- Alerta similar para los procesos múltiples si superan el umbral"
echo "- Los procesos deben aparecer en rojo o marcados como sospechosos"
echo "- El sistema debe detectar el uso anómalo de CPU rápidamente"
echo ""
echo "Verifica las alertas en MatCom Guard y presiona Enter para continuar..."
read

echo "Test completado."
