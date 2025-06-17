#!/bin/bash

USB_SIMULADO=/media/sim_usb

# Asegurarse de que esté montado
if [ ! -d "$USB_SIMULADO" ]; then
  echo "❌ USB simulado no montado en $USB_SIMULADO"
  exit 1
fi

# Archivo normal
echo "Factura original" > "$USB_SIMULADO/invoice.txt"

# Simula crecimiento sospechoso
dd if=/dev/urandom of="$USB_SIMULADO/invoice.txt" bs=1M count=500

# Copia replicada con nombre raro
cp "$USB_SIMULADO/invoice.txt" "$USB_SIMULADO/invoice_copy_ABC123.pdf"

# Cambio de extensión
mv "$USB_SIMULADO/invoice.txt" "$USB_SIMULADO/invoice.exe"

# Permisos peligrosos
chmod 777 "$USB_SIMULADO/invoice.exe"

# Cambiar timestamp
touch -d "yesterday" "$USB_SIMULADO/invoice.exe"

echo "✅ Archivos maliciosos simulados en $USB_SIMULADO"
