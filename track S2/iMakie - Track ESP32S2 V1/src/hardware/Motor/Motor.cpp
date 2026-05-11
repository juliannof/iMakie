#include "Motor.h"
#include "../../config.h"
#include <Preferences.h>
#include "../fader/FaderADC.h"

extern FaderADC faderADC;

// ────────────────────────────────────────────────────────────────
// Motor Control — iMakie PTxx Track S2
//
// Autoría: Julian Fuentes + Claude Haiku 4.5
// Rewrite completo: 2026-05-10
//
// Hardware: DRV8833 H-bridge (EN=GPIO14, IN1=GPIO18, IN2=GPIO16)
// Control: Calibración no-bloqueante + posición dead-zone + slew-rate
//
// Principios de diseño:
// - Orden crítico: configurar pins ANTES de habilitar EN
// - Estado SIEMPRE configurado ANTES de cambios HW
// - Todas las variables static en config.h (fuente única de verdad)
// - API pública limpia, no-bloqueante, con timeout global
// ────────────────────────────────────────────────────────────────

// Variables de estado en config.h (extern disponibles aquí via config.h)

// ─── PWM Range (lee de NVS, 0=inválido) ───────────────────────
static uint8_t _pwm_min = 0;
static uint8_t _pwm_max = 0;

// ─── Funciones HW (privadas) ──────────────────────────────────
static void _hwOff() {
    analogWrite(MOTOR_IN1, 0);
    analogWrite(MOTOR_IN2, 0);
    digitalWrite(MOTOR_EN, LOW);
}

static void _hwUp(uint8_t pwm) {
    analogWrite(MOTOR_IN1, pwm);
    analogWrite(MOTOR_IN2, 0);
    digitalWrite(MOTOR_EN, HIGH);
}

static void _hwDown(uint8_t pwm) {
    analogWrite(MOTOR_IN1, 0);
    analogWrite(MOTOR_IN2, pwm);
    digitalWrite(MOTOR_EN, HIGH);
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
    int      pos = (int)_motor_adcPos;

    if (now - _motor_calibStart > CALIB_TIMEOUT) {
        _motor_phase = CalibPhase::ERROR;
        _hwOff();
        log_e("[CALIB] TIMEOUT");
        return;
    }

    switch (_motor_phase) {

    case CalibPhase::KICK_UP:
        if (now - _motor_phaseStart >= CALIB_KICK_MS) {
            now = millis();  // Recapturar timestamp fresco
            _motor_phase       = CalibPhase::GOING_UP;
            _hwUp(_pwm_min);  // Movimiento controlado hacia el tope
            _motor_stableRef   = pos;
            _motor_stableStart = now;
            log_d("[CALIB] GOING_UP");
        }
        break;

    case CalibPhase::GOING_UP:
        if (now < _motor_calibMinDetect) break;
        if (abs(pos - _motor_stableRef) > ADC_STABILITY_THRESHOLD) {
            if (pos >= MOTOR_ADC_MIN && pos <= MOTOR_ADC_MAX) {
                _motor_stableRef   = pos;
                _motor_stableStart = now;
            }
        } else if (now - _motor_stableStart >= CALIB_STUCK_TIMEOUT) {
            // Motor no se movió UP → intentar DOWN inmediatamente
            log_e("[CALIB] Sin movimiento en GOING_UP (pos=%d) → intentando DOWN", pos);
            now = millis();  // Recapturar timestamp fresco
            _motor_calibMinDetect = now + CALIB_MIN_TRAVEL_MS;
            _motor_stableRef      = (int)_motor_adcPos;  // usar posición actual
            _motor_stableStart    = now;
            _motor_settleMin      = 27000;
            _motor_settleMax      = 0;
            _motor_phase          = CalibPhase::KICK_DOWN;
            _hwDown(_pwm_max);
            _motor_phaseStart     = now;
            log_i("[CALIB] KICK_DOWN iniciado desde GOING_UP");
        } else if (now - _motor_stableStart >= CALIB_STABLE_TIME) {
            now = millis();  // Recapturar timestamp fresco
            _motor_phase      = CalibPhase::SETTLE_UP;
            _hwOff();
            _motor_settleMin  = 27000;
            _motor_settleMax  = 0;
            _motor_phaseStart = now;
            log_d("[CALIB] SETTLE_UP  pos=%d", pos);
        }
        break;

    case CalibPhase::SETTLE_UP:
        if (_motor_adcPos < _motor_settleMin) _motor_settleMin = _motor_adcPos;
        if (_motor_adcPos > _motor_settleMax) _motor_settleMax = _motor_adcPos;

        if (now - _motor_phaseStart >= CALIB_SETTLE_MS) {
            _motor_adcTop       = _motor_adcPos;
            _motor_noiseTopSpan = _motor_settleMax - _motor_settleMin;
            log_i("[CALIB] Tope superior: %d  noise_span=%d", _motor_adcTop, _motor_noiseTopSpan);

            now = millis();  // Recapturar timestamp fresco
            _motor_calibMinDetect = now + CALIB_MIN_TRAVEL_MS;
            _motor_stableRef      = (int)_motor_adcTop;
            _motor_stableStart    = now;
            _motor_settleMin      = 27000;
            _motor_settleMax      = 0;
            _motor_phase          = CalibPhase::KICK_DOWN;
            _hwDown(_pwm_max);
            _motor_phaseStart     = now;
        }
        break;

    case CalibPhase::KICK_DOWN:
        if (now - _motor_phaseStart >= CALIB_KICK_MS) {
            now = millis();  // Recapturar timestamp fresco
            _motor_phase       = CalibPhase::GOING_DOWN;
            _hwDown(_pwm_min);  // Movimiento controlado hacia el tope
            _motor_stableRef   = pos;
            _motor_stableStart = now;
            log_d("[CALIB] GOING_DOWN");
        }
        break;

    case CalibPhase::GOING_DOWN:
        if (now < _motor_calibMinDetect) break;
        if (abs(pos - _motor_stableRef) > ADC_STABILITY_THRESHOLD) {
            if (pos >= MOTOR_ADC_MIN && pos <= MOTOR_ADC_MAX) {
                _motor_stableRef   = pos;
                _motor_stableStart = now;
            }
        } else if (now - _motor_stableStart >= CALIB_STUCK_TIMEOUT) {
            // Motor no se movió en tiempo máximo → ATASCADO
            _motor_phase = CalibPhase::ERROR;
            log_e("[CALIB] MOTOR BLOQUEADO — sin movimiento en GOING_DOWN (pos=%d)", pos);
            _hwOff();
        } else if (now - _motor_stableStart >= CALIB_STABLE_TIME) {
            now = millis();  // Recapturar timestamp fresco
            _motor_phase      = CalibPhase::SETTLE_DOWN;
            _hwOff();
            _motor_settleMin  = 27000;
            _motor_settleMax  = 0;
            _motor_phaseStart = now;
            log_d("[CALIB] SETTLE_DOWN  pos=%d", pos);
        }
        break;

    case CalibPhase::SETTLE_DOWN: {
        if (_motor_adcPos < _motor_settleMin) _motor_settleMin = _motor_adcPos;
        if (_motor_adcPos > _motor_settleMax) _motor_settleMax = _motor_adcPos;

        now = millis();  // Recapturar timestamp fresco
        if (now - _motor_phaseStart < CALIB_SETTLE_MS) break;

        uint16_t adcBot       = _motor_adcPos;
        _motor_noiseBottomSpan = _motor_settleMax - _motor_settleMin;

        uint16_t marginBot = max((uint16_t)(_motor_noiseBottomSpan * 2), (uint16_t)20);
        uint16_t marginTop = max((uint16_t)(_motor_noiseTopSpan * 2), (uint16_t)20);
        uint16_t minGapRequired = marginBot + marginTop;

        log_i("[CALIB] Tope inferior: %d  noise_span=%d  margin=%d", adcBot, _motor_noiseBottomSpan, marginBot);
        log_i("[CALIB] Tope superior: noise_span=%d  margin=%d", _motor_noiseTopSpan, marginTop);
        log_i("[CALIB] Gap requerido: %d (ruidos: top=%d bot=%d)", minGapRequired, _motor_noiseTopSpan, _motor_noiseBottomSpan);

        if (_motor_adcTop > adcBot + minGapRequired) {
            _motor_adcMin    = adcBot + marginBot;
            _motor_adcMax    = _motor_adcTop - marginTop;
            _motor_adcSpan   = _motor_adcMax - _motor_adcMin;
            _motor_targetADC = (uint16_t)map((long)_motor_lastMidiTarget,
                                        0, MIDI_PB_MAX, _motor_adcMin, _motor_adcMax);
            faderADC.setCalibration(_motor_adcMin, _motor_adcMax);  // Guardar calibración en FaderADC (2026-05-11 17:45)
            _motor_phase     = CalibPhase::DONE;
            log_i("[CALIB] OK  MIN=%d MAX=%d span=%d target=%d",
                  _motor_adcMin, _motor_adcMax, _motor_adcSpan, _motor_targetADC);
        } else {
            _motor_phase = CalibPhase::ERROR;
            log_e("[CALIB] ERROR — rango inválido  top=%d bot=%d", _motor_adcTop, adcBot);
        }
        break;
    }

    default: break;
    }
}

// ─── Control de posición ──────────────────────────────────────
static void _positionTick() {
    if (_motor_adcSpan == 0) {
        _hwOff();
        return;  // No calibrado
    }

    int pos    = (int)_motor_adcPos;
    int err    = (int)_motor_targetADC - pos;
    int absErr = abs(err);

    if (absErr < DEAD_ZONE) {
        if (_motor_active) {
            _motor_active = false;
            _hwOff();
            _motor_currentPWM = 0;
            log_d("[POS] OFF  pos=%d err=%d", pos, err);
        }
        return;
    }

    if (!_motor_active) {
        _motor_active = true;
        _motor_currentPWM  = 0;
        log_d("[POS] ON  pos=%d err=%d", pos, err);
    }

    int targetPWM = _pwm_min + (min((uint16_t)absErr, _motor_adcSpan) * (_pwm_max - _pwm_min)) / _motor_adcSpan;
    targetPWM = constrain(targetPWM, _pwm_min, _pwm_max);

    _motor_currentPWM = constrain(
        _motor_currentPWM + constrain(targetPWM - _motor_currentPWM, -PWM_SLEW, PWM_SLEW),
        0, _pwm_max);

    if (err > 0) _hwUp((uint8_t)_motor_currentPWM);
    else         _hwDown((uint8_t)_motor_currentPWM);
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

void initPWM() {
    // Lee pwmMin y pwmMax de NVS (2026-05-10 20:20)
    // Si no existen o son inválidos (==0), _pwm_min y _pwm_max quedan en 0 (error)
    Preferences prefs;
    prefs.begin("ptxx", true);
    uint8_t pwmMin = prefs.getUChar("pwmMin", 0);
    uint8_t pwmMax = prefs.getUChar("pwmMax", 0);
    prefs.end();

    if (pwmMin > 0 && pwmMax > 0 && pwmMin < pwmMax) {
        _pwm_min = pwmMin;
        _pwm_max = pwmMax;
        log_i("[MOTOR] PWM range: %u-%u (NVS)", _pwm_min, _pwm_max);
    } else {
        _pwm_min = 0;
        _pwm_max = 0;
        log_e("[MOTOR] PWM invalido en NVS: min=%u max=%u", pwmMin, pwmMax);
    }
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
    if (v < MOTOR_ADC_MIN || v > MOTOR_ADC_MAX) return;  // Rechazar fuera de rango esperado
    if (_motor_adcPos > 0 &&
        abs((int)v - (int)_motor_adcPos) > ADC_SPIKE_GUARD) return;
    _motor_adcPos = v;
}

void setTarget(uint16_t midiPB) {
    _motor_lastMidiTarget = midiPB;
    if (_motor_phase != CalibPhase::DONE) return;
    _motor_targetADC = (uint16_t)map((long)midiPB, 0, MIDI_PB_MAX, _motor_adcMin, _motor_adcMax);
    log_d("[TARGET] midi=%d → adc=%d", midiPB, _motor_targetADC);
}

void off() {
    _hwOff();
    _motor_active = false;
    _motor_currentPWM = 0;
}

void stop() {
    _hwOff();
    _motor_active = false;
    _motor_currentPWM = 0;
}

uint16_t getRawADC() {
    return _motor_adcPos;
}

float getPosition() {
    if (_motor_adcSpan == 0) return 0.0f;
    int pos = (int)_motor_adcPos - (int)_motor_adcMin;
    return constrain((float)pos / (float)_motor_adcSpan, 0.0f, 1.0f);
}

uint16_t getADCMin() {
    return _motor_adcMin;
}

uint16_t getADCMax() {
    return _motor_adcMax;
}

void startCalib() {
    if (_isCalibrating()) return;
    if (_motor_adcPos < MOTOR_ADC_MIN || _motor_adcPos > MOTOR_ADC_MAX) {
        log_e("[CALIB] Lectura ADC inválida: %d (esperado %d-%d)", _motor_adcPos, MOTOR_ADC_MIN, MOTOR_ADC_MAX);
        return;
    }
    uint32_t now = millis();
    _motor_active    = false;
    _motor_currentPWM     = 0;
    _motor_settleMin      = 27000;
    _motor_settleMax      = 0;
    _motor_noiseTopSpan   = 0;
    _motor_calibStart     = now;
    _motor_calibMinDetect = now + CALIB_MIN_TRAVEL_MS;
    _motor_stableRef      = (int)_motor_adcPos;
    _motor_stableStart    = now;
    _motor_phaseStart     = now;
    _motor_phase          = CalibPhase::KICK_UP;
    _hwUp(_pwm_max);
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
    return _motor_phase == CalibPhase::DONE && _motor_adcSpan > 100;
}

void testUp(uint8_t pwm) {
    _hwUp(pwm);
    log_d("[MOTOR-TEST] UP pwm=%d", pwm);
}

void testDown(uint8_t pwm) {
    _hwDown(pwm);
    log_d("[MOTOR-TEST] DOWN pwm=%d", pwm);
}

void testOff() {
    _hwOff();
    log_d("[MOTOR-TEST] OFF");
}

uint8_t getPWMMin() {
    return _pwm_min;
}

uint8_t getPWMMax() {
    return _pwm_max;
}

} // namespace Motor
