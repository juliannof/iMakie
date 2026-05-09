# CHANGELOG — iMakie

Registro histórico de cambios significativos del proyecto iMakie.  
Formato: [Keep a Changelog](https://keepachangelog.com/)

---

## [Unreleased]

### Changed
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
