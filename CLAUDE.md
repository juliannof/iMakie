# iMakie — Contexto para Claude Code

---

## ⚠️ DIRECTIVA OBLIGATORIA

**NUNCA modificar código sin explicar PRIMERO con detalle qué vas a hacer.**

- Antes de tocar cualquier archivo: describe el cambio (qué líneas, por qué, qué efecto tiene)
- Espera aprobación explícita del usuario
- Si el usuario dice "hazlo" o "proceed", ENTONCES ejecuta los cambios
- No importa si es un cambio "pequeño" o "obvio" — SIEMPRE explica primero
- Esta regla es VINCULANTE en cada nuevo chat

---

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
`"ptxx"` — claves: `wifiSsid`, `wifiPass`, `otaPass`, `trackId`, `label`, `pwmMin`, `pwmMax`, `touchEn`, `motorDis`

---

## Arquitectura S2 — Flujo de datos

### Loop principal (main.cpp)

```
loop() {
  1. ButtonManager::update()        ← botones REC/SOLO/MUTE/SELECT → flags
  2. RS485 receive (si hay data):
     - RS485Handler::onMasterData() ← recibe faderTarget, nombre, flags
     - buildResponse()              ← arma SlavePacket con estado actual
     - sendResponse()               ← envía al master S3 o P4
     - Encoder::reset()             ← limpia delta acumulado
  3. faderADC.update()              ← lee ADC 13-bit: 0-8191
  4. FaderTouch::update()           ← detección sostenimiento: >1.5% baseline × 120ms
  5. Motor::update()                ← control PID hacia faderTarget
  6. Encoder::update()              ← acumula deltas (-127..+127)
  7. updateDisplay()                ← pantalla SPI3
  8. updateAllNeopixels()           ← LEDs
}
```

### Botones (4 contactos digitales)

| Botón | GPIO | Lógica | Envío |
|-------|------|--------|-------|
| REC | 37 | Baja=presionado | Bit 0 en SlavePacket.buttons |
| SOLO | 38 | Baja=presionado | Bit 1 en SlavePacket.buttons |
| MUTE | 39 | Baja=presionado | Bit 2 en SlavePacket.buttons |
| SELECT | 40 | Baja=presionado | Bit 3 en SlavePacket.buttons |

**Ciclo botones:**
1. `ButtonManager::update()` lee GPIO, debounce 20ms
2. Detecta flanco ascendente (release) → flag `buttonPressed`
3. `buildResponse()` encapsula bit en `SlavePacket.buttons` (bits 0-3)
4. Master recibe, mapea a nota MIDI, envía a Logic
5. **IMPORTANTE:** Logic es fuente única de verdad — S2 nunca hace toggle local

**¿Por qué no funciona SELECT actualmente?**
- Revisa `ButtonManager::update()` — ¿lee GPIO40?
- Revisa debounce: ¿20ms es suficiente? (comparar con REC/SOLO/MUTE)
- Revisa `buildResponse()` — ¿pasa bit 3 correctamente?
- Revisa RS485 — ¿llega el paquete al master sin timeouts?

### Fader (ADC GPIO10)

**Hardware:**
- ESP32-S2 ADC1_CH9 (GPIO10)
- Resolución: 13-bit (0-8191)
- Atenuación: 11dB (0-3.3V)
- Ruido: ±30 cuentas típico (→ ADS1015 será fix futuro)

**Firmware:**
- `FaderADC::update()` lee crudo + EMA lowpass (alpha=0.20)
- `FaderTouch::update()` detecta toque por sostenimiento:
  - raw > (baseline × 1.015) sostenido > 6 frames (120ms) = TOQUE
  - baseline actualizado siempre con IIR (alpha=1/16, NO congelado)
  - Perfect sin plástico, validación pendiente con plástico

**Salida:**
- `SlavePacket.faderPos` = valor 13-bit actual
- Master lee, interpola, envía `faderTarget` 14-bit vía RS485
- Motor sigue `faderTarget` usando calibración (no PID)

### Motor (DRV8833 H-bridge)

**Control:**
- IN1=GPIO14, IN2=GPIO16 (dirección via digitalWrite)
- PWM=GPIO18 (analogWrite 0-255)
- Sensor: faderADC feedback

**Estados calibración (máquina de estado):**
```
IDLE
  ↓ (detecta FLAG_CALIB)
KICK_UP (max PWM, 500ms) → fuerza movimiento inicial
  ↓
GOING_UP (PWM mediano, hasta ADC máximo)
  ↓
SETTLE_UP (PWM bajo, estabiliza 200ms)
  ↓
KICK_DOWN → GOING_DOWN → SETTLE_DOWN (simétrico)
  ↓
CALIBRATED (marca SLAVE_FLAG_CALIB_DONE, responde bien a targets)
```

**Control normal (post-calibración):**
- `Motor::setADC(currentPos)` — posición actual de fader
- `Motor::setTarget(target)` — posición deseada del master
- Dead zone: si |error| < 50 cuentas, motor apagado
- Si tocado → motor para inmediatamente (evita conflicto)

### Pantalla (ST7789V3 240×280)

**Layout:**
```
[Header 40px]        ← nombre track + flags (REC/SOLO/MUTE/SELECT)
[Main Area]          ← gráfico barras/fader + info
[VU Meter 60px]      ← pico + decay exponencial
[VPot Ring 60px]     ← anillo 15 posiciones (-7..+7) + encoder

Total: 240×400 (offset_y=20 en ST7789 interno)
```

**Sprites PSRAM:**
- `header` (240×40) — track name, flags
- `mainArea` (180×240) — main display
- `vuSprite` (60×240) — meter vertical
- `vPotSprite` (240×60) — ring + encoder level

**Actualización:**
- `updateDisplay()` en loop, redraw si `needsTOTALRedraw` o `needsVPotRedraw`
- SPI3_HOST, freq_write=5MHz, freq_read=8MHz
- BL PWM 500Hz, GPIO3

### Encoder (Rotario infinito)

**Hardware:**
- A=GPIO12, B=GPIO13 (rotatorio Gray code)
- Push=GPIO21 (debounce 3ms)
- Acciona VPot ring: -7 a +7

**Firmware:**
- Lógica única en `Encoder.cpp` (SAT no duplica)
- Debounce 3ms, derecha=+1, izquierda=-1
- Delta acumulado resetea tras `buildResponse()`
- `Encoder::currentVPotLevel` rango 0-14 mapea pantalla

### NeoPixels (12 LEDs)

**Hardware:**
- Adafruit NeoPixel GPIO36
- 12 × WS2812B RGB 5050
- Una sola instancia global en `Neopixel.cpp`

**Estados LED:**
- **0-3:** Botón corresponding (REC/SOLO/MUTE/SELECT)
  - Color según Logic feedback vía MIDI
  - Parpadea si estado "semi-active"
- **4-10:** VU meter visual (correlaciona `vuLevel`)
  - Rojo > 24, amarillo 12-23, verde 0-11
- **11:** Status (verde=OK, naranja=timeout RS485, rojo=error)

### Ciclo de comunicación RS485

```
1. Master envía MasterPacket (16B)
   - trackName, faderTarget, vuLevel, flags, etc.
   
2. S2 recibe → RS485Handler::onMasterData()
   - Actualiza: trackName, faderTarget, botones esperados, etc.
   - Marca lastRxTime

3. S2 construye SlavePacket (9B)
   - faderPos, touchState, buttons (REC/SOLO/MUTE/SELECT)
   - encoderDelta, encoderButton, flags de calibración
   
4. S2 envía respuesta inmediatamente
   - ANTES de display/motor/neopixel update
   - **CRÍTICO:** si esto tarda >150µs, master timeout
   
5. RS485Handler::checkTimeout(lastRxTime)
   - Si ms > 1000 sin recibir → LED11=rojo, suspende motor
   - Si vuelve → LED11=verde, reanuda

6. Master timeout → reintenta 3 veces con FLAG_CALIB
   - Si error persiste, marca `NOT_CALIBRATED`
```

### Órdenes de init crítico

```c
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
- Motor::init() silencia antes de Serial debug output
- RS485 debe iniciar cuando todo está listo (si falla, no bloquea boot)
- Display + NeoPixels comparten SPI, timing crítico

### SAT (Sistema de Auto-Test)

Menú en display (Encoder push > 3s):
- **Motor Off/On** — deshabilita motor
- **Motor Drive** — manual PWM test
- **Brightness** — test backlight
- **RS485 On/Off** — simula desconexión
- **LEDs Test** — secuencia RGB por índice
- **WiFi OTA** — carga firmware via WiFi
- **Reboot** — reinicia

**Nota:** SAT suspende PSRAM y sprites → libera RAM para diagnósticos

### Build S2
- Platform: pioarduino 55.03.37 / IDF5 — unificado con P4 (requiere P4)
- LovyanGFX 1.2.19, Adafruit NeoPixel (cambio desde NeoPixelBus)
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

## Hardware S3 (Extender)

**Chip:** ESP32-S3 | **Familia Mackie:** 0x14 | **Slaves:** 8 (IDs 1–8)

### RS485 (bus B)
- TX=GPIO15, RX=GPIO16, EN=GPIO1
- 500 kbaud, CRC8
- Controla 8 slaves S2 (IDs 1–8)
- Timing: TX_EN=10µs, TX_DONE=10µs, RESP_TIMEOUT=1500µs, GAP=300µs, POLL_CYCLE=20ms

### Transporte — Botones y LEDs
| Función | LED GPIO | BTN GPIO |
|---------|----------|----------|
| REC | 12 | 11 |
| PLAY | 10 | 9 |
| FF | 8 | 7 |
| STOP | 6 | 5 |
| RW | 4 | 3 |

### Notas MIDI Transporte
| Función | Nota (hex) | Decimal |
|---------|-----------|---------|
| RW | 0x5B | 91 |
| FF | 0x5C | 92 |
| STOP | 0x5D | 93 |
| PLAY | 0x5E | 94 |
| REC | 0x5F | 95 |

- Recibe feedback de Logic vía MIDI handshake familia `0x14`
- LEDs controlados por `Transporte::setLedByNote()` con velocidad 127 (on) / 0 (off)

---

## Bugs conocidos activos

### P4
1. **`uiOfflineCreate()` doble llamada en `setup()`** — ~~primera antes de `prefs.begin()`, segunda después. Leak de LVGL garantizado.~~ **RESUELTO** — solo existe una llamada, después de `prefs.begin()`.
2. **Pantalla negra en boot** — Dos causas independientes: (a) ~~backlight off si `lastPage` era 1 o 2 (backlight solo se encendía vía `uiMenuInit()` → `uiPage3Create()`). Fix: `displaySetBrightness()` en `setup()` después de `initDisplay()`.~~ **RESUELTO**. (b) ~~`UIOffline` empieza con `s_blink_label` en HIDDEN y solo lo muestra cuando el logo termina de revelarse (~5-6 s); sin logo, pantalla negra permanente.~~ **RESUELTO** — label visible desde el inicio, parpadeo activo desde el primer tick independientemente del estado del logo.
3. **Handshake MCU incorrecto** — ~~código antiguo implementaba un challenge/response propio; P4 actuaba como HOST generando challenges aleatorios.~~ **RESUELTO** — ver sección "Mackie MCU — handshake" abajo.

### S3 Extender
- **Note Off en botones de transporte** — **RESUELTO** — `onButtonReleased` envía `0x80 + note + 0x00`.
- **Handshake MCU** — **RESUELTO** — protocolo completo implementado (ver sección handshake).
- **Transport LEDs** — **RESUELTO** — notas 91–95 mapeadas a LEDs físicos en `setLedByNote()`.
- **RS485 intermitente** — **FUNCIONANDO CON TIMEOUTS** — Sistema comunica: LEDs actualizan, pantalla muestra datos. Timeouts ocasionales e impredecibles (~10-20 consecutivos, luego OK, repite). Comunicación física funciona a 500kbaud. NO se debe modificar arquitectura actual de lectura sin probar compilación first.

### S2
- **OTA WiFi no conecta en CIERTAS unidades — PROBLEMA DE HARDWARE** — En `OtaManager::enableForUpload()`, WiFi falla a conectarse (timeout 10s, error "No response from device" en cliente OTA). RS485 se deshabilita correctamente (EN=HIGH, GPIO8=LOW, Serial1.end()). **Causa:** problema de alimentación específico en esas unidades (regulador dañado, capacitor muerto, o soldadura mala en PCB). Afecta solo a ciertos S2, no todos. **Fix:** (a) desconectar cables RS485 antes de OTA, (b) alimentar con PSU de 5V @ 1A+ en lugar de USB, (c) si persiste, revisar físicamente PCB (soldaduras, regulador 3.3V, capacitores). Verificar brownout: `[BROWNOUT]` o `[RST]` en logs durante OTA.

---

## Pendientes ordenados por complejidad (S3 → P4 → S2 → Cross-system)

### S3 Extender
1. **LED REC — RESUELTO**
2. **LED FF — RESUELTO**
3. **LED RW — RESUELTO**
4. **RS485 intermitente — DOCUMENTADO, BAJO CONTROL** — Sistema S3 funciona: comunica datos, LEDs y pantalla actualizan correctamente. Timeouts ocasionales no bloquean operación. Patrón: ~10-20 timeouts, luego respuesta OK, repite. Causa desconocida (posible: timing hardware, timeout 1500µs, ISR conflicts). No intentar buffer circular o cambios arquitectónicos sin compilar first. Problema arrastrado desde S3 original.
5. VPot ring LEDs (CC 16–23, 48–55), jog wheel (CC 60), rude solo (nota 115)

### P4
6. mutex o double-buffer en `vuLevels[]`
7. respuesta táctil lenta en vista de faders — investigar qué bloquea el hilo de touch, especialmente en UIPage faders
8. no muestra datos en pantalla tras conectarse — posible regresión en la transición a CONNECTED tras cambios en handshake
9. NeoTrellis sin implementar — atención a pines I2C nuevos (SDA=GPIO33, SCL=GPIO31)

### S2
10. **Encoder — RESUELTO (2026-04-28)** — Problema de sequenciamiento: `Encoder::reset()` estaba en línea 214 (inmediatamente post-RS485), antes de procesar VPot. Esto causaba contador=0 al leer para VPot → VPot ring nunca cambiaba en Logic. SAT funcionaba porque procesaba sin ese reset intermedio. Fix: mover `reset()` a línea 242 (post-VPot, pre-updateDisplay) asegura que RS485 y pantalla usan el mismo delta.
11. **FaderTouch — EN DESARROLLO** — Detección por sostenimiento (tiempo). Perfecto sin plástico, necesita ajuste con plástico. Lógica actual: raw debe sostenerse > baseline×1.015 durante 6 frames (120ms) para detectar toque. Baseline actualizado siempre con IIR (alpha=1/16, no congelado). TEST_TOUCH en SAT testea correctamente. Próximos pasos: validar con plástico real, ajustar thresholds si es necesario.
12. revisar FaderADC tras reescritura — validar lecturas actuales con hardware real
13. ADS1015 pedido — cuando llegue, reemplazar lectura ADC nativa por I2C ADS1015 para resolver ruido en fader

### S2 — NeoPixel (continuación)
**NeoPixel secuencia de brillo (2026-04-28) — IMPLEMENTADO:**
- Azul tenue (NEOPIXEL_DIM_BRIGHTNESS=5) al inicio/reposo
- Colores muy tenues (NEOPIXEL_ULTRA_DIM=1) cuando Logic conecta primera vez
- O encendido (NEOPIXEL_DEFAULT_BRIGHTNESS=30) o tenue de morir (NEOPIXEL_ULTRA_DIM=1)
- Optimización monocore: detección de cambios interna sin flags innecesarios
- Comparación de estado (neoWaitingHandshake + 4 botones) en updateAllNeopixels()

**HW_STATUS display en boot screen (2026-04-28) — IMPLEMENTADO:**
- 10 componentes hardware con estado color-coded (Rojo=0, Naranja=1, Blanco=2)
- Fuente bitmap pequeña (setTextFont(1)) consistente con SAT
- Inyección automática vía pre_build.py desde config.h markers

### Cross-system
14. RS485: verificar pines en S3 (legacy) y P4 (TX=50, RX=51, EN=52) — confirmar que ambos compilan y funcionan correctamente
15. LEDs NeoPixel master: implementar control de brillo centralizado — estudiar si viable vía MIDI (CC o SysEx dedicado) para controlar brillo de todos los slaves desde Logic/P4

---

## Protocolo RS485

**→ [Ver documentación completa en `docs/RS485_PROTOCOL.md`](docs/RS485_PROTOCOL.md)**

RS485 es el bus serial que conecta master (P4 o S3) con slaves (S2):
- **Baudrate:** 500 kbaud, 8N1
- **Protocolo:** Binario custom, CRC8, topología star
- **Timing:** Ciclo ~20ms para 8 slaves, timeouts críticos en microsegundos
- **Bus A (P4):** 9 slaves, TX=GPIO50, RX=GPIO51, EN=GPIO52
- **Bus B (S3):** 8 slaves, TX=GPIO15, RX=GPIO16, EN=GPIO1
- **Slaves (S2):** TX=GPIO8, RX=GPIO9, EN=GPIO35

Toda la especificación de paquetes (MasterPacket, SlavePacket), máquina de estados, timing, CRC, troubleshooting y optimizaciones históricas está en [`docs/RS485_PROTOCOL.md`](docs/RS485_PROTOCOL.md).

---

### Auditoría RS485 (2026-04-27)

**Hallazgo:** Código RS485 está **bien optimizado**. Neopixel bloqueante (15-30ms) ya fue removido de ruta crítica en optimización anterior.

**Documentos de auditoría:**

| Documento | Contenido |
|---|---|
| [`RS485_TIMING_AUDIT.md`](docs/RS485_TIMING_AUDIT.md) | Análisis línea-por-línea de timing en onMasterData(), sendResponse(), updateDisplay(), updateAllNeopixels(). Identifica bottlenecks residuales y propone mediciones. |
| [`RS485_COMMENTING_GUIDE.md`](docs/RS485_COMMENTING_GUIDE.md) | Patrones de comentarios para documentar restricciones de timing. Usa cuando edites RS485Handler.cpp, RS485.cpp o main.cpp. |
| [`RS485_NEXT_ACTIONS.md`](docs/RS485_NEXT_ACTIONS.md) | Plan de 6 tareas para próximas sesiones: instrumentación de timing (30min), comentar código (45min), validar con hardware (1h), proponer mejoras (opt). |

**Resumen:**
- ✅ RS485Handler::onMasterData() — 200-400µs (correcto, sin bloqueos)
- ✅ RS485Slave::sendResponse() — 360µs (timing setup/hold correcto)
- ⚠️ updateDisplay() — 10-100ms variable (post-RS485, no crítico)
- ⚠️ updateAllNeopixels() — 15-30ms bloqueante (post-sendResponse, pero medir varianza)

**Próximo:** Ejecutar instrumentación de timing para confirmar métricas reales vs. teóricas.

---

## Encoder — Arquitectura y sequenciamiento (S2)

**Fuente única de verdad:** `src/hardware/encoder/Encoder.cpp` (confirmado 2026-04-27)
- ISR basada en cambio de flanco (CHANGE) en GPIO12 y GPIO13
- Debounce 3ms en ISR (válido)
- Dirección: A LOW + B HIGH = -1 (izquierda), A LOW + B LOW = +1 (derecha)
- Sin duplicados en SAT ni main.cpp

**Usuarios correctos:**
- `RS485Handler::buildResponse()` → captura delta para enviar al master
- `main.cpp` → calcula nivel VPot (-7..+7) y redibuja pantalla

**RESUELTO (2026-04-28) — Sequenciamiento corregido:**

Problema: `Encoder::reset()` estaba en la línea 214 de main.cpp (post-RS485, antes de procesar VPot), causando que el contador fuera 0 al leer para VPot → VPot ring nunca cambiaba en Logic. SAT funcionaba porque procesaba el contador sin ese reset intermedio.

Solución implementada: Mover `Encoder::reset()` a línea 242 (post-VPot, pre-updateDisplay). Ahora el flujo es:

```cpp
// ✓ ORDEN IMPLEMENTADO (main.cpp línea 205-246)
if (rs485.hasNewData()) {
    SlavePacket resp = RS485Handler::buildResponse(...);  // captura delta
    rs485.sendResponse(resp);
    // NO resetear aquí
}

// ... fader, motor, ...

if (!satMenu->isEncoderConsumed()) {
    Encoder::update();
    if (Encoder::hasChanged()) {
        int newLevel = constrain((int)(Encoder::getCount() / 4), -7, 7);
        if (newLevel != Encoder::currentVPotLevel) {
            Encoder::currentVPotLevel = newLevel;
            needsVPotRedraw = true;
        }
    }
}

Encoder::reset();  // RESET al final, post-VPot, pre-updateDisplay

updateButtons();
updateDisplay();  // redibuja con nuevos niveles
```

Resultado: RS485 y pantalla ahora usan el mismo delta, VPot ring responde correctamente en Logic.

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

| Nota | Decimal | LED GPIO | Vel 127 | Vel 0 |
|------|---------|----------|---------|-------|
| 0x5B | 91 | GPIO4 (RW) | RW on | RW off |
| 0x5C | 92 | GPIO8 (FF) | FF on | FF off |
| 0x5D | 93 | GPIO6 (STOP) | STOP on | STOP off |
| 0x5E | 94 | GPIO10 (PLAY) | PLAY on | PLAY off |
| 0x5F | 95 | GPIO12 (REC) | REC on | REC off |

Implementado en `Transporte::setLedByNote()`.

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
