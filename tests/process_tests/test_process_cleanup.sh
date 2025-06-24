#!/bin/bash

# Test Case 4: Proceso fantasma
# Verifica la limpieza correcta cuando un proceso se termina

echo "=== Test Process Cleanup ==="
echo "Verificando limpieza de procesos terminados..."

echo "👻 Este test verifica que MatCom Guard detecte correctamente"
echo "   cuando los procesos terminan y los remueva del monitoreo"

echo ""
echo "📋 Procesos actuales monitoreados por el sistema:"
ps aux --sort=-%cpu | head -10

echo ""
echo "🚀 Iniciando proceso temporal para monitoreo..."

# Crear un proceso que será terminado
sleep 60 &
TEMP_PID=$!

echo "✓ Proceso iniciado (PID: $TEMP_PID)"
echo "   Comando: sleep 60"

echo ""
echo "⏱️  Esperando 5 segundos para que MatCom Guard lo detecte..."
sleep 5

# Verificar que el proceso existe
if ps -p $TEMP_PID > /dev/null 2>&1; then
    echo "✓ Proceso confirmado en sistema:"
    ps -p $TEMP_PID -o pid,ppid,cmd
else
    echo "❌ Proceso no encontrado"
    exit 1
fi

echo ""
echo "💀 Terminando proceso..."
kill $TEMP_PID

# Verificar terminación
sleep 2
if ps -p $TEMP_PID > /dev/null 2>&1; then
    echo "⚠️  Proceso aún existe, forzando terminación..."
    kill -9 $TEMP_PID
    sleep 1
fi

if ps -p $TEMP_PID > /dev/null 2>&1; then
    echo "❌ Error: Proceso no se pudo terminar"
else
    echo "✓ Proceso terminado correctamente"
fi

echo ""
echo "🚀 Iniciando múltiples procesos para test de limpieza masiva..."

# Crear varios procesos y terminarlos
PIDS=()
for i in {1..5}; do
    sleep 30 &
    PIDS+=($!)
    echo "   Proceso $i creado (PID: ${PIDS[$((i-1))]})"
done

echo ""
echo "⏱️  Esperando que MatCom Guard los detecte..."
sleep 3

echo "💀 Terminando todos los procesos..."
for pid in "${PIDS[@]}"; do
    if ps -p $pid > /dev/null 2>&1; then
        kill $pid
        echo "   Proceso $pid terminado"
    fi
done

# Verificar limpieza
sleep 2
echo ""
echo "🧹 Verificando limpieza..."
REMAINING=0
for pid in "${PIDS[@]}"; do
    if ps -p $pid > /dev/null 2>&1; then
        echo "⚠️  Proceso $pid aún existe"
        kill -9 $pid 2>/dev/null
        ((REMAINING++))
    fi
done

if [ $REMAINING -eq 0 ]; then
    echo "✓ Todos los procesos fueron terminados correctamente"
else
    echo "⚠️  $REMAINING procesos requirieron terminación forzada"
fi

echo ""
echo "🚀 Test final: proceso que se termina por sí mismo..."
# Proceso que se auto-termina
bash -c 'echo "Proceso auto-terminante"; sleep 3; echo "Terminando..."; exit 0' &
SELF_TERM_PID=$!

echo "✓ Proceso auto-terminante iniciado (PID: $SELF_TERM_PID)"
sleep 5

echo ""
echo "RESULTADO ESPERADO en MatCom Guard:"
echo "- Los procesos deben aparecer en el monitor cuando se crean"
echo "- Deben desaparecer del monitor cuando se terminan"
echo "- NO deben aparecer procesos 'zombies' o fantasma"
echo "- La lista de procesos debe actualizarse automáticamente"
echo "- El recuento de procesos activos debe ser correcto"
echo ""
echo "Verifica en MatCom Guard que:"
echo "1. Los procesos aparecieron cuando se crearon"
echo "2. Los procesos desaparecieron cuando se terminaron"
echo "3. No hay procesos fantasma en la lista"
echo ""
echo "Presiona Enter para continuar..."
read

echo "Test completado."
