#include "Motor.h"
#include "../../config.h"

// ─── Hardware — sin lógica de estado ──────────────────────────
static void _hwBrake() {
    digitalWrite(MOTOR_EN, HIGH);
    analogWrite(MOTOR_IN1, 255);
    analogWrite(MOTOR_IN2, 255);
}
static void _hwOff() {
    digitalWrite(MOTOR_EN, LOW);
    analogWrite(MOTOR_IN1, 0);
    analogWrite(MOTOR_IN2, 0);
}
static void _hwUp(uint8_t pwm) {
    digitalWrite(MOTOR_EN, HIGH);
    analogWrite(MOTOR_IN2, 0);
    analogWrite(MOTOR_IN1, pwm);
}
static void _hwDown(uint8_t pwm) {
    digitalWrite(MOTOR_EN, HIGH);
    analogWrite(MOTOR_IN1, 0);
    analogWrite(MOTOR_IN2, pwm);
}

// ─── Máquina de calibración ───────────────────────────────────
//
//  Flujo:
//    KICK_UP → GOING_UP → SETTLE_UP
//    → KICK_DOWN → GOING_DOWN → SETTLE_DOWN → DONE
//
//  Los estados KICK_* esperan CALIB_KICK_MS con PWM alto
//  sin bloquear el loop — reemplazan el delay(CALIB_KICK_MS).
//
//  Los estados SETTLE_* esperan CALIB_SETTLE_MS con motor parado
//  mientras setADC() sigue recibiendo valores frescos de FaderADC
//  — reemplazan el delay(80) + faderADC.update() del código anterior.
//
enum class CalibPhase {
    IDLE,
    KICK_UP, GOING_UP, SETTLE_UP,
    KICK_DOWN, GOING_DOWN, SETTLE_DOWN,
    DONE, ERROR
};

static CalibPhase _phase          = CalibPhase::IDLE;
static uint32_t   _phaseStart     = 0;   // timestamp de entrada a la fase actual
static uint32_t   _calibStart     = 0;   // timestamp inicio calibración (timeout global)
static uint32_t   _calibMinDetect = 0;   // no buscar tope antes de este instante
static uint32_t   _stableStart    = 0;   // inicio del período estable actual
static int        _stableRef      = 0;   // referencia para detectar movimiento

static uint16_t   _adcTop         = 0;
static uint16_t   _adcMin         = 0;
static uint16_t   _adcMax         = 8191;
static uint16_t   _adcSpan        = 8191;
static uint16_t   _adcPos         = 0;   // valor actual — recibido vía setADC()
static uint16_t   _targetADC      = 0;
static uint16_t   _lastMidiTarget = 0;

static bool       _motorActive    = false;
static int        _currentPWM     = 0;

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
            _hwUp(CALIB_PWM);
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
            _phaseStart = now;
            _phase      = CalibPhase::SETTLE_UP;
            log_d("[CALIB] SETTLE_UP  pos=%d", pos);
        }
        break;

    case CalibPhase::SETTLE_UP:
        if (now - _phaseStart >= CALIB_SETTLE_MS) {
            _adcTop         = _adcPos;
            _calibMinDetect = now + CALIB_MIN_TRAVEL_MS;
            _stableRef      = (int)_adcTop;
            _stableStart    = now;
            _hwDown(CALIB_KICK_PWM);
            _phaseStart     = now;
            _phase          = CalibPhase::KICK_DOWN;
            log_i("[CALIB] Tope superior: %d", _adcTop);
        }
        break;

    case CalibPhase::KICK_DOWN:
        if (now - _phaseStart >= CALIB_KICK_MS) {
            _hwDown(CALIB_PWM);
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
            _phaseStart = now;
            _phase      = CalibPhase::SETTLE_DOWN;
            log_d("[CALIB] SETTLE_DOWN  pos=%d", pos);
        }
        break;

    case CalibPhase::SETTLE_DOWN: {
        if (now - _phaseStart < CALIB_SETTLE_MS) break;
        uint16_t adcBot = _adcPos;
        log_i("[CALIB] Tope inferior: %d", adcBot);
        if (_adcTop > adcBot + 200) {
            _adcMin    = adcBot + 20;
            _adcMax    = _adcTop - 20;
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
    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    pinMode(MOTOR_EN,  OUTPUT);
    analogWriteFrequency(MOTOR_IN1, 20000);
    analogWriteFrequency(MOTOR_IN2, 20000);
    analogWriteResolution(MOTOR_IN1, 8);
    analogWriteResolution(MOTOR_IN2, 8);
    _hwBrake();
    log_i("[MOTOR] init OK  IN1=%d IN2=%d EN=%d", MOTOR_IN1, MOTOR_IN2, MOTOR_EN);
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
    _calibStart     = millis();
    _calibMinDetect = millis() + CALIB_MIN_TRAVEL_MS;
    _stableRef      = (int)_adcPos;
    _stableStart    = millis();
    _phaseStart     = millis();
    _hwUp(CALIB_KICK_PWM);
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
    // Spike guard: descarta saltos imposibles fuera de calibración
    // (durante calib el fader se mueve rápido — cualquier delta es válido)
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

// Freno activo — usar cuando el usuario toca el fader
void stop() {
    if (_isCalibrating()) return;
    _hwBrake();
    _motorActive = false;
    _currentPWM  = 0;
}

// Corte total — usar en desconexión o apagado de emergencia
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

} // namespace Motor