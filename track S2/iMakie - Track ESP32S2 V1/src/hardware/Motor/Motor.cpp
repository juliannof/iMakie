// ============================================================
//  Motor.cpp  —  Control de fader motorizado PTxx Track S2
//  DRV8833  |  ADC via FaderADC → setADC()
//
//  CALIBRACIÓN: lógica idéntica a CalibrationManager original
//    Fase 1 CALIB_DOWN: _hwUp()   → fader sube → detecta tope superior
//    Fase 2 CALIB_UP:   _hwDown() → fader baja → detecta tope inferior
//    Detección: valor ADC estable <= ADC_STABILITY_THRESHOLD durante CALIB_STABLE_TIME
//
//  CONTROL DE POSICIÓN post-calibración:
//    _hwUp(pwm)   → sube
//    _hwDown(pwm) → baja
//    _hwStop()    → para
// ============================================================
#include "Motor.h"
#include "../../fader/FaderADC.h"
#include "../../config.h"

extern FaderADC faderADC;


static Motor::CalibState _calibState = Motor::CalibState::IDLE;


// ─── Helpers de hardware ─────────────────────────────────────
static void _hwStop() {
    digitalWrite(MOTOR_EN,  LOW);
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    _currentPWM = 0;
}

static void _hwDown(uint8_t pwm) {
    digitalWrite(MOTOR_EN, HIGH);
    digitalWrite(MOTOR_IN1, LOW);
    analogWrite(MOTOR_IN2, pwm);   // IN2 = baja
}

static void _hwUp(uint8_t pwm) {
    digitalWrite(MOTOR_EN, HIGH);
    analogWrite(MOTOR_IN1, pwm);   // IN1 = sube
    digitalWrite(MOTOR_IN2, LOW);
}



// ─── Tick de calibración — lógica idéntica a CalibrationManager ──
static void _calibTick() {
    faderADC.update();
    _adcFiltered = (float)faderADC.getFaderPos();
    int adcActual = (int)_adcFiltered;

    Serial.printf("[MOTOR] %s adc=%d estable=%lums\n",
    (_calibState == Motor::CalibState::CALIB_DOWN) ? "CALIB_DOWN" : "CALIB_UP",
    adcActual, millis() - _ultimoTiempoEstable);

    // Timeout global
    if (millis() - _tiempoInicioCalibracion > CALIB_TIMEOUT) {
        _hwStop();
        _calibState = Motor::CalibState::ERROR;
        Serial.println("[MOTOR] TIMEOUT en calibracion");
        return;
    }

    Serial.printf("[MOTOR] %s adc=%d\n",
        (_calibState == Motor::CalibState::CALIB_DOWN) ? "CALIB_DOWN" : "CALIB_UP",
        adcActual);

    // Detección de estabilidad — idéntico a CalibrationManager
    if (abs(adcActual - _ultimoValorEstable) <= ADC_STABILITY_THRESHOLD) {
        if (millis() - _ultimoTiempoEstable > CALIB_STABLE_TIME) {
            // Estabilidad detectada
            if (_calibState == Motor::CalibState::CALIB_DOWN) {
                // Tope superior encontrado
                _posicionMaximaADC   = (uint16_t)adcActual;
                Serial.printf("[MOTOR] Tope SUPERIOR detectado: %d\n", _posicionMaximaADC);

                _calibState          = Motor::CalibState::CALIB_UP;
                _ultimoTiempoEstable = millis();
                _ultimoValorEstable  = -1;  // ← fuerza reset en primer tick
                _hwDown(CALIB_PWM);  // ahora baja → busca tope inferior
                Serial.println("[MOTOR] Bajando a tope inferior...");

            } else if (_calibState == Motor::CalibState::CALIB_UP) {
                // Tope inferior encontrado
                uint16_t posicionMinimaADC = (uint16_t)adcActual;
                Serial.printf("[MOTOR] Tope INFERIOR detectado: %d\n", posicionMinimaADC);

                // Validar — igual que CalibrationManager::validarCalibracion()
                if (_posicionMaximaADC > posicionMinimaADC + 100) {
                    _adcMin     = posicionMinimaADC + 20;
                    _adcMax     = _posicionMaximaADC - 20;
                    _adcSpan    = _adcMax - _adcMin;
                    _targetADC  = (uint16_t)_adcFiltered;
                    _calibState = Motor::CalibState::DONE;
                    _hwStop();
                    Serial.printf("[MOTOR] Calibrado OK  MIN=%d MAX=%d span=%d\n",
                                  _adcMin, _adcMax, _adcSpan);
                } else {
                    _hwStop();
                    _calibState = Motor::CalibState::ERROR;
                    Serial.printf("[MOTOR] Calibracion invalida max=%d min=%d\n",
                                  _posicionMaximaADC, posicionMinimaADC);
                }
            }
        }
    } else {
        // No estable — resetear timer (igual que CalibrationManager)
        _ultimoTiempoEstable = millis();
        _ultimoValorEstable  = adcActual;
    }
}

// ─── Control de posición ─────────────────────────────────────
static bool _motorActive = false;

static void _controlTick() {
    int pos    = (int)_adcFiltered;
    int err    = (int)_targetADC - pos;
    int absErr = abs(err);

    // Hysteresis
    if (!_motorActive && absErr > DEAD_ZONE) {
        _motorActive = true;
    } else if (_motorActive && absErr < DEAD_ZONE / 2) {
        _motorActive = false;
        _hwStop();
        return;
    }

    if (!_motorActive) { _hwStop(); return; }

    // Curva de velocidad
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

    // Slew rate
    int delta   = constrain(targetPWM - _currentPWM, -PWM_SLEW, PWM_SLEW);
    _currentPWM = constrain(_currentPWM + delta, PWM_MIN, PWM_MAX);

    if (err > 0) {
        _hwUp((uint8_t)_currentPWM);    // target > pos → subir
    } else {
        _hwDown((uint8_t)_currentPWM);  // target < pos → bajar
    }
}

// ─── API pública ─────────────────────────────────────────────
namespace Motor {

void init() {
    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    pinMode(MOTOR_EN,  OUTPUT);
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    digitalWrite(MOTOR_EN,  LOW);
    analogWriteFrequency(MOTOR_IN1, 20000);
    analogWriteFrequency(MOTOR_IN2, 20000);
}

void begin() {
    faderADC.update();
    _adcFiltered = (float)faderADC.getFaderPos();
    _calibState  = CalibState::IDLE;
    Serial.println("[MOTOR] Listo. Esperando orden de calibracion del S3.");
}

void startCalib() {
    if (_calibState == CalibState::CALIB_DOWN ||
        _calibState == CalibState::CALIB_UP) return;

    faderADC.update();
    _adcFiltered             = (float)faderADC.getFaderPos();
    _tiempoInicioCalibracion = millis();
    _ultimoTiempoEstable     = millis();
    _ultimoValorEstable      = (int)_adcFiltered;  // ← valor actual, no 0
    _calibState              = CalibState::CALIB_DOWN;

    _hwUp(CALIB_PWM);  // sube → busca tope superior primero
    Serial.println("[MOTOR] Calibracion iniciada — subiendo a tope superior.");
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

void stop() { _hwStop(); }

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