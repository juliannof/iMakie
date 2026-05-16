# AITEC-17 PTxx — S2 Slave Fader
A Mackie Control fader with ESP32-S2

<img alt="ESP32 S2" src="https://www.wemos.cc/en/latest/_static/boards/s2_mini_v1.0.0_4_16x9.jpg">

---

## Hardware

**MCU:** ESP32-S2FN4R2  
**Flash:** 4MB  
**PSRAM:** 2MB (QSPI)  
**Placa:** Lolin S2 Mini

---

## Pinout (ESP32-S2FN4R2)

| Función | GPIO |
|---------|------|
| **RS485 TX** | 8 |
| **RS485 RX** | 9 |
| **RS485 EN** | 35 |
| **ADS1115 — Canal entrada (A0)** | 10 |
| **ADS1115 — SDA (I2C)** | 21 |
| **ADS1115 — SCL (I2C)** | 34 |
| **ADS1115 — Alert/RDY (ISR)** | 17 |
| **Encoder A** | 13 |
| **Encoder B** | 12 |
| **Encoder BTN** | 11 |
| **Display SCLK (SPI3)** | 7 |
| **Display MOSI (SPI3)** | 4 |
| **Display DC** | 6 |
| **Display CS** | 5 |
| **Display RST** | 33 |
| **Display BL (PWM 500Hz)** | 3 |
| **NeoPixels** | 36 |
| **Botón REC** | 37 |
| **Botón SOLO** | 38 |
| **Botón MUTE** | 39 |
| **Botón SELECT** | 40 |

### Motor (DRV8833 H-bridge)
- **EN (nSLEEP)** = GPIO14
- **IN1** = GPIO18
- **IN2** = GPIO16

### ADS1115 I2C ADC
- **Dirección I2C:** 0x48
- **Gain:** ±4.096V
- **Sample Rate:** 860 SPS
- **Modo:** Continuous (ISR-driven en GPIO17)
- **Resolución:** 16-bit (mejora 6-15× vs ADC nativo)

---

## Display

- **Panel:** ST7789V3 240×280
- **Interface:** SPI3_HOST
- **Frecuencia write:** 10MHz (unidireccional, sin MISO)
- **Config:** `memory_height=320`, `offset_y=20`, `invert=true`, `rgb_order=false`
- **Backlight:** PWM 500Hz, GPIO3
- **Alimentación:** Rail 5V (PCB V2)

---

## Comunicación

**RS485:**
- Baudrate: 500 kbaud
- Protocolo: Binario custom con CRC8
- Ciclo: ~20ms (master S3 controla)
- Timeout: 1000ms sin datos → LED11 rojo, motor suspendido

**I2C (ADS1115):**
- SDA=GPIO21, SCL=GPIO34
- Dirección: 0x48
- ISR Alert en GPIO17 (FALLING, 860 SPS)

---

## Control de Fader

**Arquitectura end-to-end:**
```
Logic Pro (PitchBend signed 14-bit)
    ↓
S3 MidiProcessor (mapea a ADC 0-27000)
    ↓
RS485 (20ms ciclo)
    ↓
S2 Motor::setTarget() (dead zone 50 cuentas)
    ↓
DRV8833 posiciona fader
    ↓
FaderADC (ADS1115) realimenta posición
    ↓
S2 responde SlavePacket.faderPos
    ↓
S3 mapea de vuelta a PitchBend
    ↓
Logic recibe feedback
```

**Calibración (no-bloqueante, 3-5s):**
1. S3 ordena FLAG_CALIB vía RS485
2. Motor ejecuta: KICK_UP → GOING_UP → SETTLE_UP → KICK_DOWN → GOING_DOWN → SETTLE_DOWN
3. S2 captura min/max ADC y envía 2 paquetes de calibración
4. Guard cooldown: no reinicia si completó hace <2000ms

---

## Compilación

```bash
cd S2/S2_V1
pio run -e lolin_s2_mini
```

Platform: pioarduino 55.03.37 / IDF5

---

## Referencias

- **Documentación exhaustiva:** Ver `FADER.md` en raíz del repo
- **Arquitectura general:** Ver `CLAUDE.md`
- **Estado técnico:** Ver `STATUS.md`
- **Historial cambios:** Ver `CHANGELOG.md`
