#!/bin/bash

# Test Case 3: Puerto alto inesperado
# Abre puerto 4444 con servidor HTTP

echo "=== Test High Port HTTP Server ==="
echo "Iniciando servidor HTTP en puerto no estándar..."

HTTP_PORT=4444

echo "🌐 Iniciando servidor HTTP en puerto $HTTP_PORT"
echo "   Los servidores HTTP normalmente usan puertos 80 o 443"
echo "   El puerto $HTTP_PORT puede indicar actividad sospechosa"

echo ""
echo "🚀 Comando: python3 -m http.server $HTTP_PORT"

# Iniciar servidor HTTP en puerto no estándar
python3 -m http.server $HTTP_PORT --bind 127.0.0.1 > /tmp/http_server.log 2>&1 &
HTTP_PID=$!

sleep 2

if ps -p $HTTP_PID > /dev/null 2>&1; then
    echo "✓ Servidor HTTP iniciado (PID: $HTTP_PID)"
else
    echo "❌ Error iniciando servidor HTTP en puerto $HTTP_PORT"
    
    # Intentar puerto alternativo
    HTTP_PORT=8888
    echo "   Intentando puerto alternativo $HTTP_PORT..."
    python3 -m http.server $HTTP_PORT --bind 127.0.0.1 > /tmp/http_server.log 2>&1 &
    HTTP_PID=$!
    sleep 2
    
    if ps -p $HTTP_PID > /dev/null 2>&1; then
        echo "✓ Servidor HTTP iniciado en puerto $HTTP_PORT (PID: $HTTP_PID)"
    else
        echo "❌ No se pudo iniciar servidor HTTP"
        exit 1
    fi
fi

echo ""
echo "📡 Verificando puerto abierto..."
netstat -tln 2>/dev/null | grep ":$HTTP_PORT " || ss -tln 2>/dev/null | grep ":$HTTP_PORT "

echo ""
echo "🔍 Probando conexión HTTP..."
HTTP_RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout 5 http://localhost:$HTTP_PORT/ 2>/dev/null)
if [ "$HTTP_RESPONSE" = "200" ]; then
    echo "✓ Servidor HTTP respondiendo correctamente (código: $HTTP_RESPONSE)"
    
    # Mostrar algunos archivos servidos
    echo "📂 Contenido siendo servido:"
    curl -s --connect-timeout 3 http://localhost:$HTTP_PORT/ 2>/dev/null | head -5 | grep -o '<title>.*</title>' || echo "   Directorio actual del proyecto"
else
    echo "⚠️  Servidor no responde o respuesta inesperada (código: $HTTP_RESPONSE)"
fi

echo ""
echo "🌐 Iniciando servidores HTTP adicionales en puertos inusuales..."

# Puertos HTTP adicionales sospechosos
HTTP_PORTS=(3333 5555 7777 9999)
HTTP_PIDS=()

for port in "${HTTP_PORTS[@]}"; do
    python3 -m http.server $port --bind 127.0.0.1 > /tmp/http_server_$port.log 2>&1 &
    pid=$!
    sleep 1
    
    if ps -p $pid > /dev/null 2>&1; then
        echo "   ✓ Servidor HTTP en puerto $port (PID: $pid)"
        HTTP_PIDS+=($pid)
    else
        echo "   ❌ Error en puerto $port"
    fi
done

echo ""
echo "⏱️  Servidores activos por 15 segundos para detección..."
sleep 15

echo ""
echo "🔍 Verificando respuestas de todos los servidores..."
for port in $HTTP_PORT "${HTTP_PORTS[@]}"; do
    response=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout 2 http://localhost:$port/ 2>/dev/null)
    echo "   Puerto $port: HTTP $response"
done

echo ""
echo "🧹 Cerrando todos los servidores HTTP..."
kill $HTTP_PID 2>/dev/null
for pid in "${HTTP_PIDS[@]}"; do
    kill $pid 2>/dev/null
done

sleep 2

# Verificar cierre
echo "📋 Verificando cierre de servidores..."
for port in $HTTP_PORT "${HTTP_PORTS[@]}"; do
    if netstat -tln 2>/dev/null | grep ":$port " || ss -tln 2>/dev/null | grep ":$port "; then
        echo "⚠️  Puerto $port aún abierto"
    else
        echo "✓ Puerto $port cerrado"
    fi
done

# Limpiar logs temporales
rm -f /tmp/http_server*.log

echo ""
echo "RESULTADO ESPERADO en MatCom Guard:"
echo "- Alerta: 'Servidor HTTP en puerto no estándar $HTTP_PORT/tcp'"
echo "- Alertas similares para puertos 3333, 5555, 7777, 9999"
echo "- Los puertos deben aparecer como INUSUALES o SOSPECHOSOS"
echo "- Debe indicar que son servicios HTTP en puertos no estándar"
echo "- Posible advertencia sobre servidores web no autorizados"
echo ""
echo "Verifica las alertas en MatCom Guard y presiona Enter para continuar..."
read

echo "Test completado."
