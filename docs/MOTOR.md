# MOTOR — Control DRV8833 y Calibración (S2 Slave)

Documentación exhaustiva del subsistema de motor fader. Incluye hardware, máquina de calibración, control de posición y diagnóstico.

**Responsable:** iMakie Development Team  
**Última actualización:** 2026-05-16  
**Estado:** En producción (17 faders activos)

---

## 1. HARDWARE MOTOR

### 1.1 DRV8833 H-Bridge

| Parámetro | Valor |
|-----------|-------|
| **Chip** | DRV8833 Dual H-Bridge |
| **EN (nSLEEP)** | GPIO14 |
| **IN1** | GPIO18 |
| **IN2** | GPIO16 |
| **Control** | `analogWrite()` — NO LEDC |
| **Frecuencia PWM** | 5kHz (analogWrite predeterminado) |
| **Rango PWM** | 0-255 (8-bit) |
| **PWM operacional** | 100-160 (calibrado) |

### 1.2 Sensor de Posición

**ADS1115 I2C ADC (16-bit, ±4.096V)**
- Rango capturado: 24–26476 (raw)
- ISR-driven, 860 SPS
- Latencia: 0-2µs (no polling)
- Mejora: 6-15× vs ADC nativo

Ver **FADER.md** para especificación completa de ADS1115.

### 1.3 Órdenes de Inicialización CRÍTICO

```c
setup() {
  1. Motor::init()           ← ⚠️ ANTES de Serial.begin() (silencia motor)
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

**¿Por qué `Motor::init()` ANTES de `Serial.begin()`?**
- EN (GPIO14) debe estar LOW inmediatamente → evita movimiento involuntario al boot
- Motor::init() ejecuta `digitalWrite(MOTOR_EN, LOW)` sin debug output
- Si Serial activo, hay mucho output → riesgo de timing issues

---

## 2. CONTROL DE MOTOR

### 2.1 APIs Críticas

```cpp
// Inicialización (setup)
Motor::init()              // Silencia motor (EN → LOW)
Motor::begin()             // Habilita control (post-setup)

// Control en loop
Motor::setADC(uint16_t pos)      // Posición actual fader (ADS1115)
Motor::setTarget(uint16_t target) // Posición objetivo (Logic)
Motor::update()                   // Máquina de estado (ejecutar SIEMPRE)

// Diagnóstico
Motor::getCalibState()     // Estado calibración (para SAT)
Motor::getRawADC()         // Lectura ADC actual
uint16_t Motor::getCalibMin() / getCalibMax() // Rango calibrado
```

### 2.2 Control de Posición (Post-calibración)

```cpp
Motor::setTarget(uint16_t target) {
    // target: 0-14848 (rango Logic)
    // _adcMin/_adcMax: valores calibrados
    
    // Mapear Logic range → ADC range
    uint16_t targetADC = _adcMin + (target * (_adcMax - _adcMin) / 14848);
    
    // Comparar con posición actual
    int16_t error = targetADC - _adcPos;  // -27000..+27000
    
    // Dead zone: si |error| < 50 → apagar motor
    if (abs(error) < DEAD_ZONE) {
        Motor::off();
        return;
    }
    
    // Dirección: arriba o abajo
    if (error > 0) {
        Motor::up(PWM_MIN);   // Movimiento suave
    } else {
        Motor::down(PWM_MIN);
    }
}
```

### 2.3 Parámetros de Control (config.h)

```cpp
// Motor — control de posición
static constexpr uint8_t  PWM_MIN                  = 100;
static constexpr uint8_t  PWM_MAX                  = 160;

// Motor — dead zone
static constexpr uint16_t DEAD_ZONE                = 50;    // error < esto → apagar motor

// Motor — spike guard (rechaza cambios > este valor)
static constexpr uint16_t ADC_SPIKE_GUARD          = 500;   // cuentas entre lecturas

// Motor — EMA filter
static constexpr float    FADER_EMA_ALPHA_FAST     = 0.20f; // 75% histórico, 25% nuevo
```

---

## 3. CALIBRACIÓN (Máquina de Estados No-Bloqueante)

### 3.1 Objetivo

Encontrar rango físico real del fader (min ADC, max ADC) midiendo movimiento y ruido sin bloqueos.

**Duración típica:** 3-5 segundos  
**Ejecución:** No-bloqueante (integrada en loop principal)

### 3.2 Diagrama de Estados

```
                ┌─────────────────────────────────┐
                │  IDLE (espera FLAG_CALIB)       │
                └────────────┬────────────────────┘
                             │
                             ▼
                ┌─────────────────────────────────┐
                │  KICK_UP (PWM=175, 500ms)       │  ← Fuerza movimiento inicial
                │  Objetivo: alcanzar ADC > 26000 │
                └────────────┬────────────────────┘
                             │
                             ▼
                ┌─────────────────────────────────┐
                │  GOING_UP (PWM=150 u 175)       │  ← Refinamiento
                │  Sigue subiendo hasta estable   │
                │  Detección: delta < 100/frame   │
                └────────────┬────────────────────┘
                             │
                             ▼
                ┌─────────────────────────────────┐
                │  SETTLE_UP (PWM=0, 200ms)       │  ← Mide ruido tope
                │  Motor apagado, capta ruido     │
                │  Resultado: _adcTop ± _noise_top│
                └────────────┬────────────────────┘
                             │
                             ▼
                ┌─────────────────────────────────┐
                │  KICK_DOWN (PWM=175, 500ms)     │  ← Repite hacia abajo
                │  Objetivo: alcanzar ADC < 100   │
                └────────────┬────────────────────┘
                             │
                             ▼
                ┌─────────────────────────────────┐
                │  GOING_DOWN (PWM=150 u 175)     │
                │  Sigue bajando hasta estable    │
                └────────────┬────────────────────┘
                             │
                             ▼
                ┌─────────────────────────────────┐
                │  SETTLE_DOWN (PWM=0, 200ms)     │  ← Mide ruido fondo
                │  Resultado: _adcBot ± _noise_bot│
                └────────────┬────────────────────┘
                             │
                             ▼
                ┌─────────────────────────────────┐
                │  VALIDATE & DONE                │
                │  MIN = bot + margin             │
                │  MAX = top - margin             │
                │  span = MAX - MIN               │
                └─────────────────────────────────┘
```

### 3.3 Inicio de Calibración

**Boot automático (S3 ordena FLAG_CALIB vía RS485):**
```
S3 envía MasterPacket con FLAG_CALIB
  ↓
S2 RS485Handler::onMasterData() detecta FLAG_CALIB
  ↓
Motor::startCalib() → transición IDLE → KICK_UP
```

**Manual (SAT menu):**
```
Usuario: Encoder push >3s → SAT menu
         Motor → Calibración
         → Motor::startCalib()
```

### 3.4 Guard Cooldown (2026-05-16)

```cpp
Motor::startCalib() {
    // Guard: no reiniciar si completó hace <2s
    if (millis() - _motor_lastCalibDone < CALIB_COOLDOWN_MS) {
        log_w("[CALIB] Enfriamiento activo, rechazando FLAG_CALIB");
        return;
    }
    
    // Inicializar máquina
    _motor_phase = CalibPhase::KICK_UP;
    _motor_calibStart = millis();
    _motor_phaseStart = millis();
    log_i("[CALIB] Iniciando KICK_UP");
}
```

**config.h:**
```cpp
static constexpr uint32_t CALIB_COOLDOWN_MS        = 2000;  // ms espera mínima (2026-05-16)
static uint32_t   _motor_lastCalibDone  = 0;    // timestamp último finish
```

**Razón:** S3 continuaba enviando FLAG_CALIB después de calibración exitosa → bucle infinito. Cooldown previene reinicios involuntarios.

### 3.5 Validación Post-Calibración

```cpp
// Después de SETTLE_DOWN, validar rango capturado

uint16_t min_margin = 100;
uint16_t max_margin = 100;

_calibratedFaderMin = _motor_adcBot + min_margin;
_calibratedFaderMax = _motor_adcTop - max_margin;

uint16_t span = _calibratedFaderMax - _calibratedFaderMin;

if (span < 1000) {
    // Rango muy pequeño → error
    _motor_phase = CalibPhase::ERROR;
    log_e("[CALIB] FAIL: span=%d (< 1000)", span);
    return;
}

log_i("[CALIB] ✓ DONE: min=%d max=%d span=%d", 
      _calibratedFaderMin, _calibratedFaderMax, span);

_motor_phase = CalibPhase::DONE;
_motor_lastCalibDone = millis();  // Registrar timestamp para cooldown
```

### 3.6 Parámetros de Calibración (config.h)

```cpp
// Motor — calibración (constantes)
static constexpr uint16_t ADC_STABILITY_THRESHOLD  = 100;     // cambio máximo para "estable"
static constexpr uint32_t CALIB_STABLE_TIME        = 500;     // ms para confirmar estable
static constexpr uint32_t CALIB_SETTLE_MS          = 200;     // ms para medir ruido
static constexpr uint32_t CALIB_MIN_TRAVEL_MS      = 300;     // ms mínimo de viaje
static constexpr uint32_t CALIB_TIMEOUT            = 6000;    // ms timeout total
static constexpr uint32_t CALIB_STUCK_TIMEOUT      = 1000;    // ms sin movimiento = atascado
static constexpr uint32_t CALIB_COOLDOWN_MS        = 2000;    // ms espera antes de reintentar
static constexpr uint8_t  PWM_SLEW                 = 5;       // cambio PWM máximo/tick
```

---

## 4. SAT (Sistema de Auto-Test) — Motor

Acceder: Encoder push >3 segundos en display normal

### 4.1 Opciones SAT

- **Motor Off** — Desactiva motor completamente
- **Motor On** — Activa motor
- **Motor Calibrar** — Inicia calibración automática
  - Se ejecuta en loop principal (Motor::update() SIEMPRE)
  - SAT solo dibuja estado (no controla)
  - Presionar REC: reinicia calibración si falla
- **Motor Drive** — Test PWM manual (slider 0-255)
- **Brightness** — Test backlight
- **RS485 On/Off** — Simula desconexión (debug)
- **LEDs Test** — Secuencia RGB por índice
- **WiFi OTA** — Carga firmware vía WiFi
- **Reboot** — Reinicia

### 4.2 Motor Calibración en SAT (2026-05-12 19:07)

**Arquitectura:**
- `main.cpp` ejecuta `Motor::update()` cada frame (incluso con SAT abierto)
- SAT **NO** ejecuta Motor::update() (evita race conditions)
- Motor::setADC() actualizado siempre en main.cpp
- SAT solo dibuja `Motor::getCalibState()`

**Ventaja:** Garantiza que calibración en SAT = calibración en loop principal.

---

## 5. DIAGNÓSTICO Y DEBUGGING

### 5.1 Expected Logs — Calibración Exitosa

```
[CALIB] Iniciando KICK_UP
[CALIB] KICK_UP → GOING_UP (ADC=26200)
[CALIB] GOING_UP → SETTLE_UP (estable en 26400)
[CALIB] SETTLE_UP: ruido_top=±8 cuentas
[CALIB] SETTLE_UP → KICK_DOWN
[CALIB] KICK_DOWN → GOING_DOWN (ADC=500)
[CALIB] GOING_DOWN → SETTLE_DOWN (estable en 100)
[CALIB] SETTLE_DOWN: ruido_bot=±5 cuentas
[CALIB] ✓ DONE: min=200 max=26300 span=26100
```

### 5.2 Expected Logs — Fallo Calibración

```
[CALIB] Iniciando KICK_UP
[CALIB] KICK_UP timeout (motor no se mueve)
[CALIB] ✗ ERROR: motor atascado o desconectado
```

### 5.3 Checklist Troubleshooting

| Síntoma | Causa Probable | Verificación |
|---------|----------------|--------------|
| Motor inmóvil | EN (GPIO14) no LOW en init() | Ver `Motor::init()` en código |
| Movimiento invertido | IN1/IN2 lógica invertida | Test manual con PWM → observar dirección |
| PWM insuficiente | PWM_MIN/MAX mal calibrados | Test Mode SAT: slider 0-255 |
| Calibración timeout | Motor atascado o sensor roto | Check ADS1115 lectura en Test Mode |
| Reinicios infinitos | S3 envía FLAG_CALIB continuamente | Verificar cooldown guard (2000ms) |
| Dead zone agresiva | DEAD_ZONE > 50 cuentas | Ajustar config.h, recompilar |

---

## 6. CONTROL EMA FILTER

Motor recibe feedback suavizado (ruido reducido ±3 cuentas vs ±30 nativo):

```cpp
// En FaderADC.cpp
filteredPos = filteredPos + (rawPos - filteredPos) * FADER_EMA_ALPHA_FAST;
// FADER_EMA_ALPHA_FAST = 0.20f (75% histórico, 25% nuevo)

// En Motor.cpp
Motor::setADC(uint16_t pos) {
    _motor_adcPos = pos;  // Usa filteredPos enviado por FaderADC
    // Luego calcula error vs target
}
```

**Ventaja:** Suaviza sin crear dead zones excesivas.

---

## 7. HISTÓRIA DE FIXES (2026-05-16)

### 2026-05-16 08:29 — Guard Cooldown & Auto-Calib Disable

**Problema:** S3 continuamente enviaba FLAG_CALIB después de calibración → motor reiniciaba calibración infinitamente.

**Root Cause:** S2 no tenía mecanismo para rechazar FLAG_CALIB si ya completó recientemente.

**Fix:**
1. Añadido `CALIB_COOLDOWN_MS = 2000` en config.h
2. Añadido `_motor_lastCalibDone` timestamp
3. Guard en `Motor::startCalib()`: rechaza si (now - _motor_lastCalibDone < 2000ms)
4. Desactivado auto-calibración en S2 main.cpp → S3 es autoridad única

**Commit:** `e0af808` + `7514f2b` + `9341f3b`

### 2026-05-09 23:50 — Lógica IN1/IN2 Invertida

**Problema:** Motor subía cuando debería bajar (lógica invertida).

**Fix:** Corregir _hwUp() y _hwDown() → UP=IN2 PWM, DOWN=IN1 PWM

### Pre-2026-05-09 — PWM Insuficiente

**Problema:** Motor muy lento o sin movimiento.

**Fix:** Calibrar PWM_MIN=100, PWM_MAX=160 (testeo en bench).

---

## Referencias

- **FADER.md** — Documentación ADS1115, calibración bidireccional, EMA filter
- **CLAUDE.md** — Directivas obligatorias (NUNCA compilar, orden init, etc.)
- **STATUS.md** — Bugs conocidos, pendientes críticos
- **config.h (S2)** — Fuente de verdad para constantes motor

---

## Últimas Actualizaciones

- **(2026-05-16)** Creado Motor.md como documento exhaustivo, trasladado contenido de CLAUDE.md
