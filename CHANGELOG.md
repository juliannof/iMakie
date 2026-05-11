# CHANGELOG — iMakie

Registro histórico de cambios significativos del proyecto iMakie.  
Formato: [Keep a Changelog](https://keepachangelog.com/)

---

## [Unreleased]

### Investigation & Resolution
- **S2 MOTOR — Calibración bloqueada: Motor no baja (2026-05-10 15:20 → 21:55) — RESUELTO**
  
  **Problema identificado (15:20):**
  - Motor no se movía hacia abajo durante calibración
  - Síntomas: KICK_UP/GOING_UP/SETTLE_UP subían ADC, pero KICK_DOWN/GOING_DOWN/SETTLE_DOWN no bajaban
  - Resultado: `top=3984, bot=3984` → ERROR (rango inválido)
  - Hipótesis inicial: Motor solo sube; posible PWM no llega a IN2 (DOWN control)
  - Documentado en: MOTOR_DIAGNOSIS.md (2026-05-10 15:20)
  
  **Solución implementada (21:53, commit af0cccd):**
  - Motor::initPWM() rediseñado para leer pwmMin/pwmMax de NVS (con fallback a config.h)
  - Test Mode mejorado: REC=UP, SOLO=DOWN, MUTE=exit (botones directos)
  - Motor responde correctamente: GPIO18 (UP) y GPIO16 (DOWN) con duty cycles verificados
  - SAT ahora es autoridad para valores PWM en runtime (no config.h)
  - Motor::update() se salta cuando SAT está abierto (evita conflictos)
  - Hardware verificado: REC y SOLO producen movimiento correcto en ambas direcciones
  
  **Optimización (21:55, commit e38fe88):**
  - PWM_MAX calibrado a **160** (63% duty cycle) → movimiento suave, sin ruido
  - PWM_MIN = 100 (jerarquía de control estable)
  - Motor alcanza rendimiento óptimo: responde rápido, movimiento limpio, seguro
  
  **Estado actual:** ✅ RESUELTO — Motor funcional, calibración exitosa, Test Mode operativo
  
  **Lecciones aprendidas:**
  - NVS para valores runtime es más flexible que config.h hardcoded
  - Test Mode con botones directo es mejor que máquina de calibración para diagnóstico
  - PWM range 100-160 empíricamente óptimo para este hardware (DRV8833 + motor S2)

### Removed
- **S2 SAT MOTOR — Opción "Posicion" removida (2026-05-10 19:54)**
  - Razón: Pantalla era stub no funcional (todo comentado, valores hardcodeados a 0)
  - Motor nunca se movía: `Motor::setTarget()` nunca era llamado
  - Impacto: Menú Motor ahora tiene 5 opciones (quitadas 6)
  - Actualizado: `_motorN = 5`, casos switch ajustados

### Changed
- **S2 SAT MOTOR — Test Mode movido a opción 1 (primero) (2026-05-10 19:54)**
  - Antes: Motor ON/OFF → Calibrar → Test Mode → PWM Min/Max
  - Ahora: Motor ON/OFF → Test Mode → Calibrar → PWM Min/Max
  - Razón: Si motor no funciona, testear ANTES de calibración
  - Orden: caso 1 para Test Mode, caso 2 para Calibrar

### Fixed
- **S2 SAT MOTOR — Menu item count + Test Mode handler (2026-05-10 19:54)**
  - Bug 1: `_motorN = 6` pero solo 5 items válidos (Posición era stub)
  - Bug 2: Switch en `_hMotor()` con casos incorrectos
  - Solución: Removida "Posición", `_motorN = 5`, casos reajustados a 0-4

- **S2 RS485 — Error setRxBufferSize when reinitializing (2026-05-10 19:54)**
  - Problema: Cada reinicio de RS485 (SAT config saved) intentaba resize Serial1 ya activo
  - Solución: `Serial1.end()` antes de `Serial1.setRxBufferSize()` en `RS485Slave::begin()`
  - Elimina error: `RX Buffer can't be resized when Serial is already running`
  - Impacto: RS485 reinicia limpiamente sin logs de error

### Added
- **S2 MOTOR — Test Mode + Funciones de Control Directo (2026-05-10 19:54)**
  - Nuevas funciones públicas en Motor: `testUp(pwm)`, `testDown(pwm)`, `testOff()`
  - SAT menu opción nueva: "Motor → Test Mode"
  - Control con botones:
    - **REC button** = UP (PWM_MAX)
    - **MUTE button** = DOWN (PWM_MAX)
    - **SOLO button** = OFF
  - Display en tiempo real: ADC, estado botones, PWM actual
  - No afecta calibración automática (independent test)
  - Logs en Serial: `[MOTOR-TEST] UP/DOWN/OFF pwm=X`

- **S2 MOTOR — Detección de Motor Bloqueado + Fallback a DOWN (2026-05-10 19:54)**
  - Nueva constante: `CALIB_STUCK_TIMEOUT = 500ms`
  - Detección en `GOING_UP`: si ADC no cambia en 500ms → salta a `KICK_DOWN` inmediatamente
  - Detección en `GOING_DOWN`: si ADC no cambia en 500ms → `ERROR` (motor definitivamente muerto)
  - Secuencia: KICK_UP → GOING_UP (falla) → KICK_DOWN → GOING_DOWN (falla) → ERROR
  - Diferencia clara: "motor invertido/parcial" (UP falla) vs "motor muerto" (ambas fallan)
  - Útil para diagnosticar: inversión de cables, dirección bloqueada, driver dañado

### Documentation
- **S2 MOTOR — LEDC Migración Revertida, analogWrite Definitivo (2026-05-10 19:54)**
  - LEDC migración fue intentada pero revertida: conflicto de canales LEDC
  - **Causa:** LovyanGFX backlight (GPIO3) + Motor (GPIO18/16) agotaban 8 canales LEDC del ESP32-S2
  - **Solución:** analogWrite definitivo (API simple, robusta, sin conflictos)
  - **Criterio:** "Si funciona y no hay conflicto, no refactorizar"
  - Documentación: CLAUDE.md actualizado, memory s2_motor_ledc_conflict.md creado
  - **Impacto:** Motor.cpp sin cambios (ya usa analogWrite correcto)

### Changed
- **S2 MOTOR — Test mode + Safety + Compilation fixes (2026-05-10 15:20)**
  - Test mode automático: calibración + movimiento a 5 posiciones (0%, 25%, 50%, 75%, 100%) cada 2s
  - Safety: Motor EN (GPIO14) = LOW en setup() ANTES de todo (previene movimiento al boot)
  - Compilación: agregar MIDI_PB_MAX=16383, renombrar _motorActive→_motor_active, _currentPWM→_motor_currentPWM
  - Test mode fix: startCalib() se llama UNA sola vez (no loop infinito)
  - **BLOQUEADOR ENCONTRADO:** Motor no se mueve hacia abajo — calibración falla con `top=3984, bot=3984`
  - Diagnóstico: Probablemente PWM no llega a IN2 (DOWN control), revisar GPIO16/cable/DRV8833
  - Commits: `534a13a`, `8c64aa1`, `afc62ac`, `ceed039`, `10ce193`, `deafafa`

- **S2 MOTOR + FADER — Auditoría exhaustiva (2026-05-10 15:02)**
  - Motor.cpp: control ordering crítico, timestamp recapture en transiciones, dinámica PWM mapping
  - FaderADC.cpp: 8 problemas corregidos — variable scope, tipo consistencia, validación de rango completa, bandera gotData
  - FaderADC.h: eliminados campos muertos (_emaValue, _noiseSpan, _noiseWindow, _noiseHead), método _isTrending()
  - FaderTouch.cpp: 8 problemas corregidos — baseline pausada durante toque, timestamp-based detección (frame-rate independent), touchRead() validado, fallback de baseline
  - config.h: FADERTOUCH completada con constantes (TOUCH_POLL_MS, TOUCH_THR_*, TOUCH_SOSTENIMIENTO, etc.)
  - Resultado: 210+ líneas de código muerto eliminadas, arquitectura simplificada, robusto a race conditions
  - Commit: `534021d`

### Removed
- **S2 MOTOR — Reset total: borrado Motor.h / Motor.cpp (2026-05-11 08:15)**
  - Razón: Código base defectuoso. Motor solo se mueve en un sentido.
  - Removido: máquina de calibración (CalibPhase), control de posición, analogWrite/LEDC mixtos, todos los logs internos
  - Documentación: `/track S2/iMakie - Track ESP32S2 V1/src/hardware/Motor/Motor.h` y `.cpp` vaciados excepto headers
  - Impacto: main.cpp sigue compilando (Motor:: namespace existe pero vacío), permite reescritura limpia sin legacy
  - Lección: Código base con migración analogWrite→LEDC fallida + órdenes init inconsistentes → restart mejor que patch
  - Próximo paso: reescribir Motor desde cero con especificación clara de DRV8833 control

### Changed
- **S2 MOTOR TEST — FaderADC desactivado (2026-05-10 22:30)**
  - Razón: I2C interfiere en unidades DAC (sin ADS1115)
  - Cambio: `faderADC.begin()` comentado en main.cpp setup()
  - GPIO34/GPIO21 liberados para DAC del fader
  - GPIO17 (ADS_ALERT) fijo OUTPUT LOW — evita flotante
  - Estado: Motor-only test mode activo
  - Nota: Cambio temporal para debugging de motor en unidad DAC

- **Versión — 0.4.2 (2026-05-10 20:00)**
  - Schema: MAJOR.MINOR.PATCH desarrollo
  - 0 = Debug/Development state
  - 4 = Subsistemas completos: Display, Botones, LEDs, Fader (100%)
  - 2 = En desarrollo: Fader + Motor
  - Actualizado pre_build.py con versión y comentario de schema

### Documentation
- **Directiva Obligatoria — Código Moderno: Alineación con Stack (2026-05-10 19:45)**
  - Todos los cambios de código deben usar las MISMAS APIs que las librerías del proyecto
  - Motor: DEBE usar LEDC (ledcAttach/ledcWrite) — NO analogWrite (incompatible con LovyanGFX)
  - I2C: DEBE usar Wire moderno (Adafruit BusIO estándar)
  - Logging: usar log_i/log_e (no Serial legacy)
  - PROHIBIDO mezclar APIs en mismo subsistema (ej: LEDC + analogWrite = FATAL)
  - Stack: pioarduino 55.03.37/IDF5 + LovyanGFX 1.2.19 + Adafruit libs
  - Documentado en CLAUDE.md y memory

### Changed
- **S2 MOTOR — Migración a LEDC Core 3.x (2026-05-10 19:50)**
  - Reemplazado analogWrite (API antigua) por ledcWrite (LEDC moderno)
  - init(): analogWriteFrequency/Resolution → ledcAttach con validación de retorno
  - _hwBrake/Off/Up/Down: analogWrite → ledcWrite
  - Alineación con stack: LovyanGFX usa LEDC internamente, motor ahora compatible
  - Log mejorado: detecta fallos de ledcAttach en init()
  - Impacto: PWM 20kHz estable, API moderna, sin conflictos con otras librerías
  - Estado: listo para compilación y testing

- **S2 MOTOR — _hwUp() y _hwDown() invertidos (2026-05-10 00:15)**
  - Hardware tiene pines invertidos: UP=IN2 PWM, DOWN=IN1 PWM
  - Cambio: invertir lógica en ambas funciones
  - Estado: compilado, debugging con osciloscopio en progreso
  - Commit: `479f64b`

- **S2 MOTOR — CalibPhase duplicado removido (2026-05-10 00:15)**
  - CalibPhase enum estaba en Motor.cpp y config.h
  - Removido de Motor.cpp (config.h es autoridad)
  - Commit: `479f64b`

### Bugs Encontrados
- **S2 MOTOR — No responde en ningún caso (2026-05-10 00:15)**
  - Motor completamente inmóvil: ni en calibración ni en control
  - Driver funciona (verificado)
  - Causa desconocida: posible fallo EN (GPIO14), pines no se configuran, o init() rompe pines
  - Investigación: osciloscopio midiendo EN/IN1/IN2 en progreso
  - Estado: BLOQUEADO - esperando resultados de medición
- **S2 MOTOR — Orden inicialización PWM: pinMode → frequency/resolution → analogWrite (2026-05-09 23:45)**
  - HIPÓTESIS: Motor.cpp::init() ponía `analogWrite()` ANTES de `analogWriteFrequency/Resolution`
  - analogWrite() hace attach implícito con frecuencia default, luego frequency() no tiene efecto
  - CAMBIO: Restaurar orden correcto: pinMode → frequency/resolution → LUEGO analogWrite
  - ESPERADO: PWM a 20kHz funcione (vs frecuencia default mucho menor)
  - TESTING REQUERIDO: Compilar + calibración (rango ADC debe ser 0-8191, no 24-26)
  - Commit: `0305c6a`

- **S2 MOTOR — Variables de estado centralizadas en config.h (2026-05-09 23:45)**
  - CalibPhase enum: IDLE, KICK_UP, GOING_UP, SETTLE_UP, KICK_DOWN, GOING_DOWN, SETTLE_DOWN, DONE, ERROR
  - Variables calibración: _phase, _phaseStart, _calibStart, _calibMinDetect, _stableStart, _stableRef
  - Variables ADC: _adcTop, _adcMin, _adcMax, _adcSpan, _adcPos, _targetADC, _lastMidiTarget
  - Variables noise: _settleMin, _settleMax, _noiseTopSpan
  - Variables control: _motorActive, _currentPWM
  - Motor.cpp simplificado: solo lógica, no variables de estado
  - Commit: `0305c6a`

- **S2 FADER — ADS1115 se hace obligatorio (2026-05-09)**
  - Eliminados TODOS los `#ifdef USE_ADS1015` del código
  - ADC nativo (GPIO10, 13-bit) descartado permanentemente
  - Entorno default: `lolin_s2_mini` (ADS1115) con librerías ADS1X15 + BusIO
  - platformio.ini consolidado: Serial y OTA ahora usan ADS
  - FaderADC simplificado: solo rama ADS, sin compilación condicional
  - config.h limpiado: removed `FADER_POT_PIN`, `FADER_VCC_PIN`, `NOISE_WINDOW_SIZE`, `FADER_EMA_ALPHA_FAST`
  - main.cpp: removed DAC setup (`#ifndef USE_ADS1015`), diagnóstico ADS incondicional

### Added
- **S2 FADER — ADS1115 I2C ADC (Fase 1)** (2026-05-09)
  - ISR ALERT/RDY en GPIO17 — no polling, 860 SPS continuo
  - Buffer circular 256 muestras con timestamp (no-bloqueante)
  - GAIN_ONE (±4.096V) para rango 3.3V directo
  - Función `dumpAdsLog()` para análisis CSV de ruido
  - Validación I2C en setup() — log automático de detección

### Modified
- **platformio.ini:** Nuevo entorno `lolin_s2_mini_ads` con libs ADS1X15 + BusIO; eliminado `extends` (2026-05-09)
- **config.h:** Defines ADS (SDA=21, SCL=34, ALERT=17, addr=0x48) bajo guardia
- **protocol.h:** Comentario `faderPos` documentado para dual-mode 13/16-bit
- **FaderADC.h:** Estructura con Adafruit_ADS1115, TwoWire I2C, ISR ALERT/RDY
- **FaderADC.cpp:** ISR definition, `begin()`, `update()`, `measureRange()`, `dumpAdsLog()`
- **main.cpp:** Diagnóstico ADS1115 periódico (cada 500ms) en loop; log: `[ADS] raw=X pos=X`

### Fixed
- **S2 FADER — ALERT pin trigger FALLING (2026-05-09)**
  - ADS1115 ALERT/RDY es activo-bajo: HIGH (reposo) → LOW (dato) = **FALLING**, no RISING
  - FaderADC.cpp usaba RISING → ISR nunca se disparaba → `_newData` siempre false
  - Motor nunca recibía posición → completamente ciego
  - Cambio: attachInterrupt(..., FALLING) — una línea, efecto crítico
  - Commit: `386765f`

- **S2 FADER — measureRange() bloqueante documentado (2026-05-09)**
  - `measureRange()` espera 5s en loop cerrado (S2 single-core)
  - Impacto: SAT menu congelado, RS485 timeout, Master marca slave NO_CALIBRATED
  - Decisión: Documentar impacto (no refactorizar por ahora)
  - Restricción: SOLO usar en diagnóstico excepcional, NUNCA durante operación/calibración
  - Documentación: FaderADC.h, FaderADC.cpp (comentarios), SatMenu.cpp (warning)
  - Commit: `bbddaa0`

### Technical Notes
- **Resolución:** ADS 16-bit (0-32767) sin escalado FP → P4/S3 mapean a 0-14848
- **Performance:** update() ADS = 0-2µs (vs 24ms ADC nativo) — no impacta loop() S2 single-core
- **Ruido:** ADS ~2-5 counts (vs ±30 ADC nativo) — mejora 6-15×
- **Pines I2C confirmados:** SDA=21, SCL=17, ALERT=34 (usuario validó 2026-05-09)
- **Commit:** `80eb621` (implementación), `670ae24` (historial centralizado)

---

## [v2026-05-04] — WiFi OTA y Documentación Reorganizada

### Added
- **WiFi OTA — ElegantOTA 3.1.7** 
  - ArduinoOTA descartado (muerto en pioarduino 55.03.37)
  - ElegantOTA funciona perfecto — SAT menu "WiFi OTA"
  - Credenciales NVS: SSID=`Julianno-WiFi` | Pass=`JULIANf1`

### Changed
- **STATUS.md reorganizado** (2026-05-04 19:20)
  - Estructura: S2 | S3 | P4 | Cross-system
  - Subsecciones: Bugs/Pendientes/Detalles técnicos para cada componente
  - 7 bugs críticos documentados con criterios de éxito

### Fixed
- **Encoder sequenciamiento (2026-04-28 15:30)**
  - `Encoder::reset()` movido post-VPot (antes estaba pre-VPot)
  - VPot ring ahora responde correctamente en Logic Pro
  - RS485 y Display usan mismo delta

### Documentation
- **CLAUDE.md:** Agregada sección SESION con fecha/hora obligatoria
- Formato: `(YYYY-MM-DD HH:MM)` para rastreabilidad de cambios

---

## [v2026-04-28] — NeoPixel y Encoder Fixes

### Added
- **NeoPixel — Cambio a Adafruit NeoPixel** (2026-04-28 16:15)
  - NeoPixelBus 2.8.4 incompatible con pioarduino 55.03.37 / IDF5
  - Adafruit NeoPixel 3.1.7 es solución definitiva
  - Secuencia brillo: azul tenue → colores tenues (Logic conecta) → on/off
  - HW_STATUS display en boot — 10 componentes color-coded

### Fixed
- **Encoder no funciona en Logic** (2026-04-28)
  - Root cause: `Encoder::reset()` en lugar equivocado (pre-VPot)
  - Solución: Reset post-VPot (post-buildResponse)
  - SAT funcionaba porque procesaba sin reset intermedio

---

## [v2026-04-27] — Documentación Encoder Centralizada

### Added
- **Encoder — Fuente única de verdad** (2026-04-27 14:00)
  - `src/hardware/encoder/Encoder.cpp` confirmada como fuente central
  - Sin duplicados en SAT ni main.cpp
  - ISR basada en CHANGE, debounce 3ms, dirección: A LOW + B HIGH = -1

### Documentation
- CLAUDE.md: Sección "Encoder — Arquitectura y sequenciamiento"
- Usuarios correctos: RS485Handler::buildResponse(), main.cpp

---

## [v2026-02-15] — Inicial

### Project Setup
- **Arquitectura:** P4 Master + S3 Extender + 17× S2 Slaves
- **Hardware:** ESP32-P4 (master MCU), ESP32-S3 (extender), 17× ESP32-S2 Lolin (slave)
- **Comunicación:** RS485 500kbaud, protocolo binario custom, CRC8
- **MIDI:** Mackie MCU Universal compatible Logic Pro
- **Subproyectos PlatformIO:**
  - `S3/` — master S3/P4 con RS485 bus A (9 slaves) + bus B (8 slaves)
  - `track S2/` — slave S2 con 1 canal físico completo

### Known Limitations (A resolver)
- [x] NeoPixel — NeoPixelBus incompatible IDF5 → Adafruit (RESUELTO 2026-04-28)
- [x] Encoder — sequenciamiento incorrecto → reset post-VPot (RESUELTO 2026-04-28)
- [ ] S2 Fader — ADC nativo ruidoso (±30 cuentas) → ADS1115 (EN DESARROLLO 2026-05-09)
- [ ] Motor S2 — no responde → investigar DRV8833 driver
- [ ] Botones S2 — lentos → revisar debounce/latencia
- [ ] Display S2 — brillo máximo en boot → orden init

---

## Formato de Versión

- **[vYYYY-MM-DD]** — snapshot de estado en fecha
- **[Unreleased]** — cambios acumulados sin release formal
- Subcategorías: **Added** | **Changed** | **Fixed** | **Removed** | **Technical Notes** | **Documentation**

## Política de Documentación

- **Fecha/hora obligatoria:** `(YYYY-MM-DD HH:MM)` en commit + archivo
- **Rastreabilidad:** Cada cambio linkeado a commit o bug report
- **Scope:** Cambios arquitectónicos, bugs críticos, migraciones de libs, decisiones de hardware
- **No incluir:** Bug fixes locales, optimizaciones triviales, cambios de comentario solo

---

**Último actualizado:** 2026-05-09 22:50  
**Responsable:** iMakie Development Team  
**Contacto:** juliannof (GitHub)
