#!/bin/bash

# Test Case 3: Fuga de memoria
# Ejecuta un programa que consume RAM excesivamente

echo "=== Test Memory Leak Process ==="
echo "Ejecutando proceso con consumo excesivo de RAM..."

echo "⚠️  ADVERTENCIA: Este test consumirá RAM progresivamente"
echo "   El test se limita a ~500MB para evitar problemas del sistema"

echo ""
echo "💾 Iniciando proceso con fuga de memoria simulada..."

# Crear script que consume memoria gradualmente
cat > /tmp/memory_leak.py << 'EOF'
#!/usr/bin/env python3
import time
import sys

print("Iniciando fuga de memoria simulada...")
memory_hog = []
chunk_size = 1024 * 1024  # 1MB chunks

try:
    for i in range(500):  # Máximo 500MB
        # Reservar memoria
        chunk = 'x' * chunk_size
        memory_hog.append(chunk)
        
        if i % 50 == 0:  # Cada 50MB
            print(f"Memoria consumida: ~{i}MB")
            
        time.sleep(0.1)  # Pausa pequeña para permitir monitoreo
        
except KeyboardInterrupt:
    print("Proceso interrumpido")
except MemoryError:
    print("Sin memoria disponible")
finally:
    print("Liberando memoria...")
    memory_hog.clear()
EOF

chmod +x /tmp/memory_leak.py

echo "🐍 Ejecutando script Python con fuga de memoria..."
python3 /tmp/memory_leak.py &
MEMORY_PID=$!

echo "✓ Proceso iniciado (PID: $MEMORY_PID)"
echo "📊 Monitoreando uso de memoria por 30 segundos..."

# Monitorear uso de memoria
for i in {1..10}; do
    if ps -p $MEMORY_PID > /dev/null 2>&1; then
        MEM_USAGE=$(ps -p $MEMORY_PID -o %mem --no-headers 2>/dev/null | tr -d ' ')
        RSS_KB=$(ps -p $MEMORY_PID -o rss --no-headers 2>/dev/null | tr -d ' ')
        RSS_MB=$((RSS_KB / 1024))
        echo "   Segundo $((i*3)): MEM = ${MEM_USAGE}% (~${RSS_MB}MB)"
        sleep 3
    else
        echo "   Proceso terminado"
        break
    fi
done

echo ""
echo "🛑 Terminando proceso de fuga de memoria..."
kill -TERM $MEMORY_PID 2>/dev/null
sleep 2

if ps -p $MEMORY_PID > /dev/null 2>&1; then
    echo "   Proceso aún ejecutándose, forzando terminación..."
    kill -9 $MEMORY_PID 2>/dev/null
fi

# Limpiar
rm -f /tmp/memory_leak.py

echo ""
echo "💾 Iniciando segundo test: simulación con 'tail /dev/zero'..."
echo "   (limitado por timeout para seguridad)"

# Test alternativo más simple pero efectivo
timeout 10 tail /dev/zero 2>/dev/null &
TAIL_PID=$!

echo "✓ Proceso tail iniciado (PID: $TAIL_PID)"
sleep 5

if ps -p $TAIL_PID > /dev/null 2>&1; then
    MEM_USAGE=$(ps -p $TAIL_PID -o %mem --no-headers 2>/dev/null | tr -d ' ')
    echo "   Uso de memoria del proceso tail: ${MEM_USAGE}%"
    kill $TAIL_PID 2>/dev/null
fi

echo ""
echo "RESULTADO ESPERADO en MatCom Guard:"
echo "- Alerta: 'Proceso 'python3' usa X% RAM (umbral: 50%)'"
echo "- Alerta: 'Proceso 'tail' usa Y% RAM (umbral: 50%)'"
echo "- Los procesos deben aparecer marcados como consumidores de memoria"
echo "- El uso de memoria debe incrementarse visiblemente en el monitor"
echo ""
echo "Verifica las alertas en MatCom Guard y presiona Enter para continuar..."
read

echo "Test completado."
