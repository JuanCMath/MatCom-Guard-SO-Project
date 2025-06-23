#!/bin/bash
# Script de ayuda para ejecutar pruebas manuales

echo "=== GUÍA DE PRUEBAS MANUALES MATCOM GUARD ==="
echo ""
echo "PASO 1: Compilar los programas de prueba"
echo "cd tests && make"
echo ""
echo "PASO 2: Compilar y ejecutar el monitor"
echo "make matcom-guard && ./matcom-guard"
echo ""
echo "PASO 3: En otra terminal, ejecutar los programas de prueba:"
echo ""
echo "CASO 1 - Proceso legítimo (lista blanca):"
echo "  cd tests && ./whitelisted_process 60"
echo "  -> Debe aparecer como 'stress' y NO generar alertas"
echo ""
echo "CASO 2 - Proceso malicioso (alto CPU):"
echo "  cd tests && ./high_cpu_process 45"
echo "  -> Debe generar alerta de alto CPU"
echo ""
echo "CASO 3 - Fuga de memoria:"
echo "  cd tests && ./memory_leak_process 80 2"
echo "  -> Debe generar alerta de alto RAM"
echo ""
echo "CASO 4 - Proceso fantasma:"
echo "  cd tests && ./normal_process 300 &"
echo "  # Esperar unos ciclos de monitoreo, luego:"
echo "  kill %1"
echo "  -> El monitor debe detectar la terminación"
echo ""
echo "NOTAS:"
echo "- Asegúrate de que la configuración tenga umbrales apropiados"
echo "- La lista blanca debe incluir 'stress' para el caso 1"
echo "- Observa los logs del monitor para ver las alertas"
echo ""

# Función para compilar automáticamente
if [ "$1" = "compile" ]; then
    echo "Compilando programas de prueba..."
    cd tests && make clean && make
    echo "Compilando monitor principal..."
    cd .. && make clean && make matcom-guard
    echo "¡Listo para las pruebas!"
fi
