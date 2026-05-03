#!/bin/bash
# ============================================================
#  ota_upload.sh — iMakie PTxx Track S2
#  Uso: ./ota_upload.sh [numero_ip] [password]
#  Ejemplos: ./ota_upload.sh 62        → 192.168.1.62
#            ./ota_upload.sh 62 9821
# ============================================================

NUM="${1:-62}"
PASSWORD="${2:-9821}"
IP="192.168.1.$NUM"
ESPOTA="$HOME/.platformio/packages/framework-arduinoespressif32/tools/espota.py"

# Buscar el firmware más reciente
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FIRMWARE="$SCRIPT_DIR/.pio/build/lolin_s2_mini_ota/firmware.bin"

if [ ! -f "$FIRMWARE" ]; then
    echo "[OTA] Error: firmware no encontrado en $FIRMWARE"
    echo "[OTA] Compila primero con PlatformIO (Build, no Upload)"
    exit 1
fi

echo "[OTA] IP       : $IP"
echo "[OTA] Firmware : $FIRMWARE"
echo "[OTA] Tamaño   : $(du -h "$FIRMWARE" | cut -f1)"
echo ""
echo "[OTA] Activar OTA desde SAT → Config WiFi → Activar OTA"


python3 "$ESPOTA" \
    -i "$IP" \
    -p 3232 \
    --auth="$PASSWORD" \
    -f "$FIRMWARE" \
    -t 60 \
    -d

if [ $? -eq 0 ]; then
    echo "[OTA] Subida completada OK"
else
    echo "[OTA] Error en la subida"
    exit 1
fi
