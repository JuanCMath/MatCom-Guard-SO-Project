#!/bin/bash

# Test Case 1: Proceso legítimo
# Ejecuta un proceso con alto uso de CPU que debería estar en lista blanca

echo "=== Test Legitimate Process ==="
echo "Ejecutando proceso legítimo con alto uso de CPU..."

echo "🔍 Este test simula un proceso que consume recursos pero es legítimo"
echo "   (ejemplo: compilación, compresión, cálculos científicos)"

echo ""
echo "💻 Iniciando proceso de compresión legítimo..."
echo "   Comando: gzip -9 < /dev/zero > /tmp/test_compression.gz"

# Crear un proceso que use CPU de forma legítima
timeout 10 gzip -9 < /dev/zero > /tmp/test_compression.gz &
LEGITIM_PID=$!

echo "✓ Proceso legítimo iniciado (PID: $LEGITIM_PID)"
echo "📊 Monitoreando uso de CPU por 10 segundos..."

# Monitorear el proceso
for i in {1..5}; do
    if ps -p $LEGITIM_PID > /dev/null 2>&1; then
        CPU_USAGE=$(ps -p $LEGITIM_PID -o %cpu --no-headers 2>/dev/null | tr -d ' ')
        echo "   Segundo $((i*2)): CPU usage = ${CPU_USAGE}%"
        sleep 2
    else
        echo "   Proceso terminado"
        break
    fi
done

# Limpiar
if ps -p $LEGITIM_PID > /dev/null 2>&1; then
    kill $LEGITIM_PID 2>/dev/null
fi
rm -f /tmp/test_compression.gz

echo ""
echo "💻 Iniciando segundo proceso legítimo (find)..."
echo "   Comando: find /usr -name '*.so' 2>/dev/null"

# Otro proceso legítimo
timeout 8 find /usr -name "*.so" > /dev/null 2>&1 &
FIND_PID=$!

sleep 3
if ps -p $FIND_PID > /dev/null 2>&1; then
    CPU_USAGE=$(ps -p $FIND_PID -o %cpu --no-headers 2>/dev/null | tr -d ' ')
    echo "✓ Proceso find ejecutándose (PID: $FIND_PID, CPU: ${CPU_USAGE}%)"
    kill $FIND_PID 2>/dev/null
fi

echo ""
echo "RESULTADO ESPERADO en MatCom Guard:"
echo "- NO debe aparecer alerta si estos procesos están en lista blanca"
echo "- Si aparece alerta, agregar 'gzip' y 'find' a la lista blanca"
echo "- Los procesos deben aparecer en el monitor pero sin alertas"
echo "- Verificar que el umbral de CPU esté configurado correctamente (>70%)"
echo ""
echo "Verifica en MatCom Guard que NO hay alertas para estos procesos y presiona Enter..."
read

echo "Test completado."
