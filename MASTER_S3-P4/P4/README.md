# iMakie — ESP32-P4 Master MCU

Master Mackie Control Universal (MCU) para Logic Pro. Controla 9 tracks S2 locales vía RS485 bus A, display LVGL 480×800, touch capacitivo GT911, y matriz NeoTrellis 4×8.

**Chip:** ESP32-P4 (GUITION JC4880P433C)  
**Familia Mackie:** 0x14  
**Slaves controlados:** 9 (IDs 1–9) en RS485 bus A  
**Display:** ST7701S MIPI-DSI 480×800  
**Touch:** GT911 capacitivo  
**NeoTrellis:** 2× Adafruit seesaw 4×4 (matriz 4×8)

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

```bash
cd MASTER_S3-P4/P4
pio run -e esp32-p4
```

**Platform:** espressif32  
**Framework:** Arduino  
**Librerías:** LovyanGFX, LVGL v9, WiFi, Ethernet (opcional)

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
