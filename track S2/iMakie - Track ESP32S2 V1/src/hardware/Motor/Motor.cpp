#include "Motor.h"
#include "../../config.h"

// ─── Hardware — analogWrite (TEMPORAL: LEDC fallaba) ──────────────────
static void _hwBrake() {
    log_i("[HW] BRAKE - EN=HIGH IN1=255 IN2=255");
    digitalWrite(MOTOR_EN, HIGH);
    analogWrite(MOTOR_IN1, 255);
    log_d("[HW] analogWrite IN1=255 OK");
    analogWrite(MOTOR_IN2, 255);
    log_d("[HW] analogWrite IN2=255 OK");
}
static void _hwOff() {
    log_d("[HW] OFF - EN=LOW IN1=0 IN2=0");
    digitalWrite(MOTOR_EN, LOW);
    analogWrite(MOTOR_IN1, 0);
    analogWrite(MOTOR_IN2, 0);
}
static void _hwUp(uint8_t pwm) {
    log_d("[HW] UP - EN=HIGH IN1=0 IN2=%d (cables invertidos)", pwm);
    digitalWrite(MOTOR_EN, HIGH);
    analogWrite(MOTOR_IN1, 0);
    analogWrite(MOTOR_IN2, pwm);
}
static void _hwDown(uint8_t pwm) {
    log_d("[HW] DOWN - EN=HIGH IN1=%d IN2=0 (cables invertidos)", pwm);
    digitalWrite(MOTOR_EN, HIGH);
    analogWrite(MOTOR_IN1, pwm);
    analogWrite(MOTOR_IN2, 0);
}



// ─── Helper ───────────────────────────────────────────────────
static bool _isCalibrating() {
    return _phase != CalibPhase::DONE  &&
           _phase != CalibPhase::IDLE  &&
           _phase != CalibPhase::ERROR;
}

// ─── Calibración no-bloqueante ────────────────────────────────
static void _calibUpdate() {
    uint32_t now = millis();
    int      pos = (int)_adcPos;

    if (now - _calibStart > CALIB_TIMEOUT) {
        _hwOff();
        _phase = CalibPhase::ERROR;
        log_e("[CALIB] TIMEOUT");
        return;
    }

    switch (_phase) {

    case CalibPhase::KICK_UP:
        if (now - _phaseStart >= CALIB_KICK_MS) {
            _hwUp(PWM_MAX);
            _stableRef   = pos;
            _stableStart = now;
            _phase       = CalibPhase::GOING_UP;
            log_d("[CALIB] GOING_UP");
        }
        break;

    case CalibPhase::GOING_UP:
        if (now < _calibMinDetect) break;
        if (abs(pos - _stableRef) > ADC_STABILITY_THRESHOLD) {
            _stableRef   = pos;
            _stableStart = now;
        } else if (now - _stableStart >= CALIB_STABLE_TIME) {
            _hwOff();
            _settleMin  = 8191;
            _settleMax  = 0;
            _phaseStart = now;
            _phase      = CalibPhase::SETTLE_UP;
            log_d("[CALIB] SETTLE_UP  pos=%d", pos);
        }
        break;

    case CalibPhase::SETTLE_UP:
        // Acumular noise durante el periodo de asentamiento
        if (_adcPos < _settleMin) _settleMin = _adcPos;
        if (_adcPos > _settleMax) _settleMax = _adcPos;

        if (now - _phaseStart >= CALIB_SETTLE_MS) {
            _adcTop       = _adcPos;
            _noiseTopSpan = _settleMax - _settleMin;
            log_i("[CALIB] Tope superior: %d  noise_span=%d", _adcTop, _noiseTopSpan);

            _calibMinDetect = now + CALIB_MIN_TRAVEL_MS;
            _stableRef      = (int)_adcTop;
            _stableStart    = now;
            _settleMin      = 8191;   // reset para SETTLE_DOWN
            _settleMax      = 0;
            _hwDown(PWM_MAX);
            _phaseStart     = now;
            _phase          = CalibPhase::KICK_DOWN;
        }
        break;

    case CalibPhase::KICK_DOWN:
        if (now - _phaseStart >= CALIB_KICK_MS) {
            _hwDown(PWM_MAX);
            _stableRef   = pos;
            _stableStart = now;
            _phase       = CalibPhase::GOING_DOWN;
            log_d("[CALIB] GOING_DOWN");
        }
        break;

    case CalibPhase::GOING_DOWN:
        if (now < _calibMinDetect) break;
        if (abs(pos - _stableRef) > ADC_STABILITY_THRESHOLD) {
            _stableRef   = pos;
            _stableStart = now;
        } else if (now - _stableStart >= CALIB_STABLE_TIME) {
            _hwOff();
            _settleMin  = 8191;
            _settleMax  = 0;
            _phaseStart = now;
            _phase      = CalibPhase::SETTLE_DOWN;
            log_d("[CALIB] SETTLE_DOWN  pos=%d", pos);
        }
        break;

    case CalibPhase::SETTLE_DOWN: {
        // Acumular noise durante el periodo de asentamiento
        if (_adcPos < _settleMin) _settleMin = _adcPos;
        if (_adcPos > _settleMax) _settleMax = _adcPos;

        if (now - _phaseStart < CALIB_SETTLE_MS) break;

        uint16_t adcBot       = _adcPos;
        uint16_t noiseSpanBot = _settleMax - _settleMin;

        // Margen = 2× noise span medido en cada tope, mínimo 20 raw
        uint16_t marginBot = max((uint16_t)(noiseSpanBot * 2), (uint16_t)20);
        uint16_t marginTop = max((uint16_t)(_noiseTopSpan * 2), (uint16_t)20);

        log_i("[CALIB] Tope inferior: %d  noise_span=%d  margin=%d", adcBot, noiseSpanBot, marginBot);
        log_i("[CALIB] Tope superior: noise_span=%d  margin=%d", _noiseTopSpan, marginTop);

        if (_adcTop > adcBot + 200) {
            _adcMin    = adcBot + marginBot;
            _adcMax    = _adcTop - marginTop;
            _adcSpan   = _adcMax - _adcMin;
            _targetADC = (uint16_t)map((long)_lastMidiTarget,
                                        0, MIDI_PB_MAX, _adcMin, _adcMax);
            _phase     = CalibPhase::DONE;
            log_i("[CALIB] OK  MIN=%d MAX=%d span=%d target=%d",
                  _adcMin, _adcMax, _adcSpan, _targetADC);
        } else {
            _phase = CalibPhase::ERROR;
            log_e("[CALIB] ERROR — rango inválido  top=%d bot=%d", _adcTop, adcBot);
        }
        break;
    }

    default: break;
    }
}

// ─── Control de posición ──────────────────────────────────────
static void _positionTick() {
    int pos    = (int)_adcPos;
    int err    = (int)_targetADC - pos;
    int absErr = abs(err);

    log_i("[POS] pos=%d target=%d err=%d active=%d", pos, _targetADC, err, _motorActive);  // ← temporal

    if (absErr < DEAD_ZONE) {
        if (_motorActive) {
            _motorActive = false;
            _hwOff();
            _currentPWM = 0;
            log_d("[POS] OFF  pos=%d err=%d", pos, err);
        }
        return;
    }

    if (!_motorActive) {
        _motorActive = true;
        _currentPWM  = 0;
        log_d("[POS] ON  pos=%d err=%d", pos, err);
    }

    int targetPWM = PWM_MIN + (min(absErr, 1000) * (PWM_MAX - PWM_MIN)) / 1000;
    targetPWM = constrain(targetPWM, PWM_MIN, PWM_MAX);

    _currentPWM = constrain(
        _currentPWM + constrain(targetPWM - _currentPWM, -PWM_SLEW, PWM_SLEW),
        0, PWM_MAX);

    if (err > 0) _hwUp((uint8_t)_currentPWM);
    else         _hwDown((uint8_t)_currentPWM);
}

// ─── API pública ──────────────────────────────────────────────
namespace Motor {

void init() {
    // TEMPORAL: Revertir a analogWrite (LEDC fallaba silenciosamente)
    // ORDEN CRÍTICO: pinMode → frequency/resolution → LUEGO analogWrite
    log_i("[MOTOR] init(): iniciando configuración analogWrite (TEMPORAL)");

    pinMode(MOTOR_EN,  OUTPUT);
    log_i("[MOTOR] pinMode EN (GPIO%d) OK", MOTOR_EN);

    pinMode(MOTOR_IN1, OUTPUT);
    log_i("[MOTOR] pinMode IN1 (GPIO%d) OK", MOTOR_IN1);

    pinMode(MOTOR_IN2, OUTPUT);
    log_i("[MOTOR] pinMode IN2 (GPIO%d) OK", MOTOR_IN2);

    // Estado de seguridad: EN siempre LOW hasta que se habilite
    digitalWrite(MOTOR_EN, LOW);
    log_i("[MOTOR] digitalWrite EN=LOW OK");

    // Configurar frecuencia ANTES de analogWrite
    log_i("[MOTOR] Configurando analogWriteFrequency IN1/IN2 a 20kHz...");
    analogWriteFrequency(MOTOR_IN1, 20000);
    analogWriteFrequency(MOTOR_IN2, 20000);
    log_i("[MOTOR] analogWriteFrequency OK");

    log_i("[MOTOR] Configurando analogWriteResolution IN1/IN2 a 8-bit...");
    analogWriteResolution(MOTOR_IN1, 8);
    analogWriteResolution(MOTOR_IN2, 8);
    log_i("[MOTOR] analogWriteResolution OK");

    // Inicializar duty cycle en 0
    log_i("[MOTOR] Initializing duty cycles to 0");
    analogWrite(MOTOR_IN1, 0);
    analogWrite(MOTOR_IN2, 0);
    log_i("[MOTOR] analogWrite IN1=0, IN2=0 OK");

    _hwOff();
    log_i("[MOTOR] init COMPLETE - analogWrite (TEMPORAL) - EN=%d IN1=%d IN2=%d freq=20kHz",
          MOTOR_EN, MOTOR_IN1, MOTOR_IN2);
}



void begin() {
    _phase       = CalibPhase::IDLE;
    _motorActive = false;
    _currentPWM  = 0;
}

void startCalib() {
    if (_isCalibrating()) return;
    _motorActive    = false;
    _currentPWM     = 0;
    _settleMin      = 8191;
    _settleMax      = 0;
    _noiseTopSpan   = 0;
    _calibStart     = millis();
    _calibMinDetect = millis() + CALIB_MIN_TRAVEL_MS;
    _stableRef      = (int)_adcPos;
    _stableStart    = millis();
    _phaseStart     = millis();
    _hwUp(PWM_MAX);
    _phase          = CalibPhase::KICK_UP;
    log_i("[CALIB] Iniciada");
}

void update() {
    if (_isCalibrating()) {
        _calibUpdate();
    } else if (_phase == CalibPhase::DONE) {
        _positionTick();
    } else {
        _hwOff();
    }
}

void setADC(uint16_t v) {
    if (!_isCalibrating() &&
        _adcPos > 0 &&
        abs((int)v - (int)_adcPos) > ADC_SPIKE_GUARD) return;
    _adcPos = v;
}

void setTarget(uint16_t midiPB) {
    _lastMidiTarget = midiPB;
    if (_phase != CalibPhase::DONE) return;
    _targetADC = (uint16_t)map((long)midiPB, 0, MIDI_PB_MAX, _adcMin, _adcMax);
    log_d("[TARGET] midi=%d → adc=%d", midiPB, _targetADC);
}

void stop() {
    if (_isCalibrating()) return;
    _hwBrake();
    _motorActive = false;
    _currentPWM  = 0;
}

void off() {
    _hwOff();
    _motorActive = false;
    _currentPWM  = 0;
    if (_isCalibrating()) {
        _phase = CalibPhase::IDLE;
        log_i("[MOTOR] off() — calibración abortada");
    }
}

CalibState getCalibState() {
    switch (_phase) {
    case CalibPhase::KICK_UP:
    case CalibPhase::GOING_UP:
    case CalibPhase::SETTLE_UP:   return CalibState::CALIB_UP;
    case CalibPhase::KICK_DOWN:
    case CalibPhase::GOING_DOWN:
    case CalibPhase::SETTLE_DOWN: return CalibState::CALIB_DOWN;
    case CalibPhase::DONE:        return CalibState::DONE;
    case CalibPhase::ERROR:       return CalibState::ERROR;
    default:                      return CalibState::IDLE;
    }
}

bool     isCalibrated() { return _phase == CalibPhase::DONE; }
uint16_t getADCMin()    { return _adcMin; }
uint16_t getADCMax()    { return _adcMax; }
uint16_t getRawADC()    { return _adcPos; }

float getPosition() {
    if (_adcSpan == 0) return 0.0f;
    return constrain(((float)_adcPos - _adcMin) / (float)_adcSpan, 0.0f, 1.0f);
}

void driveRaw(int pwm) {
    if (pwm == 0)  { _hwOff(); return; }
    if (pwm > 0)   _hwUp((uint8_t)constrain( pwm, 0, 255));
    else           _hwDown((uint8_t)constrain(-pwm, 0, 255));
}

void testUpDown() {
    static uint32_t testStartTime = 0;
    static int testPhase = 0;  // 0 = UP for 1s, 1 = DOWN for 1s

    if (testStartTime == 0) {
        testStartTime = millis();
    }

    uint32_t elapsed = millis() - testStartTime;
    uint32_t cycleTime = elapsed % 6000;  // 6s cycle: 3s UP + 3s DOWN

    if (cycleTime < 3000) {
        _hwUp(PWM_MIN);
        if (testPhase != 0) {
            testPhase = 0;
            log_i("[TEST] UP — PWM_MIN=%d en IN2", PWM_MIN);
        }
    } else {
        _hwDown(PWM_MIN);
        if (testPhase != 1) {
            testPhase = 1;
            log_i("[TEST] DOWN — PWM_MIN=%d en IN1", PWM_MIN);
        }
    }
}

} // namespace Motor