#include "Motor.h"
#include "../../fader/FaderADC.h"
#include "../../config.h"

extern FaderADC faderADC;

// ─── Hardware ────────────────────────────────────────────────
static void _hwStop() {
    digitalWrite(MOTOR_EN, HIGH);
    analogWrite (MOTOR_IN1, 255);
    analogWrite (MOTOR_IN2, 255);
}
static void _hwOff() {
    digitalWrite(MOTOR_EN, LOW);
    analogWrite (MOTOR_IN1, 0);
    analogWrite (MOTOR_IN2, 0);
}
static void _hwUp(uint8_t pwm) {
    digitalWrite(MOTOR_EN, HIGH);
    analogWrite (MOTOR_IN2, 0);
    analogWrite (MOTOR_IN1, pwm);
}
static void _hwDown(uint8_t pwm) {
    digitalWrite(MOTOR_EN, HIGH);
    analogWrite (MOTOR_IN1, 0);
    analogWrite (MOTOR_IN2, pwm);
}

// ─── Estado de calibración ───────────────────────────────────
enum class CalibPhase { IDLE, GOING_UP, GOING_DOWN, DONE, ERROR };
static CalibPhase _phase = CalibPhase::IDLE;

static uint32_t _stableStart = 0;
static int      _stableRef   = 0;
static uint16_t _adcTop      = 0;
static bool _hasMoved = false;

static bool _motorActive = false;  // ← aquí


// ─── Calibración ─────────────────────────────────────────────
static void _calibTick() {
    faderADC.update();
    int adc = (int)faderADC.getFaderPos();

    Serial.printf("[CALIB] %s  adc=%d\n",
        _phase == CalibPhase::GOING_UP ? "UP" : "DOWN", adc);

    // Si el ADC cambia significativamente, actualizar referencia
    if (abs(adc - _stableRef) > ADC_STABILITY_THRESHOLD) {
        _stableRef   = adc;
        _stableStart = millis();
        
        // Marcar que ha habido movimiento (solo la primera vez)
        if (!_hasMoved) {
            _hasMoved = true;
        }
        return;
    }

    // Si no ha habido movimiento aún, esperar
    if (!_hasMoved) return;
    
    // Si no ha pasado suficiente tiempo estable, esperar
    if (millis() - _stableStart < CALIB_STABLE_TIME) return;

    // Detección de tope
    if (_phase == CalibPhase::GOING_UP) {
        _adcTop      = (uint16_t)adc;
        _stableRef   = adc;
        _stableStart = millis();
        Serial.printf("[CALIB] Tope superior: %d\n", _adcTop);
        _phase = CalibPhase::GOING_DOWN;
        _hwDown(CALIB_KICK_PWM);
        delay(CALIB_KICK_MS);
        _hwDown(CALIB_PWM);

    } else {
        _hwStop();
        uint16_t adcBot = (uint16_t)adc;
        Serial.printf("[CALIB] Tope inferior: %d\n", adcBot);

        if (_adcTop > adcBot + 200) {
            _adcMin    = adcBot + 20;
            _adcMax    = _adcTop - 20;
            _adcSpan   = _adcMax - _adcMin;
            _targetADC = (uint16_t)adc;
            _phase     = CalibPhase::DONE;
            Serial.printf("[CALIB] OK  MIN=%d  MAX=%d  span=%d\n",
                          _adcMin, _adcMax, _adcSpan);
        } else {
            _phase = CalibPhase::ERROR;
            Serial.println("[CALIB] ERROR — rango invalido");
        }
    }
}


// ─── API pública ─────────────────────────────────────────────
namespace Motor {

void init() {
    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    pinMode(MOTOR_EN,  OUTPUT);
    analogWriteFrequency(MOTOR_IN1, 20000);
    analogWriteFrequency(MOTOR_IN2, 20000);
    _hwStop();
}

void begin() {
    faderADC.update();
    _adcFiltered = (float)faderADC.getFaderPos();
    _phase       = CalibPhase::IDLE;
}

void startCalib() {
    _hasMoved = false;

    if (_phase == CalibPhase::GOING_UP ||
        _phase == CalibPhase::GOING_DOWN) return;
    faderADC.update();
    _stableRef   = (int)faderADC.getFaderPos();
    _stableStart = millis();
    _phase       = CalibPhase::GOING_UP;
    _hwUp(CALIB_KICK_PWM);
    delay(CALIB_KICK_MS);
    _hwUp(CALIB_PWM);
    Serial.println("[CALIB] Iniciada");
}


//***************************************
// POSITION TICK
// **************************************

static void _positionTick() {
    int pos    = (int)_adcFiltered;
    int err    = (int)_targetADC - pos;
    int absErr = abs(err);
    
    static int lastErr = 0;
    static int stableCount = 0;
    
    //Serial.printf("[POS] pos=%d target=%d err=%d pwm=%d active=%d\n",
    //    pos, _targetADC, err, _currentPWM, _motorActive);

    // ─── HOLDING TORQUE CONTINUO ───────────────────────────
    if (absErr < HOLD_ZONE) {
    // Siempre aplicar holding torque cuando estamos cerca
    int holdPWM;
    
    if (absErr < DEAD_ZONE) {
        // Dentro de zona muerta: holding mínimo pero suficiente
        holdPWM = HOLD_MIN;  // Ahora 35, no 25
        stableCount++;
        
        // Solo desactivar después de MUCHO tiempo estable y error casi cero
        if (stableCount > 200 && absErr < 5) {  // Más ciclos y error muy pequeño
            _motorActive = false;
            _hwOff();
            _currentPWM = 0;
            return;
        }
    } else {
        // En zona de aproximación: holding proporcional
        holdPWM = HOLD_MIN + (absErr * HOLD_GAIN);
        holdPWM = constrain(holdPWM, HOLD_MIN, HOLD_MAX);
        stableCount = 0;
    }
    
    // Aplicar holding torque en dirección correcta
    if (err > 0) {
        _hwUp((uint8_t)holdPWM);
    } else {
        _hwDown((uint8_t)holdPWM);
    }
    _currentPWM = holdPWM;
    _motorActive = true;
    return;
}
    
    // ─── CONTROL NORMAL ────────────────────────────────────
    stableCount = 0;
    
    if (!_motorActive && absErr > DEAD_ZONE) {
        _motorActive = true;
    }
    
    if (!_motorActive) {
        _hwOff();
        return;
    }

    // Calcular PWM (con ganancia proporcional)
    int targetPWM;
    int maxError = 1000;  // Error para PWM máximo
    
    if (absErr >= maxError) {
        targetPWM = PWM_MAX;
    } else {
        // Fórmula lineal simple
        targetPWM = PWM_MIN + (absErr * (PWM_MAX - PWM_MIN)) / maxError;
    }
    
    targetPWM = constrain(targetPWM, PWM_MIN, PWM_MAX);

    // Slew rate
    int delta = targetPWM - _currentPWM;
    if (delta > PWM_SLEW) delta = PWM_SLEW;
    if (delta < -PWM_SLEW) delta = -PWM_SLEW;
    _currentPWM += delta;
    _currentPWM = constrain(_currentPWM, 0, PWM_MAX);

    // Aplicar dirección
    if (err > 0) {
        _hwUp((uint8_t)_currentPWM);
    } else {
        _hwDown((uint8_t)_currentPWM);
    }
}

// **********************************
// UPDATE PRINCIPAL (LLAMAR EN LOOP)
// **********************************

void update() {
    if (_phase == CalibPhase::GOING_UP ||
        _phase == CalibPhase::GOING_DOWN) {
        _calibTick();
    } else if (_phase == CalibPhase::DONE) {
        _positionTick();
    } else {
        _hwStop();
    }
}

void setADC(uint16_t v) { _adcFiltered = (float)v; }

void stop() {
    if (_phase == CalibPhase::GOING_UP ||
        _phase == CalibPhase::GOING_DOWN) return;
    _hwStop();
}

void setTarget(uint16_t midiTarget) {
    if (_phase != CalibPhase::DONE) return;
    _targetADC = (uint16_t)map((long)midiTarget, 0, 14848, _adcMin, _adcMax);
    Serial.printf("[TARGET] midi=%d → adc=%d (min=%d max=%d)\n", 
                  midiTarget, _targetADC, _adcMin, _adcMax);
}

Motor::CalibState getCalibState() {
    switch (_phase) {
        case CalibPhase::GOING_UP:   return CalibState::CALIB_DOWN;
        case CalibPhase::GOING_DOWN: return CalibState::CALIB_UP;
        case CalibPhase::DONE:       return CalibState::DONE;
        case CalibPhase::ERROR:      return CalibState::ERROR;
        default:                     return CalibState::IDLE;
    }
}

bool     isCalibrated() { return _phase == CalibPhase::DONE; }
uint16_t getADCMin()    { return _adcMin; }
uint16_t getADCMax()    { return _adcMax; }
uint16_t getRawADC()    { return (uint16_t)_adcFiltered; }
float    getPosition()  {
    if (_adcSpan == 0) return 0.0f;
    return constrain((_adcFiltered - _adcMin) / (float)_adcSpan, 0.0f, 1.0f);
}
void driveRaw(int pwm) {
    if (pwm == 0) { _hwStop(); return; }
    if (pwm > 0) _hwUp((uint8_t)constrain( pwm, 0, 255));
    else         _hwDown((uint8_t)constrain(-pwm, 0, 255));
}

} // namespace Motor