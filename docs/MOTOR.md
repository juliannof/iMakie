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

### 1.3 Orden de Inicialización en setup() — CRÍTICO (2026-05-16)

**Líneas exactas de main.cpp:**

```c
setup() {
  // 1️⃣  LÍNEA 120-121: Configura GPIO14 (MOTOR_EN) ANTES de Motor::init()
  pinMode(MOTOR_EN, OUTPUT);
  digitalWrite(MOTOR_EN, LOW);      ← ⚠️ INMEDIATO (evita movement)
  
  // 2️⃣ LÍNEA 122: Safety delay
  delay(10);
  
  // 3️⃣ LÍNEA 123: Motor::init() — silencia motor (EN ya LOW)
  Motor::init();
  
  // LÍNEA 124: Serial.begin()
  Serial.begin(115200);
  
  // LÍNEA 132: Lee PWM range de NVS (guardado por SAT)
  Motor::initPWM();             ← ⚠️ SAT guarda pwmMin/pwmMax tras calibración
  
  // ... Display, Neopixels, Encoder, ButtonManager, SatMenu init ...
  
  // LÍNEA 187: ⚠️ CRÍTICO — ADC debe estar listo ANTES de Motor::update()
  faderADC.begin();
  
  // ... más hardware ...
  
  // LÍNEA 233: Motor baja a posición 0 ANTES de RS485
  Motor::goToMin();  // ← Baja fader, espera órdenes S3
  
  // LÍNEA 236: RS485 init — ÚLTIMO (Motor bajando, ADC listo)
  rs485.begin(slaveId);
}
```

**Estado final tras setup():**
- Motor EN (GPIO14) = activo, bajando hacia posición 0
- PWM range = cargado de NVS (si SAT calibró previamente)
- ADC = listo (FaderADC inicializado)
- Fader = bajando, llegará a 0 en loop() dentro de ~1-2 segundos
- RS485 = escuchando a S3
- Motor esperará orden de calibración (FLAG_CALIB) de S3

**¿Por qué este orden?**

1. **Motor EN LOW ANTES de Motor::init():** Evita pulso accidental en pines IN1/IN2 durante init
2. **Motor::init() ANTES de Serial:** Configura PWM sin debug output
3. **Motor::initPWM() DESPUÉS de Serial:** Lee NVS (requiere log output si hay error)
4. **faderADC.begin() ANTES de SAT init:** Motor necesita feedback ADC en loop()
5. **Motor::goToMin() ANTES de RS485:** Garantiza fader en posición 0 al recibir órdenes S3
6. **RS485 init ÚLTIMO:** Todos los módulos listos para procesar MasterPackets (incluyendo FLAG_CALIB)

---

## 2. ARQUITECTURA MOTOR v3 — PRIORIDADES DE CONTROL (2026-05-16 18:45)

### 2.0.1 Jerarquía de Prioridades — VINCULANTE

```
PRIORIDAD 1 (MÁXIMA):  Usuario mueve fader → Motor para INMEDIATAMENTE
PRIORIDAD 2:           Motor::goToMin() ejecuta SIEMPRE si no conectado a S3
PRIORIDAD 3:           S3 ordena posición → Motor se mueve SOLO si usuario NO toca
PRIORIDAD 4 (MÍNIMA):  Sin comando: Motor idle en posición actual
```

**Principio fundamental:** El usuario tiene control físico absoluto. S3 es esclavo que responde, no maestro que ordena.

### 2.0.2 Flujo Completo Operación (2026-05-16 18:45)

```
SETUP:
  Motor::init()         ← Configura pines
  Motor::initPWM()      ← Lee PWM de NVS (o fallback config.h)
  Motor::goToMin()      ← INMEDIATAMENTE baja a 0
  
LOOP (mientras motor bajando):
  Motor::update()       ← Máquina de estados baja a 0
  Motor::setADC()       ← Recibe posición ADC desde FaderADC
  Motor::setADCDelta()  ← Detecta movimiento usuario

CUANDO FADER LLEGA A 0:
  Motor en AT_TARGET (posición 0)
  Motor apagado
  Esperando órdenes

ESCENARIOS DURANTE OPERACIÓN:

  1️⃣  Calibración (S3 ordena, independiente de Logic):  (2026-05-16 19:24)
      ├─ S3 envía FLAG_CALIB al boot
      │  └─ Motor::requestCalibration()  ← procesado ANTES de desconexión
      │     ├─ Si ADC ≠ 0: Motor::goToMin() baja a 0
      │     └─ Si ADC = 0: startCalib() directo
      ├─ Motor transiciona: GOING_TO_MIN → WAITING_FOR_CALIB → CALIBRATING
      └─ BuildResponse() reporta CALIB_DONE cuando completa
      
  2️⃣  S3 conectado + usuario NO toca (post-calibración):
      └─ S3 envía setTarget(X):
         └─ Motor se mueve a X (si usuario NO toca)

  3️⃣  Usuario mueve fader:
      ├─ Motor::setADCDelta() detecta delta grande O FaderTouch activo
      ├─ Motor::stop() INMEDIATAMENTE
      ├─ Usuario es MASTER → ADC actual = nueva posición
      ├─ touchState=1 reportado a S3 vía RS485
      └─ S3 ignora targets mientras usuario toque (setTargetFromS3 rechaza)

  4️⃣  Usuario suelta fader:
      ├─ _motor_manualTouchDetected = false (después 200ms debounce)
      └─ S3 puede enviar nuevo target (Motor acepta)

  5️⃣  S3 desconectado (Logic no conectado):
      ├─ Motor::setConnected(false) ejecutado
      ├─ Motor::goToMin() SIEMPRE activo
      └─ Fader baja a 0 indefinidamente (IDLE loop)

  6️⃣  S3 conectado (Logic conectado):
      ├─ Motor::setConnected(true) ejecutado
      ├─ Motor NO baja automáticamente
      └─ Espera órdenes S3
```

### 2.0.3 Variables de Estado y Guards

```cpp
// Estado conexión S3
static bool _connected;                    ← setConnected() actualiza esto

// Detección movimiento usuario
static bool _motor_manualTouchDetected;    ← setADCDelta() actualiza
static uint32_t _motor_manualTouchStartTime;
static uint16_t _motor_lastADCForDelta;

// Máquina estados
static MotorState _motor_state;            ← IDLE, GOING_TO_MIN, WAITING_FOR_CALIB, CALIBRATING, MOVING_TO_TARGET, AT_TARGET

// Flags
static bool _pendingCalib;                 ← requestCalibration() pone en true
static bool _motor_goingToMin;             ← goToMin() pone en true
```

---

## 2. CONTROL DE MOTOR

### 2.1 APIs Críticas (Motor.h)

```cpp
// Inicialización (setup)
Motor::init()              // Configura pines (EN→LOW, IN1/IN2 PWM 20kHz)
Motor::initPWM()           // Lee pwmMin/Max de NVS (SAT las guarda)
Motor::goToMin()           // Baja fader a posición 0 (MASTER, ejecuta SIEMPRE si !_connected)
Motor::off()               // Apaga motor (emergencia)

// Calibración (v3 — 2026-05-16)
Motor::requestCalibration() // FLAG_CALIB desde RS485 → baja a 0 si necesario, luego calibra
Motor::startCalib()        // Inicia máquina calibración (KICK_UP → DONE) — REQUIERE estar en 0
Motor::getCalibState()     // Estado actual (IDLE/CALIB_UP/CALIB_DOWN/DONE/ERROR)

// Control de usuario (MASTER — máxima prioridad)
Motor::setADCDelta(uint16_t currentADC) // Detecta movimiento usuario (delta > 500 O capacitivo)
                           // Si activo: Motor::stop(), usuario toma control, ADC = target

// Control desde S3 (ESCLAVO — solo si usuario NO toca)
Motor::setTargetFromS3(uint16_t adcTarget) // S3 ordena posición — RECHAZADO si usuario toca
Motor::setConnected(bool connected)        // Notifica estado conexión S3 (goToMin respeta esto)

// Control en loop (post-calibración)
Motor::setADC(uint16_t pos)  // Actualiza _motor_adcPos desde FaderADC (ejecutar SIEMPRE)
Motor::update()              // Máquina de estado (ejecutar SIEMPRE en main loop)

// Diagnóstico & Test
Motor::getRawADC()         // Lectura ADC actual (_motor_adcPos)
Motor::getPosition()       // Posición normalizada 0.0-1.0
Motor::getADCMin() / getADCMax() // Rango calibrado
Motor::getState()          // MotorState actual (IDLE, GOING_TO_MIN, etc.)
Motor::testUp(pwm) / testDown(pwm) / testOff()  // Test Manual SAT
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

### 2.2.1 Motor::initPWM() y Persistencia NVS (2026-05-16)

**Flujo completo PWM Min/Max:**

```
Primer boot:
  Motor::initPWM() → lee NVS ("ptxx", pwmMin/Max) → no existe
  → _pwm_min=0, _pwm_max=0 (inválido)
  → Motor usa fallback PWM_MIN=100, PWM_MAX=160 (config.h)

Usuario entra SAT → edita PWM Min/Max:
  SatMenu::loadConfig() → lee NVS
  SatMenu::saveConfig() → guarda con:
    _prefs.putUChar("pwmMin", _cfg.pwmMin);
    _prefs.putUChar("pwmMax", _cfg.pwmMax);

Próximo boot:
  Motor::initPWM() → lee NVS
  → encuentra pwmMin=123, pwmMax=157 (valor guardado por SAT)
  → _pwm_min=123, _pwm_max=157 ← usa valores persistentes
```

**Código relevante:**

Motor.cpp (líneas 297-315):
```cpp
void initPWM() {
    Preferences prefs;
    prefs.begin("ptxx", true);  // read-only
    uint8_t pwmMin = prefs.getUChar("pwmMin", 0);
    uint8_t pwmMax = prefs.getUChar("pwmMax", 0);
    prefs.end();

    if (pwmMin > 0 && pwmMax > 0 && pwmMin < pwmMax) {
        _pwm_min = pwmMin;      // ← Usa valor de SAT
        _pwm_max = pwmMax;      // ← Usa valor de SAT
        log_i("[MOTOR] PWM: %u-%u (NVS)", _pwm_min, _pwm_max);
    } else {
        _pwm_min = 0;
        _pwm_max = 0;           // ← Fallback a config.h en Motor.cpp init
        log_e("[MOTOR] PWM NVS inválido");
    }
}
```

SatMenu.cpp (guardado):
```cpp
// Dentro de saveConfig()
_prefs.putUChar("pwmMin",  _cfg.pwmMin);   // Persistencia
_prefs.putUChar("pwmMax",  _cfg.pwmMax);   // Persistencia
```

**¿Por qué es importante?**
- Cada S2 puede tener motor con características diferentes (fricción, inercia, etc.)
- SAT calibra PWM Min/Max para ese motor específico en la sesión física
- NVS persiste calibración entre reboots → no necesita recalibrar PWM cada boot
- Motor::initPWM() es el puente entre SAT y motor en el siguiente boot

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

## 2.4 Comportamiento Inicial (Boot) — Motor::goToMin() (2026-05-16 10:45)

**Flujo de setup() → loop():**

```
setup() LÍNEA 233: Motor::goToMin()
  ↓
_motor_goingToMin = true
_hwDown(_pwm_max)  ← Motor comienza a bajar
log: "goToMin: bajando a posición 0..."
  ↓
setup() termina, entra a loop()
  ↓
loop() tick 1-N: Motor::update() ejecuta lógica goToMin:
  if (_motor_adcPos <= MOTOR_ADC_MIN + 10) {  // ADC ≈ 20-30
    _hwOff()           ← Motor se apaga
    _motor_goingToMin = false
    log: "goToMin: llegó a 0 (ADC=XX)"
    return
  }
  ↓
Fader en 0, motor apagado, esperando órdenes S3
```

**Duración:** ~1-2 segundos (solo baja, no mide ruido)

**Estado tras goToMin() DONE:**

| Estado | Valor | Notas |
|--------|-------|-------|
| Fader posición | ADC ≈ 20-30 (posición 0) | Físicamente abajo |
| Motor | Apagado (EN=LOW) | No consume corriente |
| Fase motor | `CalibPhase::IDLE` | Listo para calibración |
| PWM_MIN/MAX | Cargado de NVS | Conoce su rango PWM |
| ADC | Listo (FaderADC activo) | Leyendo continuamente |

**Siguiente paso:** Espera FLAG_CALIB de S3 para calibración completa

```
S3 envía MasterPacket con FLAG_CALIB
  ↓
RS485Handler::onMasterData() detecta FLAG_CALIB
  ↓
Motor::startCalib() → transición IDLE → KICK_UP
  ↓
~8 segundos: calibración completa (sube, baja, mide ruido)
  ↓
Motor::update() → DONE
  ↓
Fader listo para recibir PitchBend de Logic
```

**Ventajas de goToMin():**
- ✅ Fader garantizado en 0 al boot
- ✅ Rápido (~1-2s) vs calibración (~8s)
- ✅ Posición conocida antes de calibración
- ✅ Motor listo para órdenes S3 inmediatamente

---

## 2.5 Máquina de Estados Motor v2 — Usuario Master (2026-05-16 10:52)

**Prioridad correcta:**
```
Usuario tocando > S3 commands > Motor autónomo
```

**Principios (actualizado 2026-05-16 10:52):**
- **Usuario es el master absoluto** — puede tomar control en cualquier momento
- S3 controla SOLO si usuario no toca el fader
- Si usuario mueve → Motor para INMEDIATAMENTE, ADC actual = nuevo target
- Si usuario suelta → Motor queda en posición, S3 puede mandar nuevo target
- Sin comando S3 y sin usuario: fader en 0 (IDLE → GOING_TO_MIN → AT_TARGET)
- Estados: IDLE → GOING_TO_MIN → WAITING_FOR_CALIB → CALIBRATING → IDLE
- Alternativa: IDLE → GOING_TO_MIN → AT_TARGET (usuario soltó)
- CONNECTED a S3: NO baja a 0 automáticamente, espera órdenes
- DISCONNECTED de S3: baja a 0 (goToMin loop)

### **2.5.1 Estados y Transiciones**

```
┌─────────────────────────────────────────────────────────────┐
│ IDLE                                                        │
│ - Fader en 0, esperando órdenes S3 o usuario             │
│ - Si ADC > 30: bajar a 0 (GOING_TO_MIN)                  │
│ - Si ADC ≤ 30: apagar motor                               │
└─────────────────────────────────────────────────────────────┘
    ↓ (ADC > 30)
┌─────────────────────────────────────────────────────────────┐
│ GOING_TO_MIN                                                │
│ - Motor baja con PWM_MAX                                   │
│ - Si llega a 0:                                            │
│   ├─ Si _pendingCalib: → WAITING_FOR_CALIB               │
│   └─ Sino: → AT_TARGET                                    │
└─────────────────────────────────────────────────────────────┘
    ↓ (FLAG_CALIB pendiente)
┌─────────────────────────────────────────────────────────────┐
│ WAITING_FOR_CALIB                                           │
│ - En 0, esperando que startCalib() se ejecute             │
│ - Cuando _pendingCalib activado: → CALIBRATING            │
└─────────────────────────────────────────────────────────────┘
    ↓ (_pendingCalib = true)
┌─────────────────────────────────────────────────────────────┐
│ CALIBRATING                                                 │
│ - Máquina calibración en curso (KICK_UP → DONE)          │
│ - Si DONE o ERROR: → IDLE                                 │
└─────────────────────────────────────────────────────────────┘
    ↓ (calibración completa)
└──────────→ IDLE (vuelve al inicio)

┌─────────────────────────────────────────────────────────────┐
│ MOVING_TO_TARGET (desde S3 setTarget)                      │
│ - Motor se mueve a posición S3                             │
│ - Si llega: → AT_TARGET                                    │
└─────────────────────────────────────────────────────────────┘
    ↓ (error < DEAD_ZONE)
┌─────────────────────────────────────────────────────────────┐
│ AT_TARGET                                                   │
│ - Posición objetivo alcanzada, esperando nuevo comando S3  │
│ - Si timeout (30s): → IDLE                                │
│ - Si usuario suelta en Y: → GOING_TO_MIN (_userDropTarget) │
└─────────────────────────────────────────────────────────────┘
```

### **2.5.2 Funciones Públicas (API v2 — 2026-05-16 10:52)**

```cpp
// Motor state machine
void requestCalibration();           // FLAG_CALIB desde RS485
void setTargetFromS3(uint16_t adc);  // setTarget ADC desde RS485 — GUARDED por usuario
void setUserDropTarget(uint16_t adc); // Usuario soltó fader en ADC
void setConnected(bool connected);   // Notifica estado conexión S3 (NEW 2026-05-16)
MotorState getState();                // Consulta estado motor
void setADCDelta(uint16_t currentADC); // Detecta movimiento usuario (delta OR capacitivo)
```

### **2.5.3 Escenarios de Uso**

**Escenario 1: Boot sin comando S3**
```
setup() → Motor en IDLE
loop(): ADC > 30 → GOING_TO_MIN → baja a 0 → IDLE
Motor queda en 0, esperando S3
```

**Escenario 2: Usuario suelta fader en posición Y**
```
IDLE (fader en 0) 
  → usuario arrastra a Y
  → usuario suelta
  → setUserDropTarget(Y) 
  → GOING_TO_MIN (ahora target=Y)
  → llega a Y
  → AT_TARGET (esperando S3)
```

**Escenario 3: S3 ordena calibración (FLAG_CALIB)**
```
IDLE (fader en posición X) 
  → requestCalibration() detecta X ≠ 0
  → GOING_TO_MIN + _pendingCalib=true
  → llega a 0
  → WAITING_FOR_CALIB
  → startCalib() ejecuta
  → CALIBRATING 
  → ~8s después: DONE
  → IDLE
```

**Escenario 4: S3 ordena setTarget(P) mientras está en AT_TARGET**
```
AT_TARGET (posición Y)
  → setTargetFromS3(P) desde RS485
  → MOVING_TO_TARGET (target=P)
  → llega a P
  → AT_TARGET
```

### **2.5.4 Variables Internas**

```cpp
static MotorState _motor_state;        // Estado actual
static bool _pendingCalib;             // Flag: calibración en espera
static uint16_t _userDropTarget;       // ADC donde usuario soltó
static uint16_t _s3Target;             // Target actual de S3
static uint32_t _atTargetStartTime;    // timestamp llegada a AT_TARGET
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

### 3.3 Inicio de Calibración (v3 — 2026-05-16 18:45)

**Boot automático (S3 ordena FLAG_CALIB vía RS485):**
```
S3 envía MasterPacket con FLAG_CALIB
  ↓
S2 RS485Handler::onMasterData() detecta FLAG_CALIB
  ↓
Motor::requestCalibration()  ← NUEVA FUNCIÓN — reemplaza startCalib()
  ├─ Si fader EN 0:
  │   └─ Motor::startCalib() → KICK_UP inmediatamente
  └─ Si fader NO EN 0:
      └─ Motor::goToMin() primero
         ├─ Baja a 0
         ├─ Motor queda en AT_TARGET
         └─ startCalib() se ejecuta en siguiente ciclo
```

**Código requestCalibration() — implementación (2026-05-16):**
```cpp
void requestCalibration() {
    // S3 ordena FLAG_CALIB → si fader en 0, calibra; si no, baja primero
    if (_motor_adcPos <= (MOTOR_ADC_MIN + 10)) {
        // Ya en 0 → calibra directamente
        _motor_state = MotorState::CALIBRATING;
        startCalib();
        log_i("[MOTOR] requestCalibration: fader ya en 0, calibrando");
    } else {
        // No en 0 → baja primero, luego calibra
        _motor_state = MotorState::GOING_TO_MIN;
        _pendingCalib = true;           ← FLAG para startCalib() en siguiente ciclo
        _motor_goingToMin = true;
        _hwDown(_pwm_max);
        log_i("[MOTOR] requestCalibration: bajando a 0 primero, luego calibrar");
    }
}
```

**Manual (SAT menu):**
```
Usuario: Encoder push >3s → SAT menu
         Motor → Calibración
         → Motor::requestCalibration()  ← Mismo flujo
```

**Ventajas requestCalibration() vs startCalib():**
- ✅ Garantiza fader en 0 ANTES de calibración
- ✅ No necesita "esperar" que usuario lo coloque
- ✅ Arquitectura clean: S3 no necesita conocer posición actual
- ✅ Reutiliza goToMin() (MASTER control)

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

## 3. DETECCIÓN DE USUARIO — Master Control (2026-05-16 10:52)

### 3.1 Mecanismo: Sensor Capacitivo + Delta ADC

**Dos detecciones en paralelo:**

| Método | Fuente | Umbral | Ventaja |
|--------|--------|--------|---------|
| **Capacitivo** | FaderTouch::isTouched() | Contacto físico | Preciso, sin lag |
| **Delta ADC** | setADCDelta(currentADC) | > 500 cuentas/tick | Detecta velocidad rápida |

**Lógica:**
```cpp
bool userTouch = (delta > MANUAL_TOUCH_THRESHOLD) || FaderTouch::isTouched();

if (userTouch && !_motor_manualTouchDetected) {
    // Usuario toma control
    _motor_manualTouchDetected = true;
    Motor::stop();  // Para motor INMEDIATAMENTE
    _motor_state = MotorState::AT_TARGET;
    _motor_targetADC = currentADC;  // ADC actual = nueva posición
}
```

### 3.2 Flujo: Usuario Toma Control

```
Loop() — usuario mueve fader rápido:
  setADCDelta(currentADC) detecta delta > 500
    ↓
  _motor_manualTouchDetected = true
  Motor::stop()  ← Motor para INMEDIATAMENTE
  _motor_state = AT_TARGET  ← Usuario define posición
  _motor_targetADC = currentADC  ← Nuevo target aceptado
  log: "Usuario master: adc=12345"
    ↓
  RS485Handler::buildResponse():
    touchState = FaderTouch::isTouched() ? 1 : 0
    faderPos = Motor::getRawADC()  ← Nueva posición
    → envía SlavePacket a S3
      ↓
  S3 recibe touchState=1 + nueva faderPos
    → Logic entiende "usuario movió fader"
    → Logic NOT envía nuevo target (respeta usuario)
      ↓
  Usuario suelta fader (después de MANUAL_TOUCH_DEBOUNCE_MS):
    _motor_manualTouchDetected = false
    Motor queda en AT_TARGET (nueva posición del usuario)
    S3 ahora puede mandar nuevo target
```

### 3.3 Guardia en setTargetFromS3()

```cpp
void setTargetFromS3(uint16_t adcTarget) {
    if (_motor_manualTouchDetected || FaderTouch::isTouched()) {
        return;  // Usuario es master — S3 ignorado
    }
    _motor_targetADC = adcTarget;
    _motor_state = MotorState::MOVING_TO_TARGET;
}
```

**Interpretación:**
- Si `_motor_manualTouchDetected` activo → usuario moviendo → IGNORA S3
- Si `FaderTouch::isTouched()` → dedo en fader → IGNORA S3
- Sino → usuario liberó → S3 PUEDE controlar

### 3.4 Conexión S3 (setConnected)

**Nueva variable:**
```cpp
static bool _connected = false;  // Estado conexión S3
```

**Guards en IDLE y goToMin():**

**IDLE:**
```cpp
if (!_connected && _motor_adcPos > (MOTOR_ADC_MIN + 10)) {
    // Sin S3 → bajar a 0
    _motor_state = MotorState::GOING_TO_MIN;
} else {
    // CONNECTED → motor quieto
    _hwOff();
}
```

**goToMin():**
```cpp
if (_connected) return;  // No ejecutar si S3 conectado
```

**Flujo:**
```
Boot: _connected = false
  → IDLE detecta ADC > 30
  → Baja a 0 (GOING_TO_MIN)
  → Llega a 0 (AT_TARGET)
  
RS485Handler recibe MasterPacket con connected=1
  → Motor::setConnected(true)
  → update() IDLE: !_connected es false → no baja
  → Motor quieto, espera target S3
  
S3 ordena setTargetFromS3()
  → Motor va a target (MOVING_TO_TARGET → AT_TARGET)
  
RS485 timeout 500ms
  → checkTimeout() → Motor::setConnected(false)
  → update() IDLE: !_connected es true → baja a 0
```

---

## Referencias

- **FADER.md** — Documentación ADS1115, calibración bidireccional, EMA filter
- **CLAUDE.md** — Directivas obligatorias (NUNCA compilar, orden init, etc.)
- **STATUS.md** — Bugs conocidos, pendientes críticos
- **config.h (S2)** — Fuente de verdad para constantes motor

---

## Últimas Actualizaciones

- **(2026-05-16)** Creado Motor.md como documento exhaustivo, trasladado contenido de CLAUDE.md
