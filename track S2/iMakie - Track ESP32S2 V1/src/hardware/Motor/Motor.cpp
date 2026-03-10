// ============================================================
//  Motor.cpp  —  Control de fader motorizado PTxx Track S2
//  DRV8833  |  ADC via FaderADC → setADC()  |  DAC ref GPIO17
//
//  CALIBRACIÓN NO BLOQUEANTE (máquina de estados):
//    IDLE → CALIB_DOWN → CALIB_UP → DONE
//    Avanza en update() — no bloquea el loop
//    Watchdog nunca muere
//
//  ARQUITECTURA ADC:
//    FaderADC es el único dueño del ADC — siempre.
//    Motor::setADC() recibe el valor desde main.cpp.
//
//  TOUCH (gestionado en main.cpp):
//    touch=1 → Motor::stop()    usuario mueve manualmente
//    touch=0 → Motor::update()  motor controla posición
// ============================================================
#include "Motor.h"
#include "../../fader/FaderADC.h"
#include "../../config.h"

extern FaderADC faderADC;

// ─── Parámetros de control ────────────────────────────────────
static constexpr uint8_t  PWM_MIN      = 80;    // mínimo PWM que mueve el motor
static constexpr uint8_t  PWM_MAX      = 255;   // máximo
static constexpr uint8_t  PWM_SLEW     = 12;    // máx cambio de PWM por ciclo
static constexpr int      DEAD_ZONE    = 15;    // FaderADC counts — no mover
static constexpr int      BRAKE_DIST   = 20;    // FaderADC counts — frenar
static constexpr float    CURVE_GAMMA  = 0.6f;  // curva no lineal

// ─── Parámetros de calibración ───────────────────────────────
static constexpr uint8_t  CALIB_PWM        = 200;   // velocidad calibración
static constexpr uint16_t STALL_THRESHOLD  = 30;    // FaderADC counts mínimo de movimiento
static constexpr uint32_t STALL_TIME       = 400;   // ms sin movimiento → tope detectado
static constexpr uint32_t CALIB_TIMEOUT    = 5000;  // ms máximo por fase

// ─── Estado interno ───────────────────────────────────────────
static Motor::CalibState _calibState   = Motor::CalibState::IDLE;
static uint16_t _adcMin      = 0;
static uint16_t _adcMax      = 8191;
static uint16_t _adcSpan     = 8191;
static float    _adcFiltered = 0.0f;
static uint16_t _targetADC   = 0;
static int      _currentPWM  = 0;

// Calibración — estado interno de la máquina
static uint16_t _calibLastPos  = 0;
static uint32_t _calibStallT   = 0;
static uint32_t _calibStartT   = 0;
static uint16_t _calibTopeDown = 0;

// ─── Helpers de hardware ─────────────────────────────────────
static void _hwStop() {
    digitalWrite(MOTOR_EN,  LOW);
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    _currentPWM = 0;
}

static void _hwDrive(int pwm) {
    if (pwm == 0) { _hwStop(); return; }
    digitalWrite(MOTOR_EN, HIGH);
    if (pwm > 0) {
        analogWrite(MOTOR_IN1, constrain( pwm, 0, 255));
        digitalWrite(MOTOR_IN2, LOW);
    } else {
        digitalWrite(MOTOR_IN1, LOW);
        analogWrite(MOTOR_IN2, constrain(-pwm, 0, 255));
    }
}

// ─── Tick de calibración (llamado desde update()) ────────────
static void _calibTick() {
    faderADC.update();
    _adcFiltered = (float)faderADC.getFaderPos();
    uint16_t pos = (uint16_t)_adcFiltered;
    uint32_t now = millis();

    // Timeout por fase
    if (now - _calibStartT > CALIB_TIMEOUT) {
        _hwStop();
        _calibState = Motor::CalibState::ERROR;
        Serial.printf("[MOTOR] Calibracion TIMEOUT en fase %d\n",
                      (int)_calibState);
        return;
    }

    // Detectar movimiento
    uint16_t delta = (pos > _calibLastPos) ? (pos - _calibLastPos)
                                           : (_calibLastPos - pos);
    if (delta >= STALL_THRESHOLD) {
        _calibLastPos = pos;
        _calibStallT  = now;
    }

    bool stalled = (now - _calibStallT) >= STALL_TIME;

    if (_calibState == Motor::CalibState::CALIB_DOWN) {
        Serial.printf("[MOTOR] DOWN pos=%d delta=%d stall=%lums\n",
                      pos, delta, now - _calibStallT);
        if (stalled) {
            _hwStop();
            delay(80);
            _calibTopeDown = (uint16_t)_adcFiltered;
            Serial.printf("[MOTOR] Tope BAJO detectado pos=%d\n", _calibTopeDown);

            // Arrancar fase UP
            _calibState   = Motor::CalibState::CALIB_UP;
            _calibLastPos = _calibTopeDown;
            _calibStallT  = millis();
            _calibStartT  = millis();
            _hwDrive(-CALIB_PWM); // -pwm = sube en este hardware
        }

    } else if (_calibState == Motor::CalibState::CALIB_UP) {
        Serial.printf("[MOTOR] UP pos=%d delta=%d stall=%lums\n",
                      pos, delta, now - _calibStallT);
        if (stalled) {
            _hwStop();
            delay(80);
            uint16_t topeUp = (uint16_t)_adcFiltered;
            Serial.printf("[MOTOR] Tope ALTO detectado pos=%d\n", topeUp);

            if (topeUp > _calibTopeDown && (topeUp - _calibTopeDown) > 500) {
                _adcMin   = _calibTopeDown + 20;
                _adcMax   = topeUp         - 20;
                _adcSpan  = _adcMax - _adcMin;
                _targetADC = (uint16_t)_adcFiltered; // no mover al terminar
                _calibState = Motor::CalibState::DONE;
                Serial.printf("[MOTOR] Calibrado OK  MIN=%d MAX=%d span=%d\n",
                              _adcMin, _adcMax, _adcSpan);
            } else {
                _calibState = Motor::CalibState::ERROR;
                Serial.printf("[MOTOR] Calibracion fallida bajo=%d alto=%d\n",
                              _calibTopeDown, topeUp);
            }
        }
    }
}

// ─── Control de posición (llamado desde update() si DONE) ────
static void _controlTick() {
    int pos    = (int)_adcFiltered;
    int err    = (int)_targetADC - pos;
    int absErr = abs(err);

    if (absErr <= DEAD_ZONE) {
        _hwStop();
        return;
    }

    int targetPWM;
    if (absErr <= BRAKE_DIST) {
        float frac = (float)absErr / BRAKE_DIST;
        targetPWM  = PWM_MIN + (int)(frac * (PWM_MIN * 0.6f));
    } else {
        float norm = (float)absErr / (float)_adcSpan;
        float vel  = powf(norm, CURVE_GAMMA);
        targetPWM  = PWM_MIN + (int)(vel * (PWM_MAX - PWM_MIN));
        targetPWM  = constrain(targetPWM, PWM_MIN, PWM_MAX);
    }

    if (err < 0) targetPWM = -targetPWM;

    int delta   = targetPWM - _currentPWM;
    delta       = constrain(delta, -PWM_SLEW, PWM_SLEW);
    _currentPWM += delta;

    _hwDrive(_currentPWM);
}

// ─── API pública ─────────────────────────────────────────────
namespace Motor {

// Llamar lo primero en setup() — silencia el motor antes de todo
void init() {
    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    pinMode(MOTOR_EN,  OUTPUT);
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    digitalWrite(MOTOR_EN,  LOW);
}

// Arranca la calibración no bloqueante
// Requiere faderADC.begin() llamado antes
void begin() {
    faderADC.update();
    _adcFiltered  = (float)faderADC.getFaderPos();
    _calibLastPos = (uint16_t)_adcFiltered;
    _calibStallT  = millis();
    _calibStartT  = millis();
    _calibState   = CalibState::CALIB_DOWN;
    _hwDrive(-CALIB_PWM); // -pwm = baja
    Serial.println("[MOTOR] Calibracion iniciada...");
}

void setADC(uint16_t value) {
    _adcFiltered = (float)value;
}

void setTarget(uint16_t midiTarget) {
    if (_calibState != CalibState::DONE) return;
    _targetADC = (uint16_t)map((long)midiTarget, 0, 16383, _adcMin, _adcMax);
}

void update() {
    switch (_calibState) {
        case CalibState::CALIB_DOWN:
        case CalibState::CALIB_UP:
            _calibTick();
            break;
        case CalibState::DONE:
            _controlTick();
            break;
        case CalibState::IDLE:
        case CalibState::ERROR:
        default:
            _hwStop();
            break;
    }
}

void stop() {
    _hwStop();
}

CalibState getCalibState() { return _calibState; }
bool       isCalibrated()  { return _calibState == CalibState::DONE; }
uint16_t   getADCMin()     { return _adcMin; }
uint16_t   getADCMax()     { return _adcMax; }
uint16_t   getRawADC()     { return (uint16_t)_adcFiltered; }
float      getPosition()   {
    if (_adcSpan == 0) return 0.0f;
    return constrain((_adcFiltered - _adcMin) / (float)_adcSpan, 0.0f, 1.0f);
}

} // namespace Motor