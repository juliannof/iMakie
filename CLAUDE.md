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
(MCU #1, 9 tracks, IDs 1–9)

ESP32-P4  ←→  RS485 bus B  ←→  8× ESP32-S2 (PTxx Track)
(MCU #2 / Extender, IDs 1–8)
```

- **P4 #1** reemplaza S3 #1 (MCU). **P4 #2** reemplaza S3 #2 (Extender).
- **S2 slaves** son el hardware definitivo — 17 unidades Lolin S2 Mini.
- RP2040 descartado permanentemente. S2 es la plataforma slave definitiva.

---

## Subproyectos PlatformIO

| Directorio | MCU | Rol |
|---|---|---|
| `S3/` | ESP32-S3 (legacy) / ESP32-P4 | Master MCU — USB-MIDI + RS485 |
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
- Librería: `makuna/NeoPixelBus@^2.8.4`, método `NeoEsp32I2s0800KbpsMethod`
- **Una sola instancia global** — segunda instancia reinicializa I2S0 y corrompe el strip
- Acceso exclusivamente a través de `Neopixel.cpp`

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
- **Note Off ausente en botones de transporte** — ~~`onButtonPressed` enviaba Note On pero no había handler de release; Logic veía los botones como eternamente pulsados.~~ **RESUELTO** — `onButtonReleased` en `Transporte.cpp` envía `0x80 + note + 0x00` al soltar.
- **Handshake MCU incorrecto** — ~~mismo problema que P4; S3 anunciaba `0x00` periódicamente siendo Logic quien debe iniciar.~~ **RESUELTO** — protocolo estándar implementado (commit d80f15f).
- **DEVICE_FAMILY cambiado a 0x14** — S3 Extender ahora se anuncia como familia 0x14 (igual que P4). Logic configurado con dos "Mackie Control" en vez de MCU + Extender. Los botones de transporte funcionan.
- **Echo de comandos de configuración** — añadido echo de SysEx 0x20, 0x21, 0x0A, 0x0B, 0x0C en `MIDIProcessor.cpp`.
- **Transport LEDs pendiente** — Logic envía notas de transporte al Extender (confirmado funcionando en sesiones anteriores). Investigación en curso para estabilizar el comportamiento.

---

## Pendientes ordenados por prioridad

1. **S3 Extender: Transport LEDs** — Logic envía las notas (confirmado), falta estabilizar el manejo en firmware. Investigar qué mensajes exactos llegan al Extender en MIDI Monitor filtrado por "iMakie-Extender".
2. S2: FaderTouch por varianza (plástico) — `TOUCH_VAR_THRESHOLD` etc ya en `config.h`
3. S3/P4 MCU: VPot ring LEDs (CC 16–23, 48–55), jog wheel (CC 60), rude solo (nota 115)
4. P4: considerar mutex o double-buffer en `vuLevels[]`

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

### Mackie MCU — handshake correcto

Logic es siempre el iniciador. El dispositivo nunca envía `0x00` por su cuenta.

```
Logic → Device:  F0 00 00 66 <family> 00 F7          (Device Query)
Device → Logic:  F0 00 00 66 <family> 01 <serial 4B> <version 4B> F7

Logic → Device:  F0 00 00 66 <family> 13 F7          (Host Connection Query, 8-10 veces)
Device → Logic:  F0 00 00 66 <family> 14 00 F7       → logicConnectionState = CONNECTED
```

- `DEVICE_FAMILY`: `0x14` tanto para P4 MCU como para S3 Extender — Logic configurado con **dos unidades "Mackie Control"** (no MCU + Extender)
- Logic sondea múltiples familias (0x10, 0x11, 0x14, 0x15, 0x17) — responder solo a la propia
- `case 0x13` en `processMackieSysEx` tiene guard `if (logicConnectionState != CONNECTED)` para no re-disparar calibración y cambio de página en las 8-10 queries repetidas
- `case 0x15` responde con versión de firmware (`F0 00 00 66 <family> 15 '1'.'.'2'.'.'0' F7`)

### Mackie MCU — comandos de configuración que requieren echo

Logic envía estos SysEx tras el handshake y espera que el dispositivo los devuelva tal cual para completar la inicialización:

- `0x20` — Fader touch sensitivity
- `0x21` — Fader touch sensitivity (canal)
- `0x0A` — LCD backlight timeout
- `0x0B` — MIDI port routing
- `0x0C` — Fader motor enable

Sin el echo, Logic no completa la inicialización y no envía estado de botones/LEDs. El echo está implementado en `processMackieSysEx` (S3 Extender `MIDIProcessor.cpp`).

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
