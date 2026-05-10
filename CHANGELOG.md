# CHANGELOG — iMakie

Registro histórico de cambios significativos del proyecto iMakie.  
Formato: [Keep a Changelog](https://keepachangelog.com/)

---

## [Unreleased]

### Changed
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
