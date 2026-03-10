// ============================================================
//  Motor.cpp  —  Control de fader motorizado PTxx Track S2
//  DRV8833  |  ADC via FaderADC → setADC()  |  DAC ref GPIO17
//
//  CALIBRACIÓN NO BLOQUEANTE (máquina de estados):
//    IDLE → CALIB_DOWN → CALIB_UP → DONE
//    Disparada por S3 vía RS485 → Motor::startCalib()
//    Avanza en update() — no bloquea el loop nunca
//
//  DETECCIÓN DE TOPE:
//    Lógica de estabilidad: si el ADC no cambia más de
//    ADC_STABILITY_THRESHOLD durante ADC_STABLE_TIME → tope
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



// ─── Estado interno ───────────────────────────────────────────
static Motor::CalibState _calibState   = Motor::CalibState::IDLE;
static uint16_t _adcMin      = 0;
static uint16_t _adcMax      = 8191;
static uint16_t _adcSpan     = 8191;
static float    _adcFiltered = 0.0f;
static uint16_t _targetADC   = 0;
static int      _currentPWM  = 0;

// Calibración — estado interno
static uint16_t _calibTopeDown  = 0;
static uint32_t _calibStartT    = 0;
static int      _calibStableVal = 0;
static uint32_t _calibStableT   = 0;

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
        analogWrite(MOTOR_IN1, constrain(pwm, 0, 255));
        digitalWrite(MOTOR_IN2, LOW);
    } else {
        digitalWrite(MOTOR_IN1, LOW);
        analogWrite(MOTOR_IN2, constrain(-pwm, 0, 255));
    }
}

// ─── Tick de calibración ─────────────────────────────────────
// Detección de tope: estabilidad O posición límite
//   valor quieto ADC_STABLE_TIME → tope
//   pos <= 150 → tope inferior inmediato
//   pos >= 8000 → tope superior inmediato
static void _calibTick() {
    faderADC.update();
    _adcFiltered = (float)faderADC.getFaderPos();
    int pos = (int)_adcFiltered;
    uint32_t now = millis();

    // Timeout por fase
    if (now - _calibStartT > CALIB_TIMEOUT) {
        _hwStop();
        _calibState = Motor::CalibState::ERROR;
        Serial.println("[MOTOR] Calibracion TIMEOUT");
        return;
    }

    bool atBottom = (pos <= 150);
    bool atTop    = (pos >= 8000);

    if (abs(pos - _calibStableVal) <= ADC_STABILITY_THRESHOLD) {
        // Quieto — acumular tiempo estable
        uint32_t tiempoEstable = now - _calibStableT;

        Serial.printf("[MOTOR] %s pos=%d estable=%lums\n",
            (_calibState == Motor::CalibState::CALIB_DOWN) ? "DOWN" : "UP  ",
            pos, tiempoEstable);

        bool topeDetectado = (tiempoEstable >= ADC_STABLE_TIME) ||
                             (_calibState == Motor::CalibState::CALIB_DOWN && atBottom) ||
                             (_calibState == Motor::CalibState::CALIB_UP   && atTop);

        if (topeDetectado) {
            _hwStop();
            delay(100);
            faderADC.update();
            _adcFiltered = (float)faderADC.getFaderPos();
            pos = (int)_adcFiltered;

            if (_calibState == Motor::CalibState::CALIB_DOWN) {
                _calibTopeDown = (uint16_t)pos;
                Serial.printf("[MOTOR] Tope BAJO detectado pos=%d\n", _calibTopeDown);

                delay(300); // pausa antes de invertir
                _calibState     = Motor::CalibState::CALIB_UP;
                _calibStableVal = pos;
                _calibStableT   = millis();
                _calibStartT    = millis();
                _hwDrive(-CALIB_PWM); // sube

            } else {
                uint16_t topeUp = (uint16_t)pos;
                Serial.printf("[MOTOR] Tope ALTO detectado pos=%d\n", topeUp);

                if (topeUp > _calibTopeDown && (topeUp - _calibTopeDown) > 500) {
                    _adcMin     = _calibTopeDown + 20;
                    _adcMax     = topeUp         - 20;
                    _adcSpan    = _adcMax - _adcMin;
                    _targetADC  = (uint16_t)_adcFiltered;
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
    } else {
        // Valor cambiando — resetear timer de estabilidad
        _calibStableVal = pos;
        _calibStableT   = now;
    }
}

// ─── Control de posición ─────────────────────────────────────
static bool _motorActive = false;

static void _controlTick() {
    int pos    = (int)_adcFiltered;
    int err    = (int)_targetADC - pos;
    int absErr = abs(err);

    // Hysteresis: arranca si supera DEAD_ZONE, para si baja de DEAD_ZONE/2
    if (!_motorActive && absErr > DEAD_ZONE) {
        _motorActive = true;
    } else if (_motorActive && absErr < DEAD_ZONE / 2) {
        _motorActive = false;
        _hwStop();
        return;
    }

    if (!_motorActive) { _hwStop(); return; }

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

    int slewed  = targetPWM - _currentPWM;
    slewed      = constrain(slewed, -PWM_SLEW, PWM_SLEW);
    _currentPWM += slewed;

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
    // PWM 20kHz — por encima del umbral auditivo
    analogWriteFrequency(MOTOR_IN1, 20000);
    analogWriteFrequency(MOTOR_IN2, 20000);
}

// Inicializar sin arrancar calibración
// La calibración la dispara el S3 vía RS485 → startCalib()
void begin() {
    faderADC.update();
    _adcFiltered = (float)faderADC.getFaderPos();
    _calibState  = CalibState::IDLE;
    Serial.println("[MOTOR] Listo. Esperando orden de calibracion del S3.");
}

// Llamar cuando el S3 ordena calibrar por RS485
void startCalib() {
    if (_calibState == CalibState::CALIB_DOWN ||
        _calibState == CalibState::CALIB_UP) return; // ya calibrando

    faderADC.update();
    _adcFiltered    = (float)faderADC.getFaderPos();
    _calibStableVal = (int)_adcFiltered;
    _calibStableT   = millis();
    _calibStartT    = millis();
    _calibState     = CalibState::CALIB_DOWN;
    _hwDrive(+CALIB_PWM); // +pwm = baja
    Serial.println("[MOTOR] Calibracion iniciada por S3.");
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