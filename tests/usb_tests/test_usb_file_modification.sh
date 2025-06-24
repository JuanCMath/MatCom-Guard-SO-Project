#!/bin/bash

# Test Case 2: Archivo modificado
# Cambia el contenido de un archivo en el USB

echo "=== Test USB File Modification ==="
echo "Modificando contenido de archivo existente..."

USB_MOUNT="/media/test_usb_device"

# Verificar que el directorio existe
if [ ! -d "$USB_MOUNT" ]; then
    echo "❌ Error: Directorio USB no encontrado. Ejecuta primero test_usb_insertion.sh"
    exit 1
fi

# Verificar que el archivo existe
if [ ! -f "$USB_MOUNT/archivo.txt" ]; then
    echo "❌ Error: archivo.txt no encontrado. Ejecuta primero test_usb_insertion.sh"
    exit 1
fi

echo "📄 Contenido original:"
cat "$USB_MOUNT/archivo.txt"

echo ""
echo "🔧 Modificando archivo..."
sleep 2

# Modificar el contenido del archivo
echo "CONTENIDO MODIFICADO - POSIBLE MALWARE" > "$USB_MOUNT/archivo.txt"
echo "Línea sospechosa añadida" >> "$USB_MOUNT/archivo.txt"

echo "✓ Archivo modificado"
echo "📄 Nuevo contenido:"
cat "$USB_MOUNT/archivo.txt"

echo ""
echo "RESULTADO ESPERADO en MatCom Guard:"
echo "- Alerta: 'Cambio detectado en $USB_MOUNT/archivo.txt (hash modificado)'"
echo "- El hash del archivo debe haber cambiado"
echo "- Debe aparecer como modificado en la interfaz USB"
echo ""
echo "Verifica la alerta en MatCom Guard y presiona Enter para continuar..."
read

echo "Test completado."
