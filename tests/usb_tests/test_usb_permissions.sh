#!/bin/bash

# Test Case 5: Atributos cambiados
# Cambia permisos de archivos en el USB

echo "=== Test USB File Permissions Change ==="
echo "Modificando permisos de archivos en USB..."

USB_MOUNT="/media/test_usb_device"

# Verificar que el directorio existe
if [ ! -d "$USB_MOUNT" ]; then
    echo "❌ Error: Directorio USB no encontrado. Ejecuta primero test_usb_insertion.sh"
    exit 1
fi

# Verificar que el archivo existe
if [ ! -f "$USB_MOUNT/script.sh" ]; then
    echo "❌ Error: script.sh no encontrado. Ejecuta primero test_usb_insertion.sh"
    exit 1
fi

echo "📋 Permisos originales:"
ls -la "$USB_MOUNT/script.sh"

echo ""
echo "🔧 Cambiando permisos a 777 (lectura/escritura/ejecución para todos)..."
sleep 2

# Cambiar permisos del archivo
chmod 777 "$USB_MOUNT/script.sh"

echo "✓ Permisos cambiados"
echo "📋 Nuevos permisos:"
ls -la "$USB_MOUNT/script.sh"

echo ""
echo "🔧 Creando archivo con permisos sospechosos..."
echo "Script con permisos peligrosos" > "$USB_MOUNT/dangerous_script.sh"
chmod 777 "$USB_MOUNT/dangerous_script.sh"

echo "✓ Archivo con permisos 777 creado:"
ls -la "$USB_MOUNT/dangerous_script.sh"

echo ""
echo "🔧 Cambiando permisos de otros archivos..."
# Cambiar permisos de archivos existentes si existen
for file in "$USB_MOUNT"/*.txt "$USB_MOUNT"/*.exe; do
    if [ -f "$file" ]; then
        echo "Cambiando permisos de $(basename "$file")..."
        chmod 777 "$file"
    fi
done

echo ""
echo "📋 Estado final de permisos:"
ls -la "$USB_MOUNT"

echo ""
echo "RESULTADO ESPERADO en MatCom Guard:"
echo "- Alerta: 'Permisos modificados en $USB_MOUNT/script.sh (ahora 777)'"
echo "- Alerta: 'Archivo con permisos peligrosos: $USB_MOUNT/dangerous_script.sh (777)'"
echo "- Alertas adicionales para otros archivos modificados"
echo "- Los archivos deben aparecer marcados con permisos sospechosos"
echo ""
echo "Verifica las alertas en MatCom Guard y presiona Enter para continuar..."
read

echo "Test completado."
