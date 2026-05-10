#include "Motor.h"
#include "../../config.h"

// Variables de estado en config.h (extern disponibles aquí via config.h)

// ─── Funciones HW (privadas) ──────────────────────────────────
static void _hwOff() {
    analogWrite(MOTOR_IN1, 0);
    analogWrite(MOTOR_IN2, 0);
    digitalWrite(MOTOR_EN, LOW);
}

static void _hwUp(uint8_t pwm) {
    digitalWrite(MOTOR_EN, HIGH);
    analogWrite(MOTOR_IN1, pwm);
    analogWrite(MOTOR_IN2, 0);
}

static void _hwDown(uint8_t pwm) {
    digitalWrite(MOTOR_EN, HIGH);
    analogWrite(MOTOR_IN1, 0);
    analogWrite(MOTOR_IN2, pwm);
}

// ─── Helper ───────────────────────────────────────────────────
static bool _isCalibrating() {
    return _motor_phase != CalibPhase::DONE  &&
           _motor_phase != CalibPhase::IDLE  &&
           _motor_phase != CalibPhase::ERROR;
}

// ─── Calibración no-bloqueante ────────────────────────────────
static void _calibUpdate() {
    uint32_t now = millis();
    int      pos = (int)_adcPos;

    if (now - _calibStart > CALIB_TIMEOUT) {
        _hwOff();
        _motor_phase = CalibPhase::ERROR;
        log_e("[CALIB] TIMEOUT");
        return;
    }

    switch (_motor_phase) {

    case CalibPhase::KICK_UP:
        if (now - _motor_phaseStart >= CALIB_KICK_MS) {
            _hwUp(PWM_MAX);
            _stableRef   = pos;
            _stableStart = now;
            _motor_phase       = CalibPhase::GOING_UP;
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
            _motor_phaseStart = now;
            _motor_phase      = CalibPhase::SETTLE_UP;
            log_d("[CALIB] SETTLE_UP  pos=%d", pos);
        }
        break;

    case CalibPhase::SETTLE_UP:
        if (_adcPos < _settleMin) _settleMin = _adcPos;
        if (_adcPos > _settleMax) _settleMax = _adcPos;

        if (now - _motor_phaseStart >= CALIB_SETTLE_MS) {
            _adcTop       = _adcPos;
            _noiseTopSpan = _settleMax - _settleMin;
            log_i("[CALIB] Tope superior: %d  noise_span=%d", _adcTop, _noiseTopSpan);

            _calibMinDetect = now + CALIB_MIN_TRAVEL_MS;
            _stableRef      = (int)_adcTop;
            _stableStart    = now;
            _settleMin      = 8191;
            _settleMax      = 0;
            _hwDown(PWM_MAX);
            _motor_phaseStart     = now;
            _motor_phase          = CalibPhase::KICK_DOWN;
        }
        break;

    case CalibPhase::KICK_DOWN:
        if (now - _motor_phaseStart >= CALIB_KICK_MS) {
            _hwDown(PWM_MAX);
            _stableRef   = pos;
            _stableStart = now;
            _motor_phase       = CalibPhase::GOING_DOWN;
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
            _motor_phaseStart = now;
            _motor_phase      = CalibPhase::SETTLE_DOWN;
            log_d("[CALIB] SETTLE_DOWN  pos=%d", pos);
        }
        break;

    case CalibPhase::SETTLE_DOWN: {
        if (_adcPos < _settleMin) _settleMin = _adcPos;
        if (_adcPos > _settleMax) _settleMax = _adcPos;

        if (now - _motor_phaseStart < CALIB_SETTLE_MS) break;

        uint16_t adcBot       = _adcPos;
        uint16_t noiseSpanBot = _settleMax - _settleMin;

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
            _motor_phase     = CalibPhase::DONE;
            log_i("[CALIB] OK  MIN=%d MAX=%d span=%d target=%d",
                  _adcMin, _adcMax, _adcSpan, _targetADC);
        } else {
            _motor_phase = CalibPhase::ERROR;
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
    log_i("[MOTOR] init(): configurando pines y PWM");

    pinMode(MOTOR_EN, OUTPUT);
    digitalWrite(MOTOR_EN, LOW);
    log_i("[MOTOR] GPIO%d (EN) LOW", MOTOR_EN);

    pinMode(MOTOR_IN1, OUTPUT);
    analogWriteFrequency(MOTOR_IN1, 20000);
    analogWriteResolution(MOTOR_IN1, 8);
    analogWrite(MOTOR_IN1, 0);
    log_i("[MOTOR] GPIO%d (IN1) 20kHz 8-bit", MOTOR_IN1);

    pinMode(MOTOR_IN2, OUTPUT);
    analogWriteFrequency(MOTOR_IN2, 20000);
    analogWriteResolution(MOTOR_IN2, 8);
    analogWrite(MOTOR_IN2, 0);
    log_i("[MOTOR] GPIO%d (IN2) 20kHz 8-bit", MOTOR_IN2);

    _hwOff();
    log_i("[MOTOR] init COMPLETE");
}

void update() {
    if (_isCalibrating()) {
        _calibUpdate();
    } else if (_motor_phase == CalibPhase::DONE) {
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
    if (_motor_phase != CalibPhase::DONE) return;
    _targetADC = (uint16_t)map((long)midiPB, 0, MIDI_PB_MAX, _adcMin, _adcMax);
    log_d("[TARGET] midi=%d → adc=%d", midiPB, _targetADC);
}

void off() {
    _hwOff();
    _motorActive = false;
    _currentPWM = 0;
}

void stop() {
    _hwOff();
    _motorActive = false;
    _currentPWM = 0;
}

uint16_t getRawADC() {
    return _adcPos;
}

float getPosition() {
    if (_adcSpan == 0) return 0.0f;
    int pos = (int)_adcPos - (int)_adcMin;
    return constrain((float)pos / (float)_adcSpan, 0.0f, 1.0f);
}

uint16_t getADCMin() {
    return _adcMin;
}

uint16_t getADCMax() {
    return _adcMax;
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
    _motor_phaseStart     = millis();
    _hwUp(PWM_MAX);
    _motor_phase          = CalibPhase::KICK_UP;
    log_i("[CALIB] Iniciada");
}

CalibState getCalibState() {
    switch (_motor_phase) {
        case CalibPhase::KICK_UP:
        case CalibPhase::GOING_UP:
        case CalibPhase::SETTLE_UP:
            return CalibState::CALIB_UP;
        case CalibPhase::KICK_DOWN:
        case CalibPhase::GOING_DOWN:
        case CalibPhase::SETTLE_DOWN:
            return CalibState::CALIB_DOWN;
        case CalibPhase::DONE:
            return CalibState::DONE;
        case CalibPhase::ERROR:
            return CalibState::ERROR;
        default:
            return CalibState::IDLE;
    }
}

bool isCalibrated() {
    return _motor_phase == CalibPhase::DONE && _adcSpan > 100;
}

} // namespace Motor
