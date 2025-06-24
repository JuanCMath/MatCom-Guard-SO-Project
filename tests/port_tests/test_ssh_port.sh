#!/bin/bash

# Test Case 1: Puerto legítimo (SSH)
# Abre el puerto SSH y verifica detección

echo "=== Test Legitimate SSH Port ==="
echo "Probando detección de puerto SSH legítimo..."

SSH_PORT=22

echo "🔍 Verificando estado actual del servicio SSH..."
SSH_STATUS=$(systemctl is-active ssh 2>/dev/null || systemctl is-active sshd 2>/dev/null || echo "inactive")
echo "   Estado SSH: $SSH_STATUS"

echo ""
if [ "$SSH_STATUS" = "active" ]; then
    echo "✓ SSH ya está ejecutándose en puerto $SSH_PORT"
    echo "📡 Verificando puerto SSH..."
    netstat -tln | grep ":$SSH_PORT " || ss -tln | grep ":$SSH_PORT "
else
    echo "🚀 Intentando iniciar servicio SSH..."
    echo "   Nota: Requiere permisos de administrador"
    
    # Intentar iniciar SSH
    if command -v systemctl > /dev/null; then
        echo "   Usando systemctl para iniciar SSH..."
        sudo systemctl start ssh 2>/dev/null || sudo systemctl start sshd 2>/dev/null
        sleep 2
        SSH_STATUS=$(systemctl is-active ssh 2>/dev/null || systemctl is-active sshd 2>/dev/null || echo "failed")
    fi
    
    if [ "$SSH_STATUS" = "active" ]; then
        echo "✓ SSH iniciado correctamente"
    else
        echo "⚠️  No se pudo iniciar SSH automáticamente"
        echo "   Iniciando servidor SSH simulado en puerto 2222..."
        
        # SSH simulado en puerto alternativo
        SSH_PORT=2222
        echo "Servidor SSH simulado" | nc -l $SSH_PORT &
        NC_PID=$!
        sleep 1
        
        if ps -p $NC_PID > /dev/null 2>&1; then
            echo "✓ Servidor SSH simulado iniciado en puerto $SSH_PORT"
        else
            echo "❌ Error iniciando servidor simulado"
            exit 1
        fi
    fi
fi

echo ""
echo "📡 Verificando puertos SSH abiertos..."
netstat -tln 2>/dev/null | grep ":$SSH_PORT " || ss -tln 2>/dev/null | grep ":$SSH_PORT " || echo "Puerto no visible en netstat/ss"

echo ""
echo "🔍 Escaneando puerto SSH desde localhost..."
timeout 5 nc -z localhost $SSH_PORT && echo "✓ Puerto $SSH_PORT accesible" || echo "❌ Puerto $SSH_PORT no accesible"

echo ""
echo "⏱️  Esperando 10 segundos para que MatCom Guard detecte el puerto..."
sleep 10

echo ""
echo "🧹 Limpieza..."
if [ -n "$NC_PID" ] && ps -p $NC_PID > /dev/null 2>&1; then
    kill $NC_PID 2>/dev/null
    echo "✓ Servidor simulado terminado"
fi

echo ""
echo "RESULTADO ESPERADO en MatCom Guard:"
echo "- Reporte: 'Puerto SSH detectado en $SSH_PORT/tcp'"
echo "- El puerto debe aparecer como legítimo/conocido"
echo "- NO debe aparecer como sospechoso"
echo "- Debe estar categorizado como servicio estándar"
echo ""
echo "Verifica en MatCom Guard que el puerto SSH aparece correctamente y presiona Enter..."
read

echo "Test completado."
