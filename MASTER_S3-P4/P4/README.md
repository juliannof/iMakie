# iMakie — ESP32-P4 Master MCU

Master Mackie Control Universal (MCU) para Logic Pro. Controla 9 tracks S2 locales vía RS485 bus A, display IPS táctil 480×800, y matriz NeoTrellis 4×8.

**Placa de desarrollo:** GUITION ESP32-P4 Capacitive Touch IPS 4.3"  
**Chip:** ESP32-P4 (Xtensa dual-core 360MHz)  
**Flash:** 16MB (QIO)  
**PSRAM:** 8MB (OPI)  
**Display:** IPS 4.3" 480×800 (ST7701S MIPI-DSI 2-lane)  
**Touch:** Capacitivo GT911 (I2C)  
**Familia Mackie:** 0x14  
**Slaves controlados:** 9 (IDs 1–9) en RS485 bus A  
**NeoTrellis:** 2× Adafruit seesaw 4×4 (matriz 4×8)

---

## Especificación de placa (2026-05-16)

**Módulo:** GUITION JC4880P443C-I-W (placa de desarrollo integrada)  
**Procesador:** ESP32-P4 Xtensa dual-core 360MHz (Core0 + Core1)  
**Memoria:**
- Flash: 16MB (QIO mode)
- PSRAM: 8MB (OPI mode)
- Bootloader: 0x0 (256KB)
- App: 0x10000 (15.75MB)

**Display:** IPS capacitivo 4.3"
- Resolución: 480×800 píxeles
- Interface: MIPI-DSI 2-lane (ST7701S driver)
- Colores: 16M (24-bit RGB)
- Touch: Capacitivo multitouch GT911 (I2C)
- Brillo: Ajustable 0-255

**Energía:**
- Voltaje: USB 5V → regulador interno 3.3V
- Corriente: ~200mA idle, 400mA full power, picos 500mA
- USB: alimenta placa, display, touch y periféricos

**Conectividad:**
- RS485 bus A: 500 kbaud (9 slaves S2)
- I2C_NUM_0: NeoTrellis seesaw (GPIO 33/31)
- I2C_NUM_1: GT911 touch (GPIO 7/8)
- UART: RS485 (GPIO 50/51/52)

---

## Pinout definitivo P4

| Función | GPIO | Tipo | Notas |
|---------|------|------|-------|
| **RS485 TX (bus A)** | 50 | UART TX | 500 kbaud |
| **RS485 RX (bus A)** | 51 | UART RX | 500 kbaud |
| **RS485 EN (driver enable)** | 52 | GPIO output | Transceiver externo |
| **NeoTrellis SDA (I2C_NUM_0)** | 33 | I2C SDA | Dirección 0x2F/0x2E |
| **NeoTrellis SCL (I2C_NUM_0)** | 31 | I2C SCL | Dirección 0x2F/0x2E |
| **Touch SDA (GT911, I2C_NUM_1)** | 7 | I2C SDA | Capacitivo |
| **Touch SCL (GT911, I2C_NUM_1)** | 8 | I2C SCL | Capacitivo |
| **Display MIPI-DSI** | Integrado | MIPI 2-lane | ST7701S en placa |

---

## Compilación

### Build con PlatformIO

```bash
cd MASTER_S3-P4/P4
pio run -e esp32-p4
```

### Configuración PlatformIO

```ini
[env:esp32-p4]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.37/platform-espressif32.zip
board = esp32-p4
board_build.partitions = default_16MB.csv
board_build.flash_size = 16MB
board_build.arduino.memory_type = qio_opi
```

**Flags críticos:**
- `-DBOARD_HAS_PSRAM` — Habilita PSRAM (8MB)
- `-DARDUINO_USB_MODE=0` — USB nativo
- `-DDEVICE_P4_MASTER` — Identifica como P4 Master (vs S3 Extender)

### Platform y Framework

**Platform:** espressif32 (pioarduino 55.03.37 — IDF5 + Arduino core)  
**Framework:** Arduino  
**Librerías estándar:**
- LovyanGFX (display ST7701S MIPI-DSI)
- LVGL v9 (UI framework 480×800)
- Adafruit NeoPixel (seesaw 4×4 RGB)
- Wire (I2C GT911 touch, seesaw)
- HardwareSerial (RS485)

---

## Subsistemas P4

### Display P4 (ST7701S MIPI-DSI)
→ **[docs/DISPLAY_P4.md](../../docs/DISPLAY_P4.md)** (ST7701S 480×800, LVGL v9, portrait/landscape, rotación widgets)

### Touch (GT911)
→ **[docs/TOUCH.md](../../docs/TOUCH.md)** (GT911 capacitivo, I2C_NUM_1, calibración, gestos)

### NeoTrellis
→ **[docs/NEOTRELLLIS.md](../../docs/NEOTRELLLIS.md)** (2× Adafruit seesaw 4×4, matriz 4×8, direcciones 0x2F/0x2E, RGB LEDs)

### RS485 P4 (Bus A)
→ **[docs/RS485_P4.md](../../docs/RS485_P4.md)** (500 kbaud, 9 slaves, timing, diferencias vs bus B S3)

### Arquitectura Tareas P4
→ **[docs/ARCHITECTURE_P4.md](../../docs/ARCHITECTURE_P4.md)** (dual-core Core0/Core1, flags g_switchToPage, VU meter decay, race conditions)

---

## Referencias

- **RS485 (general):** [docs/RS485.md](../../docs/RS485.md)
- **Transport:** [docs/Transport.md](../../docs/Transport.md)
- **SAT:** [docs/SAT.md](../../docs/SAT.md)
- **Arquitectura general:** [CLAUDE.md](../../CLAUDE.md)
- **Estado técnico:** [STATUS.md](../../STATUS.md)
- **S2 Slave:** [S2/README.md](../../S2/README.md)
- **S3 Extender:** [MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/README.md](../S3/iMakie-ESP32_S3_EXTENDER/README.md)
