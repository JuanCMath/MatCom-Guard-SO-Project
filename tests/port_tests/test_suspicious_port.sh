#!/bin/bash

# Test Case 2: Puerto sospechoso
# Abre puerto 31337 (comúnmente usado por malware)

echo "=== Test Suspicious Port 31337 ==="
echo "Abriendo puerto sospechoso 31337..."

SUSPICIOUS_PORT=31337

echo "🦠 El puerto 31337 es comúnmente usado por:"
echo "   - Back Orifice (malware clásico)"
echo "   - Herramientas de hacking"
echo "   - Backdoors y rootkits"

echo ""
echo "🚀 Iniciando servidor en puerto $SUSPICIOUS_PORT..."
echo "   Comando: nc -l $SUSPICIOUS_PORT"

# Iniciar netcat en puerto sospechoso
echo "Servidor sospechoso activo" | nc -l $SUSPICIOUS_PORT &
NC_PID=$!

sleep 1

if ps -p $NC_PID > /dev/null 2>&1; then
    echo "✓ Servidor iniciado en puerto $SUSPICIOUS_PORT (PID: $NC_PID)"
else
    echo "❌ Error iniciando servidor en puerto $SUSPICIOUS_PORT"
    
    # Intentar puerto alternativo si falla
    SUSPICIOUS_PORT=31338
    echo "   Intentando puerto alternativo $SUSPICIOUS_PORT..."
    echo "Servidor sospechoso activo" | nc -l $SUSPICIOUS_PORT &
    NC_PID=$!
    sleep 1
    
    if ps -p $NC_PID > /dev/null 2>&1; then
        echo "✓ Servidor iniciado en puerto $SUSPICIOUS_PORT (PID: $NC_PID)"
    else
        echo "❌ No se pudo iniciar servidor sospechoso"
        exit 1
    fi
fi

echo ""
echo "📡 Verificando puerto abierto..."
netstat -tln 2>/dev/null | grep ":$SUSPICIOUS_PORT " || ss -tln 2>/dev/null | grep ":$SUSPICIOUS_PORT "

echo ""
echo "🔍 Probando conexión al puerto sospechoso..."
timeout 3 nc -z localhost $SUSPICIOUS_PORT && echo "✓ Puerto $SUSPICIOUS_PORT accesible" || echo "⚠️  Puerto $SUSPICIOUS_PORT no accesible desde localhost"

echo ""
echo "⏱️  Manteniendo puerto abierto por 15 segundos para detección..."
sleep 15

echo ""
echo "🦠 Iniciando servidores adicionales en puertos sospechosos..."

# Puertos adicionales sospechosos
EXTRA_PORTS=(1337 31338 12345 54321)
EXTRA_PIDS=()

for port in "${EXTRA_PORTS[@]}"; do
    echo "Servidor en puerto $port" | nc -l $port &
    pid=$!
    if ps -p $pid > /dev/null 2>&1; then
        echo "   ✓ Puerto $port abierto (PID: $pid)"
        EXTRA_PIDS+=($pid)
    else
        echo "   ❌ Error en puerto $port"
    fi
    sleep 1
done

echo ""
echo "⏱️  Esperando 10 segundos adicionales para detección múltiple..."
sleep 10

echo ""
echo "🧹 Cerrando todos los servidores sospechosos..."
kill $NC_PID 2>/dev/null
for pid in "${EXTRA_PIDS[@]}"; do
    kill $pid 2>/dev/null
done

sleep 2

# Verificar que se cerraron
echo "📋 Verificando cierre de puertos..."
for port in $SUSPICIOUS_PORT "${EXTRA_PORTS[@]}"; do
    if netstat -tln 2>/dev/null | grep ":$port " || ss -tln 2>/dev/null | grep ":$port "; then
        echo "⚠️  Puerto $port aún abierto"
    else
        echo "✓ Puerto $port cerrado"
    fi
done

echo ""
echo "RESULTADO ESPERADO en MatCom Guard:"
echo "- Alerta: 'Puerto $SUSPICIOUS_PORT/tcp abierto (no estándar)'"
echo "- Alertas adicionales para puertos 1337, 31338, 12345, 54321"
echo "- Los puertos deben aparecer marcados como SOSPECHOSOS"
echo "- Debe haber indicadores visuales de advertencia (color rojo, iconos)"
echo "- Posible alerta de 'Puerto asociado con malware conocido'"
echo ""
echo "Verifica las alertas en MatCom Guard y presiona Enter para continuar..."
read

echo "Test completado."
