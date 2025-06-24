#!/bin/bash

# Test Case 3: Archivo nuevo sospechoso
# Crea un archivo con nombre sospechoso en el USB

echo "=== Test USB New Suspicious File ==="
echo "Creando archivo sospechoso en USB..."

USB_MOUNT="/media/test_usb_device"

# Verificar que el directorio existe
if [ ! -d "$USB_MOUNT" ]; then
    echo "❌ Error: Directorio USB no encontrado. Ejecuta primero test_usb_insertion.sh"
    exit 1
fi

echo "🦠 Creando archivo malware.exe..."
sleep 2

# Crear archivo con nombre sospechoso
cat > "$USB_MOUNT/malware.exe" << EOF
#!/bin/bash
# Archivo simulado de malware
echo "Este es un archivo sospechoso"
while true; do
    echo "Código malicioso simulado"
    sleep 1
done
EOF

chmod +x "$USB_MOUNT/malware.exe"

echo "✓ Archivo malware.exe creado"
echo "📄 Contenido del archivo:"
head -5 "$USB_MOUNT/malware.exe"

echo ""
echo "🦠 Creando otros archivos sospechosos..."
echo "Keylogger detectado" > "$USB_MOUNT/keylogger.dll"
echo "Ransomware payload" > "$USB_MOUNT/ransomware.bin"
echo "Trojan horse" > "$USB_MOUNT/trojan.bat"

echo "✓ Archivos sospechosos adicionales creados:"
ls -la "$USB_MOUNT"/*.exe "$USB_MOUNT"/*.dll "$USB_MOUNT"/*.bin "$USB_MOUNT"/*.bat 2>/dev/null

echo ""
echo "RESULTADO ESPERADO en MatCom Guard:"
echo "- Alerta: 'Archivo sospechoso añadido: $USB_MOUNT/malware.exe'"
echo "- Alerta: 'Archivo sospechoso añadido: $USB_MOUNT/keylogger.dll'"
echo "- Alerta: 'Archivo sospechoso añadido: $USB_MOUNT/ransomware.bin'"
echo "- Alerta: 'Archivo sospechoso añadido: $USB_MOUNT/trojan.bat'"
echo "- Los archivos deben aparecer marcados como sospechosos en la interfaz"
echo ""
echo "Verifica las alertas en MatCom Guard y presiona Enter para continuar..."
read

echo "Test completado."
