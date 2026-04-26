# iMakie — Contexto para Claude Code

## Qué es esto
Controlador DAW compatible Mackie Control Universal (MCU) para Logic Pro, construido sobre ESP32. Tres subproyectos en PlatformIO, todos en este repo.

---

## Arquitectura general

```
Logic Pro (macOS)
    │ USB-MIDI
    ▼
ESP32-P4  ←→  RS485 bus A  ←→  17× ESP32-S2 (PTxx Track)
(MCU, 9 tracks, IDs 1–9)

ESP32-S3  ←→  RS485 bus B  ←→  8× ESP32-S2 (PTxx Track)
(Extender, IDs 1–8)
```

- **P4** es el master MCU (reemplaza S3 #1 original). **S3 Extender** controla 8 slaves adicionales (hardware actual).
- **S2 slaves** son el hardware definitivo — 17 unidades Lolin S2 Mini.
- RP2040 descartado permanentemente. S2 es la plataforma slave definitiva.

---

## Subproyectos PlatformIO

| Directorio | MCU | Rol |
|---|---|---|
| `S3/` | ESP32-P4 (master) / ESP32-S3 (extender) | Master MCU + Extender — USB-MIDI + RS485 |
| `track S2/` | ESP32-S2 (Lolin S2 Mini) | Slave — 1 canal físico completo |

---

## Hardware PTxx Track S2 (slave)

**Chip:** ESP32-S2FN4R2 — 4MB Flash, 2MB PSRAM (QSPI)

### Pinout definitivo

| Función | GPIO |
|---|---|
| RS485 TX | 8 |
| RS485 RX | 9 |
| RS485 EN | 35 |
| Fader ADC | 10 |
| Encoder A | 12 |
| Encoder B | 13 |
| Encoder BTN | 21 |
| Motor IN1 | 14 |
| Motor IN2 | 16 |
| Motor PWM | 18 |
| Display SCLK | 7 |
| Display MOSI | 4 |
| Display DC | 6 |
| Display CS | 5 |
| Display RST | 33 |
| Display BL | 3 |
| NeoPixels | 36 |
| Botón REC | 37 |
| Botón SOLO | 38 |
| Botón MUTE | 39 |
| Botón SELECT | 40 |
| Touch fader | GPIO1 / T1 |

### Display
- Panel ST7789V3 240×280, SPI3_HOST
- `freq_write=5MHz`, `freq_read=8MHz`
- `memory_height=320`, `offset_y=20`, `invert=true`, `rgb_order=false`
- BL PWM 500Hz
- **Obligatorio:** pulso RST manual (GPIO33 LOW 100ms) antes de `tft.init()`
- Alimentado en rail 5V (no 3.3V) — PCB V2

### PSRAM
- `board_build.arduino.memory_type = qio_qspi`
- Todos los sprites LovyanGFX: `setPsram(true)` + `setColorDepth(16)` antes de `createSprite()`
- Verificación: `esp_ptr_external_ram()`, direcciones `0x3f8xxxxx`

### NeoPixels
- Librería: `adafruit/Adafruit NeoPixel` (cambio desde NeoPixelBus 2.8.4)
- **Una sola instancia global** — `Adafruit_NeoPixel neopixels(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800)`
- Acceso exclusivamente a través de `Neopixel.cpp`
- **Razón del cambio:** NeoPixelBus con pioarduino 55.03.37 falla por campo `tx_pcm_bypass` no disponible en IDF5; Adafruit NeoPixel es más simple y mantenido por Adafruit

### ADC fader
- API IDF5 `adc_oneshot` directamente (`ADC_ATTEN_DB_11`, `ADC_BITWIDTH_13`)
- `analogSetPinAttenuation` es UNRELIABLE con IDF5 en S2 — no usar

### Motor
- Driver DRV8833 H-bridge
- Control con `analogWrite` — `ledcAttach`/`ledcWrite` rompe el movimiento en esta plataforma
- `Motor::init()` debe ejecutarse antes de `Serial.begin()` (silencia motor en boot)
- Calibración no-bloqueante: `KICK_UP → GOING_UP → SETTLE_UP → KICK_DOWN → GOING_DOWN → SETTLE_DOWN`
- No PID. Dead zone approach. `FADER_EMA_ALPHA = 0.20f`

### RS485
- 500 kbaud, protocolo binario custom, CRC8
- `Serial1.setRxBufferSize()` **antes** de `Serial1.begin()`
- `sendResponse()` inmediatamente después de recibir paquete — antes de display/neopixel/motor
- `vTaskDelay(1)` en estados WAIT_RESP y GAP para no matar Core 1

### WiFi / OTA — PCB V2
- GPIO8 (RS485 TX) recibe 4.6V backfeed del bus a través del transceiver → bloquea radio WiFi
- **Fix software en `OtaManager::enableForUpload()`**: deshabilitar transceiver (EN=HIGH, GPIO8 LOW, Serial1.end()) antes de `WiFi.begin()`; restaurar si falla conexión
- El rework de 100Ω en GPIO8 está **permanentemente descartado** — fix es solo software
- OTA password: `9821` | WiFi SSID: `Julianno-WiFi`
- Provisioning via sketch USB que cachea credenciales en NVS namespace `"ptxx"`

### NVS namespace S2
`"ptxx"` — claves: `wifiSsid`, `wifiPass`, `otaPass`, `trackId`, `label`, `pwmMin`, `pwmMax`, `touchEn`, `touchThr`, `motorDis`

### Build S2
- Platform: pioarduino 53.03.11 / IDF5 — **no actualizar**, USB-MIDI del S3 funciona en esta versión
- LovyanGFX 1.2.19
- Orden de init obligatorio: `initDisplay()` → `initNeopixels()` → `initHardware()`
- FreeRTOS: `rs485.begin()` en setup, `rs485.startTask()` al final del setup
- `Serial.printf` para logging — `log_i`/`log_w` no son fiables en S2
- Versioning: `FW_VERSION` y `FW_BUILD_ID` inyectados via `pre_build.py`

---

## Hardware P4 (master)

**Placa:** GUITION JC4880P433C (ESP32-P4)

### Display
- ST7701S MIPI-DSI 2-lane, 480×800 portrait
- LVGL v9
- **Solo inicializa en portrait** — landscape por rotación software
- Rotación de widgets: `lv_obj_set_style_transform_rotation(obj, 900, 0)`, coordenadas X/Y intercambiadas mentalmente

### Touch
- GT911 en I2C_NUM_1 (SDA=GPIO7, SCL=GPIO8)

### NeoTrellis
- Adafruit seesaw, dos tiles 4×4 en I2C_NUM_0
- Direcciones `0x2F` (izquierda) y `0x2E` (derecha) → matriz 4×8
- Pines: SDA=GPIO33, SCL=GPIO31

### RS485 P4
- Transceiver externo (no el integrado de la placa)
- TX=GPIO50, RX=GPIO51, EN=GPIO52
- 9 slaves en bus A

### Arquitectura tareas P4
- `taskCore0` / `taskCore1` dual-core
- Flags de cambio de página: `volatile bool g_switchToPage3/3A/3B/Offline`
- Race conocido en `vuLevels[]`: Core 0 escribe, Core 1 lee en `handleVUMeterDecay` — sin mutex actualmente

---

## Bugs conocidos activos

### P4
1. **`uiOfflineCreate()` doble llamada en `setup()`** — ~~primera antes de `prefs.begin()`, segunda después. Leak de LVGL garantizado.~~ **RESUELTO** — solo existe una llamada, después de `prefs.begin()`.
2. **Pantalla negra en boot** — Dos causas independientes: (a) ~~backlight off si `lastPage` era 1 o 2 (backlight solo se encendía vía `uiMenuInit()` → `uiPage3Create()`). Fix: `displaySetBrightness()` en `setup()` después de `initDisplay()`.~~ **RESUELTO**. (b) ~~`UIOffline` empieza con `s_blink_label` en HIDDEN y solo lo muestra cuando el logo termina de revelarse (~5-6 s); sin logo, pantalla negra permanente.~~ **RESUELTO** — label visible desde el inicio, parpadeo activo desde el primer tick independientemente del estado del logo.
3. **Handshake MCU incorrecto** — ~~código antiguo implementaba un challenge/response propio; P4 actuaba como HOST generando challenges aleatorios.~~ **RESUELTO** — ver sección "Mackie MCU — handshake" abajo.

### S3 Extender
- **Note Off en botones de transporte** — **RESUELTO** — `onButtonReleased` envía `0x80 + note + 0x00`.
- **Handshake MCU** — **RESUELTO** — protocolo completo implementado (ver sección handshake).
- **Transport LEDs** — **RESUELTO** — notas 94/95/97 mapeadas a LEDs físicos en `setLedByNote()`.

---

## Pendientes ordenados por complejidad (S3 → P4 → S2 → Cross-system)

### S3 Extender
1. LEDs REC y FF no encienden — verificar notas exactas que envía Logic para el Extender (pueden diferir del MCU principal)
2. VPot ring LEDs (CC 16–23, 48–55), jog wheel (CC 60), rude solo (nota 115)

### P4
3. mutex o double-buffer en `vuLevels[]`
4. respuesta táctil lenta en vista de faders — investigar qué bloquea el hilo de touch, especialmente en UIPage faders
5. no muestra datos en pantalla tras conectarse — posible regresión en la transición a CONNECTED tras cambios en handshake
6. NeoTrellis sin implementar — atención a pines I2C nuevos (SDA=GPIO33, SCL=GPIO31)

### S2
7. **Encoder salta pasos** — Gray code sin debounce. Ruido en pines A/B causa transiciones falsas. Fix: debounce timer (2-5ms) antes de procesar cambio en ISR
8. FaderTouch por varianza (plástico) — `TOUCH_VAR_THRESHOLD` etc ya en `config.h`
9. revisar FaderADC tras reescritura — validar lecturas actuales con hardware real
10. ADS1015 pedido — cuando llegue, reemplazar lectura ADC nativa por I2C ADS1015 para resolver ruido en fader

### Cross-system
11. RS485: verificar pines en S3 (legacy) y P4 (TX=50, RX=51, EN=52) — confirmar que ambos compilan y funcionan correctamente
12. LEDs NeoPixel: implementar control de brillo centralizado — estudiar si viable vía MIDI (CC o SysEx dedicado) para controlar brillo de todos los slaves desde Logic/P4

---

## Protocolo RS485

Structs `MasterPacket` / `SlavePacket` en `protocol.h`. CRC8. 500kbaud.

Flags slave → master incluyen `SLAVE_FLAG_NOT_CALIBRATED`. Master detecta y dispara `FLAG_CALIB` con hasta 3 reintentos.

---

## Mackie MCU — comportamiento Logic

- Logic solo envía automation state global via notas MIDI 74–78
- `GoOffline` SysEx: `F0 00 00 66 14 0F F7`
- En desconexión Logic envía `AllFadersToMinimum` SysEx + PitchWheel -8192 en todos los canales antes del `GoOffline`
- SELECT es latch-style
- MIDI fader max para recorrido físico completo: **14848** (no 16383)
- `track_idx = currentOffset / 7`, `char_pos = currentOffset % 7` — parser SysEx `0x12`

### Mackie MCU — handshake correcto (S3 Extender / familia 0x14)

Logic sondea varias familias. El dispositivo responde a **cualquier familia** para los comandos de sondeo, luego solo procesa familia `0x14`.

```
── Fase 0: sondeo (cualquier familia) ──────────────────────────────
Logic → Device:  F0 00 00 66 <any> 00 F7
Device → Logic:  F0 00 00 66 14 01 00 00 00 01 00 00 00 00 F7

Logic → Device:  F0 00 00 66 <any> 13 F7
Device → Logic:  F0 00 00 66 14 14 00 F7

── Fase 1: handshake (familia 0x14) ────────────────────────────────
Logic → Device:  F0 00 00 66 14 21 01 F7
Device → Logic:  F0 00 00 66 14 21 01 F7   → CONNECTED

Logic → Device:  F0 00 00 66 14 20 0x 07 F7  (×8, opcional)
Logic → Device:  F0 00 00 66 14 0A 01 F7
Device → Logic:  F0 00 00 66 14 0A 01 F7

Logic → Device:  F0 00 00 66 14 0C 00 F7
Device → Logic:  F0 00 00 66 14 0C 00 F7
Device → Logic:  F0 00 00 66 14 10 00 F7   ← suscripción a feedback (inmediato)

Logic → Device:  F0 00 00 66 14 0B 0F F7
Device → Logic:  F0 00 00 66 14 0B 0F F7
```

- `DEVICE_FAMILY 0x14` para P4 y S3 Extender — Logic configurado con **dos "Mackie Control"**
- `CONNECTED` se establece al recibir `0x21` (con `connectedSinceTime = millis()`)
- `0x0C` hardcodeado a `0x00` (Surface Type = Master) — **no cambiar**, necesario para recibir transport LEDs
- `0x10 00` es la suscripción a feedback — sin él Logic no envía Note On de transporte

### Mackie MCU — transport LEDs (S3 Extender)

Logic envía notas en canal 1 para controlar los LEDs de transporte:

| Nota | Decimal | Vel 127 | Vel 0 |
|------|---------|---------|-------|
| A♯5 | 94 | PLAY on + STOP off | PLAY off + STOP on |
| B5  | 95 | REC on | REC off |
| C♯6 | 97 | FF on  | FF off |

Implementado en `Transporte::setLedByNote()`. La nota 94 controla Play y Stop de forma complementaria.

---

## Convenciones de código

- Cambios quirúrgicos, no rewrites completos salvo petición explícita
- `Serial.printf` en S2, `log_i`/`log_w` en P4
- SatMenu callbacks en `main.cpp`: funciones estáticas nombradas, no lambdas (ICE en xtensa)
- Logic es la única fuente de verdad para estados de botones — S2 nunca hace toggle local
- Un proyecto PlatformIO por variante MCU — `config.h` de S3/S2 son independientes
- No tocar código sin ver primero los ficheros reales

---

## Repositorio

GitHub: `juliannof/iMakie`
Local: `/Users/julianno/Documents/PlatformIO/Projects/iMakie/`

Subproyectos PlatformIO:
- `S3/` — master
- `track S2/` — slave
