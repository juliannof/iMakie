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

## Inicialización (Orden Crítico)

**Orden de init en `setup()`:**

```cpp
setup() {
  1. Motor::init()           ← ANTES de Serial.begin() (silencia motor)
  2. Serial.begin()
  3. initNeopixels()
  4. initDisplay()
  5. faderADC.begin()
  6. initHardware()          ← botones + otros GPIO
  7. FaderTouch::init()
  8. Encoder::begin()
  9. ButtonManager::begin()
  10. Motor::begin()         ← habilita control
  11. rs485.begin(trackId)   ← ÚLTIMO (ya todo funcional)
}
```

**¿Por qué importa el orden?**
- **Motor::init()** debe ejecutarse ANTES de `Serial.begin()` para silenciar motor sin debug output
- **RS485** debe iniciar cuando todo está listo (si falla, no bloquea boot)
- **Display + NeoPixels** comparten SPI, timing crítico
- **Encoder** debe inicializarse DESPUÉS de hardware GPIO pero ANTES de RS485 (ISR attachInterrupt)
- **FaderADC** depende de I2C (Wire), debe estar antes que RS485 poll

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

## Build S2

**Platform:** pioarduino 55.03.37 / IDF5 — unificado con P4 (requiere P4)

**Librerías clave:**
- **LovyanGFX 1.2.19** — Display driver ST7789V3 (cambio desde NeoPixelBus)
- **Adafruit NeoPixel** — Control LEDs WS2812B (compatible IDF5)
- **ADS1115 library** — ADC I2C 16-bit
- **Otras:** Wire (I2C moderno), FreeRTOS, ESP-IDF5

**Orden de init obligatorio en código:**
```cpp
// main.cpp setup()
initDisplay()      // LovyanGFX init ANTES
initNeopixels()    // Adafruit NeoPixel DESPUÉS (SPI timing)
initHardware()     // GPIO, botones, etc.
```

**Logging:**
- `Serial.printf()` para S2 (recomendado)
- `log_i()`/`log_w()` no son fiables en S2 (IDF5 limitation)

**Versioning:** `FW_VERSION` y `FW_BUILD_ID` inyectados vía `pre_build.py` en build time

---

## Compilación

```bash
cd S2/S2_V1
pio run -e lolin_s2_mini
```

---

## Referencias

- **Fader (ADS1115):** [docs/FADER.md](../docs/FADER.md)
- **Motor (DRV8833):** [docs/MOTOR.md](../docs/MOTOR.md)
- **RS485 (protocolo):** [docs/RS485.md](../docs/RS485.md)
- **Botones:** [docs/BUTTONS.md](../docs/BUTTONS.md)
- **Display (ST7789V3):** [docs/DISPLAY.md](../docs/DISPLAY.md)
- **Encoder (Gray code):** [docs/ENCODER.md](../docs/ENCODER.md)
- **LEDs (NeoPixel):** [docs/LEDS.md](../docs/LEDS.md)
- **SAT (Auto-Test):** [docs/SAT.md](../docs/SAT.md)
- **WiFi/OTA:** [docs/WIFI.md](../docs/WIFI.md)
- **Arquitectura general:** [CLAUDE.md](../CLAUDE.md)
- **Estado técnico:** [STATUS.md](../STATUS.md)
- **Historial cambios:** [CHANGELOG.md](../CHANGELOG.md)
