#!/bin/bash

# Test Case 4: Archivo eliminado
# Elimina un archivo legítimo del USB

echo "=== Test USB File Deletion ==="
echo "Eliminando archivo legítimo del USB..."

USB_MOUNT="/media/test_usb_device"

# Verificar que el directorio existe
if [ ! -d "$USB_MOUNT" ]; then
    echo "❌ Error: Directorio USB no encontrado. Ejecuta primero test_usb_insertion.sh"
    exit 1
fi

# Verificar que el archivo existe
if [ ! -f "$USB_MOUNT/documento.pdf" ]; then
    echo "❌ Error: documento.pdf no encontrado. Ejecuta primero test_usb_insertion.sh"
    exit 1
fi

echo "📄 Archivos antes de la eliminación:"
ls -la "$USB_MOUNT"

echo ""
echo "🗑️ Eliminando documento.pdf..."
sleep 2

# Eliminar archivo legítimo
rm "$USB_MOUNT/documento.pdf"

echo "✓ Archivo documento.pdf eliminado"
echo ""
echo "📄 Archivos después de la eliminación:"
ls -la "$USB_MOUNT"

echo ""
echo "🗑️ Eliminando archivo adicional..."
# Crear y luego eliminar otro archivo para más pruebas
echo "Archivo temporal importante" > "$USB_MOUNT/importante.doc"
sleep 1
rm "$USB_MOUNT/importante.doc"

echo "✓ Archivo importante.doc eliminado"

echo ""
echo "RESULTADO ESPERADO en MatCom Guard:"
echo "- Alerta: 'Archivo eliminado: $USB_MOUNT/documento.pdf'"
echo "- Alerta: 'Archivo eliminado: $USB_MOUNT/importante.doc'"
echo "- Los archivos deben desaparecer de la interfaz USB"
echo "- El recuento de archivos debe actualizarse"
echo ""
echo "Verifica las alertas en MatCom Guard y presiona Enter para continuar..."
read

echo "Test completado."
