#!/bin/bash

# Test Case 4: Timeout en puertos cerrados
# Escanea puertos que deberían estar cerrados

echo "=== Test Closed Port Scanning ==="
echo "Escaneando puertos cerrados para verificar detección..."

# Puertos que típicamente están cerrados
CLOSED_PORTS=(9999 9998 9997 8765 7654)

echo "🔍 Este test verifica que MatCom Guard detecte correctamente"
echo "   puertos cerrados durante el escaneo"

echo ""
echo "📡 Escaneando puertos que deberían estar cerrados..."

for port in "${CLOSED_PORTS[@]}"; do
    echo ""
    echo "🔍 Escaneando puerto $port..."
    
    # Verificar que el puerto esté realmente cerrado
    if netstat -tln 2>/dev/null | grep ":$port " || ss -tln 2>/dev/null | grep ":$port "; then
        echo "⚠️  Puerto $port está abierto (inesperado)"
        continue
    fi
    
    echo "   ✓ Puerto $port confirmado como cerrado"
    
    # Intentar conexión con timeout
    echo "   🔗 Probando conexión con timeout..."
    
    start_time=$(date +%s)
    timeout 5 nc -z localhost $port 2>/dev/null
    result=$?
    end_time=$(date +%s)
    duration=$((end_time - start_time))
    
    if [ $result -eq 0 ]; then
        echo "   ❌ Conexión exitosa (inesperado)"
    elif [ $result -eq 124 ]; then
        echo "   ⏱️  Timeout después de $duration segundos"
    else
        echo "   ✓ Conexión falló como esperado ($duration segundos)"
    fi
    
    # Scan adicional con nmap si está disponible
    if command -v nmap > /dev/null; then
        echo "   🛡️  Escaneando con nmap..."
        nmap_result=$(nmap -p $port --host-timeout 3s localhost 2>/dev/null | grep "$port")
        echo "   Resultado nmap: $nmap_result"
    fi
    
    sleep 1
done

echo ""
echo "🔍 Escaneando rango de puertos cerrados..."
echo "   Escaneando puertos 9990-9999 (debería estar todo cerrado)"

# Escanear un rango que probablemente esté cerrado
start_time=$(date +%s)
for port in {9990..9999}; do
    # Scan rápido sin output verbose
    timeout 1 nc -z localhost $port 2>/dev/null &
    
    # Limitar procesos concurrentes
    if (( $(jobs -r | wc -l) >= 5 )); then
        wait
    fi
done

# Esperar que terminen todos los scans
wait
end_time=$(date +%s)
scan_duration=$((end_time - start_time))

echo "✓ Escaneo de rango completado en $scan_duration segundos"

echo ""
echo "🔍 Test de puerto específico con múltiples intentos..."
TARGET_PORT=9999

echo "   Realizando 10 intentos de conexión a puerto $TARGET_PORT..."
for i in {1..10}; do
    start_time=$(date +%s)
    timeout 2 nc -z localhost $TARGET_PORT 2>/dev/null
    result=$?
    end_time=$(date +%s)
    duration=$((end_time - start_time))
    
    if [ $result -eq 124 ]; then
        status="TIMEOUT"
    elif [ $result -eq 0 ]; then
        status="CONECTADO"
    else
        status="CERRADO"
    fi
    
    echo "   Intento $i: $status (${duration}s)"
    sleep 0.5
done

echo ""
echo "🔍 Verificando puertos conocidos que deberían estar cerrados..."

# Lista de puertos comunes que típicamente están cerrados
COMMON_CLOSED=(21 23 25 53 110 143 993 995 1433 3389 5432)

echo "   Verificando puertos de servicios comunes..."
for port in "${COMMON_CLOSED[@]:0:5}"; do  # Solo los primeros 5 para no saturar
    timeout 2 nc -z localhost $port 2>/dev/null
    result=$?
    
    if [ $result -eq 0 ]; then
        echo "   Puerto $port: ABIERTO (puede ser normal)"
    else
        echo "   Puerto $port: CERRADO ✓"
    fi
done

echo ""
echo "RESULTADO ESPERADO en MatCom Guard:"
echo "- Reporte: 'Puerto 9999/tcp cerrado'"
echo "- Reportes similares para otros puertos escaneados"
echo "- Los puertos cerrados deben aparecer como 'CERRADO' o 'FILTRADO'"
echo "- NO debe haber alertas de seguridad para puertos cerrados normales"
echo "- El escaneo debe completarse sin errores de timeout"
echo "- Los intentos de conexión deben ser registrados"
echo ""
echo "Verifica en MatCom Guard que:"
echo "1. Los puertos cerrados aparecen correctamente"
echo "2. No hay alertas falsas para puertos cerrados"
echo "3. El escaneo se completa en tiempo razonable"
echo ""
echo "Presiona Enter para continuar..."
read

echo "Test completado."
