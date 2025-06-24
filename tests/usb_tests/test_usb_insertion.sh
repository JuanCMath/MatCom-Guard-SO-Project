#!/bin/bash

# Test Case 1: Inserción USB
# Simula la inserción de un USB con archivos conocidos

echo "=== Test USB Insertion ==="
echo "Simulando inserción de dispositivo USB..."

# Crear directorio temporal para simular USB (debe estar en /media para que MatCom Guard lo detecte)
USB_MOUNT="/media/test_usb_device"
sudo mkdir -p "$USB_MOUNT"
sudo chown $(whoami):$(whoami) "$USB_MOUNT"

# Crear archivos base conocidos
echo "Documento legítimo" > "$USB_MOUNT/documento.pdf"
echo "Script de ejemplo" > "$USB_MOUNT/script.sh"
echo "Archivo de texto normal" > "$USB_MOUNT/archivo.txt"
chmod 644 "$USB_MOUNT/documento.pdf"
chmod 755 "$USB_MOUNT/script.sh"
chmod 644 "$USB_MOUNT/archivo.txt"

echo "✓ USB simulado montado en: $USB_MOUNT"
echo "✓ Archivos base creados:"
ls -la "$USB_MOUNT"
echo ""
echo "RESULTADO ESPERADO en MatCom Guard:"
echo "- El sistema debe registrar el dispositivo"
echo "- Debe establecer baseline (hashes de archivos)"
echo "- Debe mostrar los 3 archivos en la interfaz USB"
echo ""
echo "Presiona Enter para continuar o Ctrl+C para salir..."
read

echo "Test completado. El directorio $USB_MOUNT permanece para los siguientes tests."
