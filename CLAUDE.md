# iMakie — Contexto para Claude Code

<!-- 🔒 ⚠️ DESHABILITADO (2026-05-16 09:00): CLAUDE.md se sube a GitHub por ahora
- Contiene arquitectura sensible, pines GPIO, vulnerabilidades conocidas
- Si el repo es público → agregar a `.gitignore`
- Si el repo es privado → OK mantenerlo
- Comando: `echo "CLAUDE.md" >> .gitignore && git rm --cached CLAUDE.md`
-->

---

## 📋 ARQUITECTURA (Consulta directa)

**→ [Ver en STATUS.md](STATUS.md)** — Referencia única de componentes, flujos MIDI, RS485, y puntos críticos.  
**No preguntar sobre arquitectura. Consultar STATUS.md.**

---

## ⚠️ DIRECTIVA OBLIGATORIA

**IDIOMA — Español de España SIEMPRE (2026-05-11 18:30):**
- Comunicación con el usuario: únicamente en español de España
- Vosotros, distinción theta-zeta, léxico peninsular
- Nada de "che", "boludo", "mate" ni otros regionalismos
- Tono: directo, sin rodeos, profesional
- Esta regla es VINCULANTE en CADA respuesta

**NUNCA modificar código sin explicar PRIMERO con detalle qué vas a hacer.**

- Antes de tocar cualquier archivo: describe el cambio (qué líneas, por qué, qué efecto tiene)
- Espera aprobación explícita del usuario
- Si el usuario dice "hazlo" o "proceed", ENTONCES ejecuta los cambios
- No importa si es un cambio "pequeño" o "obvio" — SIEMPRE explica primero
- Esta regla es VINCULANTE en cada nuevo chat

**Documentación — Fecha y hora obligatorios (2026-05-04 19:20):**
- Cuando documentes cambios en CLAUDE.md o en comentarios de código: **SIEMPRE incluye fecha + hora**
- Formato: `(YYYY-MM-DD HH:MM)` ejemplo: `(2026-05-04 14:30)`
- Si un cambio está documentado sin fecha/hora, **cuestiona su validez antes de aplicarlo**
- La fecha/hora permite rastrear evolución y correlacionar con commits/sesiones
- Sin fecha, el cambio es inútil para auditoría y debugging futuro

**Commits a GitHub:**
- Cuando el usuario dice **"commit"** o **"commitea"** = **git commit + git push** automático
- No es solo guardar local — es guardar + subir a GitHub en una operación
- **Fecha/hora en mensajes de commit (2026-05-11 09:00):** antes de hacer commits con timestamp, usar `date` para obtener hora actual del sistema
  - Nunca asumir la hora — siempre verificar con `date` en bash
  - Formato: `(YYYY-MM-DD HH:MM)` ejemplo: `(2026-05-11 09:00)`
  - Esto asegura que la hora en commits es real, no adivinada

**COMPILACIÓN — PROHIBIDO COMPLETAMENTE (2026-05-10 19:30):**
- **NUNCA compilar bajo ningún concepto ni circunstancia**
- No invocar platformio, pio, python, o herramientas de build
- No intentar validar código mediante compilación
- Cambios de código se entregan sin compilación — el usuario compila en su máquina
- Esta regla es ABSOLUTA e INVIOLABLE

**🔒 HARDWARE LOCKED — NO CAMBIOS FÍSICOS (2026-05-10 22:00):**
- **El hardware ES el que ES. Hacerlo funcionar POR SOFTWARE.**
- **NUNCA proponer cambiar cables, pines, soldaduras, o conexiones físicas**
- Incluso si "sería más fácil", la respuesta es: invertir lógica/compensar en código
- Esto incluye: no sugerir intercambiar motores, cambiar DRV8833, resoldear, etc.
- Ejemplo: cables al revés → invertir _hwUp/_hwDown en software, NO cambiar cables físicos
- Si diagnosticas problema físico → documentar, no corregir
- Esta regla es ABSOLUTA — hardware no se toca bajo ninguna circunstancia

**Código Moderno — Alineación con Stack de Librerías (2026-05-10 19:45):**
- **Usar MISMAS APIs que las librerías del proyecto usan internamente**
- **Motor S2:** usa `analogWrite` (no LEDC) — conflicto de canales LEDC con LovyanGFX backlight (2026-05-10 19:54)
  - analogWrite es simple, robusta, sin conflictos de recursos
  - LEDC migración fue revertida tras identificar agotamiento de canales
- Adafruit librerías usan Wire moderno → I2C DEBE usar Wire (no legacy)
- IDF5/pioarduino 55.03.37 es la base → APIs deben ser compatibles
- Validar SIEMPRE retorno de inicializaciones: `if (!Wire.begin())`, etc.
- Logging: usar log_i/log_e (no Serial legacy) — ya integrado en setup()
- Comentar versión de API usada en código (ej: "analogWrite PWM 20kHz 8-bit")

**Desarrollo de código:**
- Cambios quirúrgicos, no rewrites completos salvo petición explícita
- `Serial.printf` en S2, `log_i`/`log_w` en P4
- SatMenu callbacks en `main.cpp`: funciones estáticas nombradas, no lambdas (ICE en xtensa)
- Logic es la única fuente de verdad para estados de botones — S2 nunca hace toggle local
- Un proyecto PlatformIO por variante MCU — `config.h` de S3/S2 son independientes
- No tocar código sin ver primero los ficheros reales

**Variables de estado y configuración — SIEMPRE en config.h (2026-05-10 13:43):**
- **NUNCA** declarar `static` variables de estado/configuración en .cpp archivos
- Todas las variables `static` deben estar en `config.h` como fuente única de verdad
- Incluye: constantes PWM, umbrales calibración, variables de estado de hardware (ADC min/max, etc.)
- Razón: config.h es el punto central de referencia — permite auditoría, debugging, y cambios sin búsquedas de código
- Excepción: funciones helper privadas file-scope en .cpp (sufijos `_hw`, `_calib`) pueden tener variables locales transitorias
- Si una variable afecta comportamiento o calibración → va en config.h

**Sincronización Código ↔ CLAUDE.md:**
- Cuando cambies arquitectura de `loop()`, orden de `init()`, pines GPIO, o tiempos críticos → **actualiza CLAUDE.md con los diagramas/tablas correspondientes**
- Cambios pequeños (bug fixes, optimizaciones locales) no requieren actualización
- CLAUDE.md es fuente de verdad para decisiones de diseño

**🏗️ ARQUITECTURA DE MÓDULOS — REGLAS A FUEGO (2026-05-12 00:45):**
- **NUNCA reescribir funciones que existan en módulos de producción**
- **Módulos de producción:** Motor, FaderADC, FaderTouch, ButtonManager, Encoder, RS485Handler, Display
- **SatMenu es CONSUMIDOR de módulos, NUNCA reimplementación**
- **Antes de escribir cualquier función nueva:**
  1. Verificar si existe en Motor.cpp, FaderADC.cpp, FaderTouch.cpp, ButtonManager.cpp, Encoder.cpp, RS485Handler.cpp, Display
  2. Si existe → usar directamente (include + llamada a función pública)
  3. Si NO existe → crear en módulo correspondiente, NO en SatMenu
- **Si algo es inesperado → preguntar antes de actuar**
- **Un cambio a la vez, confirmar antes de continuar**
- Esta regla es ABSOLUTA — evita duplicación de código y mantiene cohesión arquitectónica

**⚠️ VALIDACIÓN EN HARDWARE — OBLIGATORIA (2026-05-13 15:59):**
- **TODO cambio en RS485, Motor, FaderADC, o protocolo REQUIERE validación en hardware**
- **NUNCA desplegar cambios RS485 sin test físico en rig S3-S2**
- **Mínimo requerido:**
  - S3 arranca → ordena calibración a S2
  - S2 calibra → envía min/max → S3 captura
  - Logic envía PitchBend → S3 mapea → S2 motor sigue
  - Validar: fader llega a 100%, motor es suave, sin lag
- **Si cambio afecta Motor, FaderADC, o calibración → test en bench ANTES**
- **RS485 es crítico.** Un bug puede dejar el sistema no operacional (incomunicación total S3-S2)
- Esta regla es VINCULANTE — sin validación hardware, NO merge/deploy

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

## Estructura de carpetas (2026-05-12 17:47)

```
/AITEC/
├── MASTER_S3-P4/
│   ├── P4/                            ← Master MCU (ESP32-P4)
│   ├── S3/iMakie-ESP32_S3_EXTENDER/   ← Extender (ESP32-S3)
├── S2/S2_V1/                          ← Slaves (ESP32-S2 ×17)
├── C3_PowerRelay/                     ← Proyecto separado (power control)
├── CLAUDE.md                          ← Este archivo (directivas obligatorias)
├── STATUS.md                          ← Referencia técnica de componentes
├── docs/                             ← 📌 DOCUMENTACIÓN CENTRALIZADA
│   ├── FADER.md                      ← ADS1115, calibración, mapping (2026-05-16)
│   ├── MOTOR.md                      ← DRV8833, control, SAT (2026-05-16)
│   ├── RS485.md                      ← Protocolo, timing, troubleshooting (2026-05-16)
│   ├── WIFI.md                       ← Provisioning, OTA, ElegantOTA (2026-05-16)
│   ├── BUTTONS.md                    ← Debounce, ButtonManager, mapeo MIDI (2026-05-16)
│   ├── DISPLAY.md                    ← ST7789V3, sprites PSRAM, layout (2026-05-16)
│   ├── ENCODER.md                    ← ISR Gray code, sequenciamiento, SAT (2026-05-16)
│   ├── LEDS.md                       ← NeoPixels WS2812B, asignación, estados (2026-05-16)
│   └── SAT.md                        ← Sistema Auto-Test, navegación, integración (2026-05-16)
├── CHANGELOG.md                       ← Historial de cambios
├── README.md                          ← Intro repo
└── platformio.ini                     ← Índice de subproyectos
```

**📌 DOCUMENTACIÓN CENTRALIZADA (2026-05-16):**
- **FADER.md** — ADS1115 ADC, calibración bidireccional, mapping Logic↔ADC
- **MOTOR.md** — DRV8833, máquina de estados, guard cooldown, SAT calibración
- **RS485.md** — Protocolo binario, timing, paquetes, máquina estados
- **WIFI.md** — Provisioning credenciales, OTA, ElegantOTA, NVS
- **BUTTONS.md** — Debounce, ButtonManager, mapeo MIDI, troubleshooting
- **DISPLAY.md** — ST7789V3, sprites PSRAM, layout, LovyanGFX
- **ENCODER.md** — ISR Gray code, sequenciamiento, SAT, VPot ring
- **LEDS.md** — WS2812B NeoPixel, asignación LEDs, estados, brillo
- **SAT.md** — Sistema Auto-Test, navegación menú, integración módulos
- CLAUDE.md — Directivas vinculantes únicamente (no duplicar)

---

## Subproyectos PlatformIO

| Directorio | MCU | Rol | Compilar |
|---|---|---|---|
| `MASTER_S3-P4/P4/` | ESP32-P4 | Master MCU — USB-MIDI + RS485 bus A | `cd MASTER_S3-P4/P4 && pio run` |
| `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/` | ESP32-S3 | Extender — RS485 bus B (8 slaves) | `cd MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER && pio run` |
| `S2/S2_V1/` | ESP32-S2 | Slave — 1 canal físico completo | `cd S2/S2_V1 && pio run` |

---

## Hardware PTxx Track S2 (slave) — 📌 Ver S2/README.md

**Chip:** ESP32-S2FN4R2 — 4MB Flash, 2MB PSRAM (QSPI)

**Pinout, Display, PSRAM, NeoPixels, Motor, ADS1115:** 
→ **[Ver S2/README.md](S2/README.md)** (fuente única de verdad)

**Motor.md y FADER.md:** Documentación exhaustiva específica de subsistemas

### Fader & Motor — 📌 Ver FADER.md y MOTOR.md

**Documentación centralizada (2026-05-16):**
- **FADER.md** — ADS1115 (16-bit ISR 860 SPS), calibración bidireccional, mapping Logic↔ADC, EMA filter
- **MOTOR.md** — DRV8833 H-bridge, máquina de estados (KICK_UP…SETTLE_DOWN), guard cooldown, SAT

### RS485 — 📌 Ver RS485.md

**Documentación exhaustiva:** 
→ **[RS485.md](docs/RS485.md)** (topología, timing, paquetes, máquina estados, troubleshooting)

### WiFi / OTA — 📌 Ver WIFI.md

**Documentación exhaustiva:** 
→ **[WIFI.md](docs/WIFI.md)** (provisioning, OTA ElegantOTA, NVS, boot OTA-only mode)

---

## Arquitectura S2 — 📌 Ver MOTOR.md y FADER.md

**Toda la documentación técnica de S2 está centralizada:**
- **MOTOR.md** — Loop principal, máquina calibración, SAT, APIs Motor
- **FADER.md** — ADS1115, touch capacitivo, EMA filter, mapping Logic↔ADC

**CLAUDE.md mantiene solo directivas críticas.** Para detalles arquitectura S2: leer MOTOR.md + FADER.md.

---

## ~~Arquitectura S2 — Flujo de datos (MOVIDO A MOTOR.md)~~

### Loop principal (main.cpp)

```
loop() {
  0. ButtonManager::update()        ← botones REC/SOLO/MUTE/SELECT → flags
     - Si SAT abierto en MOTOR_CALIB + REC presionado: Motor::startCalib() (2026-05-12 19:07)
  1. SAT menu — si está abierto, return (máxima prioridad)
  2. Encoder::update()              ← ANTES de RS485, captura delta actualizado
     - Si changed: actualiza VPotLevel, setea needsVPotRedraw
  3. RS485.update() (si no suspended):
     - Si hasNewData():
       - RS485Handler::onMasterData() ← recibe faderTarget, nombre, flags
       - buildResponse() + sendResponse() ← arma y envía SlavePacket
       - Encoder::reset()             ← limpia delta acumulado
     - checkTimeout()
  4. faderADC.update()              ← lee ADC 13-bit: 0-8191
  5. FaderTouch::update()           ← detección sostenimiento
  6. Motor::setADC(faderADC.getFaderPos()) ← Motor recibe ADC actual
  7. Motor::update()                ← máquina de estado calibración/control
     - CRÍTICO: ejecuta SIEMPRE (incluso si SAT abierto)
     - SAT NO ejecuta Motor::update() (evita race condition) (2026-05-12 19:07)
     - Si FaderTouch::isTouched() → Motor::stop()
     - Si no → Motor::update()
  8. updateButtons()                ← procesa botones para display
  9. updateDisplay()                ← Display SPI3
  10. updateAllNeopixels()          ← LEDs
}
```

### Botones — 📌 Ver BUTTONS.md

**Documentación exhaustiva:** 
→ **[BUTTONS.md](docs/BUTTONS.md)** (hardware GPIO37-40, debounce 20ms, ButtonManager, mapeo MIDI notas 160-163)

### Fader ADC — 📌 Ver FADER.md

**Documentación exhaustiva:** 
→ **[FADER.md](docs/FADER.md)** (ADS1115, ISR 860 SPS, FaderTouch capacitivo, salida SlavePacket)

### Motor — 📌 Ver MOTOR.md

**Documentación exhaustiva:** 
→ **[MOTOR.md](docs/MOTOR.md)** (DRV8833, máquina calibración KICK_UP..SETTLE_DOWN, control, SAT)

### Pantalla — 📌 Ver DISPLAY.md

**Documentación exhaustiva:** 
→ **[DISPLAY.md](docs/DISPLAY.md)** (ST7789V3, sprites PSRAM, layout, LovyanGFX, SAT suspend/restore)

### Encoder — 📌 Ver ENCODER.md

**Documentación exhaustiva:** 
→ **[ENCODER.md](docs/ENCODER.md)** (ISR Gray code, sequenciamiento critical, SAT push >3s, VPot ring -7..+7)

### NeoPixels — 📌 Ver LEDS.md

**Documentación exhaustiva:**
→ **[LEDS.md](docs/LEDS.md)** (WS2812B 12 LEDs, asignación 0-3/4-10/11, estados color, brillo, SAT test)

### Ciclo de comunicación RS485 — 📌 Ver RS485.md

**Documentación exhaustiva:**
→ **[RS485.md](docs/RS485.md)** (topología, MasterPacket/SlavePacket, timing, máquina estados, CRC8, troubleshooting)

### Inicialización S2 — 📌 Ver S2/README.md

**Documentación exhaustiva:**
→ **[S2/README.md](S2/README.md)** (orden init crítico, dependencias, timing SPI/I2C/RS485)

### SAT (Sistema de Auto-Test) — 📌 Ver SAT.md

**Documentación exhaustiva:**
→ **[SAT.md](docs/SAT.md)** (navegación, menú, integración Motor/Display/Encoder/LEDs/WiFi/RS485, troubleshooting)

### Build S2
- Platform: pioarduino 55.03.37 / IDF5 — unificado con P4 (requiere P4)
- LovyanGFX 1.2.19, Adafruit NeoPixel (cambio desde NeoPixelBus)
- Orden de init obligatorio: `initDisplay()` → `initNeopixels()` → `initHardware()`
- FreeRTOS: `rs485.begin()` en setup, `rs485.startTask()` al final del setup
- `Serial.printf` para logging — `log_i`/`log_w` no son fiables en S2
- Versioning: `FW_VERSION` y `FW_BUILD_ID` inyectados via `pre_build.py`

---

## Hardware S3 (Extender)

**Chip:** ESP32-S3 | **Familia Mackie:** 0x14 | **Slaves:** 8 (IDs 1–8)

### Pinout definitivo S3

| Función | GPIO |
|---|---|
| RS485 TX | 15 |
| RS485 RX | 16 |
| RS485 EN | 1 |
| LED REC | 12 |
| BTN REC | 11 |
| LED PLAY | 10 |
| BTN PLAY | 9 |
| LED FF | 8 |
| BTN FF | 7 |
| LED STOP | 6 |
| BTN STOP | 5 |
| LED RW | 4 |
| BTN RW | 3 |

### RS485 (bus B)
- 500 kbaud, CRC8 (ver Pinout: TX=GPIO15, RX=GPIO16, EN=GPIO1)
- Controla 8 slaves S2 (IDs 1–8)
- Timing: TX_EN=30µs, TX_DONE=30µs, RESP_TIMEOUT=3000µs, GAP=300µs, POLL_CYCLE=20ms

### Transporte — Botones y LEDs
(Ver Pinout definitivo S3 para asignación GPIO de REC/PLAY/FF/STOP/RW)

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

## Hardware P4 (master)

**Placa:** GUITION JC4880P433C (ESP32-P4)

### Pinout definitivo P4

| Función | GPIO |
|---|---|
| RS485 TX | 50 |
| RS485 RX | 51 |
| RS485 EN | 52 |
| NeoTrellis SDA | 33 |
| NeoTrellis SCL | 31 |
| Touch SDA (GT911) | 7 |
| Touch SCL (GT911) | 8 |
| Display MIPI-DSI | Integrado placa |

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

**→ [Ver STATUS.md](STATUS.md)** para bugs conocidos y pendientes.

## Protocolo RS485

**→ [Ver documentación completa en STATUS.md](STATUS.md)**

RS485 es el bus serial que conecta master (P4 o S3) con slaves (S2):
- **Baudrate:** 500 kbaud, 8N1
- **Protocolo:** Binario custom, CRC8, topología star
- **Timing:** Ciclo ~20ms para 8 slaves, timeouts críticos en microsegundos
- **Bus A (P4):** 9 slaves, TX=GPIO50, RX=GPIO51, EN=GPIO52
- **Bus B (S3):** 8 slaves, TX=GPIO15, RX=GPIO16, EN=GPIO1
- **Slaves (S2):** TX=GPIO8, RX=GPIO9, EN=GPIO35

Toda la especificación de paquetes (MasterPacket, SlavePacket), máquina de estados, timing, CRC, troubleshooting y optimizaciones históricas está en STATUS.md.

Ver también: **→ [STATUS.md](STATUS.md)** para auditoría RS485 y métricas de timing.

---

## Encoder — Arquitectura y sequenciamiento (S2)

**Fuente única de verdad:** `src/hardware/encoder/Encoder.cpp` (confirmado 2026-04-27 14:00)
- ISR basada en cambio de flanco (CHANGE) en GPIO12 y GPIO13
- Debounce 3ms en ISR (válido)
- Dirección: A LOW + B HIGH = -1 (izquierda), A LOW + B LOW = +1 (derecha)
- Sin duplicados en SAT ni main.cpp

**Usuarios correctos:**
- `RS485Handler::buildResponse()` → captura delta para enviar al master
- `main.cpp` → calcula nivel VPot (-7..+7) y redibuja Display

**RESUELTO (2026-04-28 15:30) — Sequenciamiento corregido:**

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

Resultado: RS485 y Display ahora usan el mismo delta, VPot ring responde correctamente en Logic.

---

## Mackie MCU — comportamiento Logic

- Logic solo envía automation state global via notas MIDI 74–78
- `GoOffline` SysEx: `F0 00 00 66 14 0F F7`
- En desconexión Logic envía `AllFadersToMinimum` SysEx + PitchWheel -8192 en todos los canales antes del `GoOffline`
- SELECT es latch-style
- MIDI fader max para recorrido físico completo: **14848** (no 16383)
- `track_idx = currentOffset / 7`, `char_pos = currentOffset % 7` — parser SysEx `0x12`

### Rango de Faders — Mapeo Logic → Hardware (2026-05-13 00:30)

**Arquitectura de mapeo (S3 centraliza responsabilidad):**
```
Logic Pro
  │ PitchBend 0-14848
  ↓
S3 MidiProcessor::processPitchBend()
  │ Recibe bendValue 0-14848
  ↓
S3 RS485Master::setFaderTarget()
  │ Si slave calibrado: mapea 0-14848 → rango real S2 (ej: 25-26468)
  │ Si no calibrado: mapea 0-14848 → rango teórico (0-27000)
  ↓
S3 envía MasterPacket.faderTarget → valor YA MAPEADO
  ↓
S2 recibe faderTarget (valor final)
  ↓
S2 Motor::setTarget(target) → usa DIRECTAMENTE (sin map(), O(1))
  ↓
Motor posiciona a target
```

**Calibración — rango se envía una sola vez al boot:**
1. S3 ordena FLAG_CALIB a S2
2. S2 calibra → obtiene min/max
3. S2 envía 2 paquetes:
   - Paquete 1: faderPos=min, flags=CALIB_DONE|CALIB_SENDING|CALIB_IS_MIN
   - Paquete 2: faderPos=max, flags=CALIB_DONE|CALIB_SENDING (sin IS_MIN)
4. S3 almacena calibratedMin/Max para ese S2
5. Desde entonces: S3 usa rango real para mapeos

**Razón arquitectónica:** S2 es single-core. Motor::setTarget() debe ser O(1) sin cálculos.

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


## Repositorio (2026-05-12 17:47)

**GitHub:** `juliannof/iMakie`  
**Local:** `/Users/julianno/Documents/PlatformIO/Projects/AITEC/` (reorganizado como carpeta unificada)

**Compilar cada subproyecto:**
```bash
# Master P4
cd MASTER_S3-P4/P4 && pio run

# Extender S3
cd MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER && pio run

# Slave S2
cd S2/S2_V1 && pio run
```

---

## SESION (2026-05-10 22:00-22:05) — S2 Test Mode ADC Real-Time

**Objetivo:** Fix SAT Test Mode pantalla — Motor::getRawADC() siempre devolvía valor de entrada, nunca se actualizaba.

**Root cause:** Motor::setADC() rechazaba deltas > 200 (SPIKE_GUARD) cuando SAT abría.

**Cambios implementados:**
1. config.h: ADC_SPIKE_GUARD 200 → 500 (línea 104)
2. main.cpp: Motor::setADC() movido antes SAT check (línea 283) — ejecuta cada frame incluso en Test Mode
3. SatMenu.cpp _tickMotorTest(): Lee directo faderADC.getFaderPos() en lugar Motor::getRawADC() (línea 1088)

**Estado actual:** 
- ✅ ADC obtiene valor correcto de faderADC en tiempo real
- ❌ **Pantalla no se redibuja en tiempo real** — MOTOR_TEST no está en lista `live` en SatMenu::update() línea 113-119
  - Solución pendiente: agregar `_scr == Scr::MOTOR_TEST` a lista live
  - Esto hará que _render() se ejecute cada frame

**Por hacer próxima sesión:**
- Agregar MOTOR_TEST a lista `live` en SatMenu.cpp líneas 113-119
- Verificar que pantalla Test Mode redibuja cada frame

---

## SESION (2026-05-04)

- STATUS.md reorganizado: S2, S3, P4, Cross-system con estructura MAYÚSCULAS + NEGRITA
- Subsecciones Bugs/Pendientes/Detalles técnicos en cada componente
- 7 bugs críticos documentados en S2 (RS485 pérdida, Display brillo, Botones lentos, Fader no funciona, FaderTouch con plástico, Motor no funciona, Encoder solo SAT)
- WiFi/OTA: ArduinoOTA muerto → ElegantOTA funciona
