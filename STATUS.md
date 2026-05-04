# iMakie — Estado, Bugs y Pendientes

---

## S2 (ESP32-S2)

### **RS485**
**Estado:** funcional

#### Bugs
(ninguno)

#### Pendientes
- Verificar pines en P4 (TX=50, RX=51, EN=52) — confirmar compilación y funcionamiento

#### Detalles técnicos
- 500 kbaud, protocolo binario custom, CRC8
- Comunicación master-slave: MasterPacket (16B) → SlavePacket (9B)
- Timing crítico: sendResponse() < 150µs después de recibir
- Timeout 1000ms → LED11 rojo, suspende motor

---

### **NEOPIXEL**
**Estado:** funcional

#### Bugs
(ninguno)

#### Pendientes
(ninguno)

#### Detalles técnicos
- Cambio a Adafruit NeoPixel 3.1.7 (incompatible NeoPixelBus con IDF5)
- Secuencia de brillo (2026-04-28 16:15) — IMPLEMENTADO:
  - Azul tenue (NEOPIXEL_DIM_BRIGHTNESS=5) al inicio/reposo
  - Colores muy tenues (NEOPIXEL_ULTRA_DIM=1) cuando Logic conecta
  - O encendido (NEOPIXEL_DEFAULT_BRIGHTNESS=30) o tenue de morir
  - Optimización monocore: detección de cambios interna sin flags
- HW_STATUS display en boot: 10 componentes color-coded

---

### **BOTONES**
**Estado:** funcional

#### Bugs
(ninguno)

#### Pendientes
(ninguno)

#### Detalles técnicos
- 4 contactos digitales: REC (GPIO37), SOLO (GPIO38), MUTE (GPIO39), SELECT (GPIO40)
- Debounce 20ms, flanco ascendente (release) → flag `buttonPressed`
- Bits 0-3 en SlavePacket.buttons → MIDI notes al master
- Logic es fuente única de verdad (S2 nunca toggle local)

---

### **FADER**
**Estado:** funcional, validación pendiente

#### Bugs
(ninguno)

#### Pendientes
- Validar lecturas con hardware real tras reescritura
- ADS1015 pedido — reemplazar ADC nativa por I2C ADS1015 (resuelve ruido ±30 cuentas)

#### Detalles técnicos
- ESP32-S2 ADC1_CH9 (GPIO10), 13-bit (0-8191), atenuación 11dB
- EMA lowpass: alpha=0.20f
- Salida: SlavePacket.faderPos (13-bit) → master interpola 14-bit → Motor sigue

---

### **FADER TOUCH**
**Estado:** EN DESARROLLO

#### Bugs
(ninguno)

#### Pendientes
- Validar con plástico real (perfecto sin plástico actualmente)
- Ajustar thresholds si es necesario tras validación

#### Detalles técnicos
- Detección por sostenimiento: raw > baseline×1.015 sostenido > 6 frames (120ms) = TOQUE
- Baseline actualizado siempre con IIR (alpha=1/16, no congelado)
- TEST_TOUCH en SAT testea correctamente

---

### **MOTOR**
**Estado:** funcional post-calibración

#### Bugs
(ninguno)

#### Pendientes
(ninguno)

#### Detalles técnicos
- Driver DRV8833: IN1=GPIO14, IN2=GPIO16 (dirección), PWM=GPIO18 (analogWrite)
- Calibración no-bloqueante: KICK_UP → GOING_UP → SETTLE_UP → KICK_DOWN → GOING_DOWN → SETTLE_DOWN
- Control normal: dead zone (error < 50 cuentas = apagado)
- Si tocado → motor para inmediatamente

---

### **DISPLAY**
**Estado:** funcional

#### Bugs
(ninguno)

#### Pendientes
(ninguno)

#### Detalles técnicos
- ST7789V3 240×280, SPI3_HOST, freq_write=10MHz (unidireccional, sin MISO)
- Pulso RST manual (GPIO33 LOW 100ms) antes de tft.init()
- Layout: header 40px + main area + VU meter + VPot ring
- Sprites PSRAM: header, mainArea, vuSprite, vPotSprite

---

### **ENCODER**
**Estado:** funcional (sequenciamiento corregido 2026-04-28)

#### Bugs
(ninguno)

#### Pendientes
(ninguno)

#### Detalles técnicos
- Rotario Gray code: A=GPIO12, B=GPIO13, Push=GPIO21
- Debounce 3ms, derecha=+1, izquierda=-1
- Delta acumulado resetea post-buildResponse() (línea 242, post-VPot)
- Rango VPot: -7 a +7 (mapea Display 0-14)
- Resuelto (2026-04-28): reset() ahora post-VPot (antes causaba VPot ring no responda)

---

### **BUILD / PLATFORM**
**Estado:** estable

#### Bugs
(ninguno)

#### Pendientes
(ninguno)

#### Detalles técnicos
- Platform: pioarduino 55.03.37 / IDF5 (unificado con P4)
- Libs: LovyanGFX 1.2.19, Adafruit NeoPixel (NOT NeoPixelBus — incompatible IDF5)
- Orden init: Motor::init() → Serial.begin() → Display → NeoPixels → Hardware → RS485.startTask()
- Versioning: FW_VERSION + FW_BUILD_ID inyectados vía pre_build.py

---

### **SAT**
**Estado:** funcional

#### Bugs
(ninguno)

#### Pendientes
(ninguno)

#### Detalles técnicos
- Menú en Display (Encoder push > 3s): Motor On/Off, Motor Drive, Brightness, RS485 On/Off, LEDs Test, WiFi OTA, Reboot
- Suspende PSRAM y sprites → libera RAM para diagnósticos
- TEST_TOUCH, TEST_ENCODER funcionales

---

## S3 (ESP32-S3 Extender)

### **RS485**
**Estado:** funcional con timeouts ocasionales

#### Bugs
- **RS485 intermitente** — **FUNCIONANDO CON TIMEOUTS** — Sistema comunica: LEDs actualizan, datos OK. Timeouts ocasionales e impredecibles (~10-20 consecutivos, luego OK, repite). Comunicación física funciona a 500kbaud. NO modificar arquitectura sin probar compilación first.

#### Pendientes
- Identificar causa raíz de timeouts (posible: timing hardware, timeout 1500µs, ISR conflicts)
- No intentar buffer circular o cambios arquitectónicos sin validación en hardware

#### Detalles técnicos
- Bus B: 500 kbaud, protocolo binario custom, CRC8
- TX=GPIO15, RX=GPIO16, EN=GPIO1 (pinout definitivo)
- Controla 8 slaves S2 (IDs 1–8)
- Timing: TX_EN=30µs, TX_DONE=30µs, RESP_TIMEOUT=3000µs, GAP=300µs, POLL_CYCLE=20ms

---

### **TRANSPORTE**
**Estado:** funcional

#### Bugs
- **Note Off en botones de transporte** — **RESUELTO** — `onButtonReleased` envía `0x80 + note + 0x00`
- **Transport LEDs** — **RESUELTO** — notas 91–95 mapeadas a LEDs físicos en `setLedByNote()`

#### Pendientes
(ninguno)

#### Detalles técnicos
- Botones: REC (GPIO11), PLAY (GPIO9), FF (GPIO7), STOP (GPIO5), RW (GPIO3)
- LEDs: REC (GPIO12), PLAY (GPIO10), FF (GPIO8), STOP (GPIO6), RW (GPIO4)
- Notas MIDI Transporte: RW=0x5B (91), FF=0x5C (92), STOP=0x5D (93), PLAY=0x5E (94), REC=0x5F (95)
- LEDs controlados por `Transporte::setLedByNote()` con velocidad 127 (on) / 0 (off)

---

### **HANDSHAKE MCU**
**Estado:** funcional (protocolo correcto 2026-05-04)

#### Bugs
- **Handshake MCU incorrecto** — **RESUELTO** — código antiguo implementaba challenge/response propio. Ahora protocolo correcto per CLAUDE.md

#### Pendientes
(ninguno)

#### Detalles técnicos
- Familia Mackie: 0x14 (S3 Extender)
- Protocolo: sondeo inicial (cualquier familia) → handshake familia 0x14
- Respuestas: 0x00→0x01, 0x13→0x14
- Surface Type: 0x00 (Master) — hardcodeado, necesario para recibir transport LEDs
- Suscripción: 0x10 00 (feedback subscription)
- Ver CLAUDE.md "Mackie MCU — handshake correcto" para detalles completos

---

### **BUILD / PLATFORM**
**Estado:** estable

#### Bugs
(ninguno)

#### Pendientes
- Verificar pines RS485 (TX=15, RX=16, EN=1) — confirmar compilación y funcionamiento en hardware

#### Detalles técnicos
- Chip: ESP32-S3 (familia Mackie: 0x14)
- Slaves: 8 (IDs 1–8)
- Config.h: independiente del S2 (proyecto separado S3/)
- DEVICE_S3_EXTENDER flag en platformio.ini

---

## P4 (ESP32-P4 Master)

### Bugs P4
1. **`uiOfflineCreate()` doble llamada en `setup()`** — ~~primera antes de `prefs.begin()`, segunda después. Leak de LVGL garantizado.~~ **RESUELTO** — solo existe una llamada, después de `prefs.begin()`.
2. **Pantalla negra en boot** — Dos causas independientes: (a) ~~backlight off si `lastPage` era 1 o 2 (backlight solo se encendía vía `uiMenuInit()` → `uiPage3Create()`). Fix: `displaySetBrightness()` en `setup()` después de `initDisplay()`.~~ **RESUELTO**. (b) ~~`UIOffline` empieza con `s_blink_label` en HIDDEN y solo lo muestra cuando el logo termina de revelarse (~5-6 s); sin logo, Display negra permanente.~~ **RESUELTO** — label visible desde el inicio, parpadeo activo desde el primer tick independientemente del estado del logo.
3. **Handshake MCU incorrecto** — ~~código antiguo implementaba un challenge/response propio; P4 actuaba como HOST generando challenges aleatorios.~~ **RESUELTO** — ver sección "Mackie MCU — handshake" en CLAUDE.md.

### Pendientes P4
6. mutex o double-buffer en `vuLevels[]`
7. respuesta táctil lenta en vista de faders — investigar qué bloquea el hilo de touch, especialmente en UIPage faders
8. no muestra datos en Display tras conectarse — posible regresión en la transición a CONNECTED tras cambios en handshake
9. NeoTrellis sin implementar — atención a pines I2C nuevos (SDA=GPIO33, SCL=GPIO31)

---

## Cross-system

### Pendientes Cross-system
14. RS485: verificar pines en S3 (legacy) y P4 (TX=50, RX=51, EN=52) — confirmar que ambos compilan y funcionan correctamente
15. LEDs NeoPixel master: implementar control de brillo centralizado — estudiar si viable vía MIDI (CC o SysEx dedicado) para controlar brillo de todos los slaves desde Logic/P4

