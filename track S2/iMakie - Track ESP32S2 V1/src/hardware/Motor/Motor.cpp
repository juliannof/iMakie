#include "Motor.h"
#include "hardware/fader/FaderADC.h" 
#include "../../config.h"

extern FaderADC faderADC;

// ─── Hardware ────────────────────────────────────────────────
static void _hwStop() {
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

enum class CalibPhase { IDLE, GOING_UP, GOING_DOWN, DONE, ERROR };
static CalibPhase _phase             = CalibPhase::IDLE;
static uint32_t   _stableStart       = 0;
static int        _stableRef         = 0;
static uint16_t   _adcTop            = 0;
static uint32_t   _calibStart        = 0;
static uint32_t   _calibMinDetectTime = 0;
static uint16_t   _lastMidiTarget    = 0;
static bool       _motorActive       = false;
static uint32_t   _motorOffTime      = 0;
static float      _adcFiltered       = 0.0f;
static float      _adcControl        = 0.0f;
static uint16_t   _adcMin            = 0;
static uint16_t   _adcMax            = 8191;
static uint16_t   _adcSpan           = 8191;
static uint16_t   _targetADC         = 0;
static int        _currentPWM        = 0;

// ─── Calibración ─────────────────────────────────────────────

static void _calibTick() {
    int adc = (int)_adcFiltered;

    log_d("[CALIB] %s  adc=%d  estable_ms=%lu",
        _phase == CalibPhase::GOING_UP ? "UP" : "DOWN",
        adc, millis() - _stableStart);

    if (millis() - _calibStart > CALIB_TIMEOUT) {
        _hwStop();
        _phase = CalibPhase::ERROR;
        log_e("[CALIB] TIMEOUT");
        return;
    }

    // Tiempo mínimo de viaje — evita detectar tope si fader ya estaba en posición
    if (millis() < _calibMinDetectTime) return;

    if (abs(adc - _stableRef) > ADC_STABILITY_THRESHOLD) {
        _stableRef   = adc;
        _stableStart = millis();
        return;
    }

    if (millis() - _stableStart < CALIB_STABLE_TIME) return;

    if (_phase == CalibPhase::GOING_UP) {
        _hwOff();
        delay(80);
        faderADC.update();
        faderADC.update();
        _adcTop      = faderADC.getFaderPos();
        _stableRef   = (int)_adcTop;
        _stableStart = millis();
        _calibMinDetectTime = millis() + 800;  // mínimo 800ms bajando
        log_i("[CALIB] Tope superior: %d", _adcTop);
        _phase = CalibPhase::GOING_DOWN;
        _hwDown(CALIB_KICK_PWM);
        delay(CALIB_KICK_MS);
        _hwDown(CALIB_PWM);

    } else {  // GOING_DOWN
        _hwOff();
        delay(80);
        faderADC.update();
        faderADC.update();
        uint16_t adcBot = faderADC.getFaderPos();
        log_i("[CALIB] Tope inferior: %d", adcBot);

        if (_adcTop > adcBot + 200) {
            _adcMin      = adcBot + 20;
            _adcMax      = _adcTop - 20;
            _adcSpan     = _adcMax - _adcMin;
            _targetADC   = (uint16_t)map((long)_lastMidiTarget, 0, 14848, _adcMin, _adcMax);
            _adcFiltered = (float)adcBot;  // posición real → el motor verá el error y se moverá
            _adcControl  = (float)adcBot;   // ← inicializar filtro lento también
            _phase       = CalibPhase::DONE;
            log_i("[CALIB] OK  MIN=%d  MAX=%d  span=%d  target=%d",
                  _adcMin, _adcMax, _adcSpan, _targetADC);
        } else {
            _phase = CalibPhase::ERROR;
            log_e("[CALIB] ERROR — rango invalido");
        }
    }
}

// ─── Posición ─────────────────────────────────────────────

static void _positionTick() {
    int pos    = (int)_adcControl;   // ← lento, filtra el ruido
    int err    = (int)_targetADC - pos;
    int absErr = abs(err);

    log_d("[POS] pos=%d target=%d err=%d pwm=%d",
          pos, _targetADC, err, _currentPWM);

    // Zona muerta — apagar y listo
    if (absErr < DEAD_ZONE) {
        if (_motorActive) {
            _motorActive = false;
            _hwOff();
            _currentPWM = 0;
            log_i("[POS] Motor OFF  pos=%d target=%d err=%d", pos, _targetADC, err);
        }
        return;
    }

    // Encender con histéresis
    if (!_motorActive) {
        if (absErr < DEAD_ZONE) return;
        _motorActive = true;
        _currentPWM  = 0;
        log_i("[POS] Motor ON  pos=%d target=%d err=%d", pos, _targetADC, err);
    }

    // PWM proporcional
    int targetPWM = PWM_MIN + (min(absErr, 1000) * (PWM_MAX - PWM_MIN)) / 1000;
    targetPWM = constrain(targetPWM, PWM_MIN, PWM_MAX);

    int delta = constrain(targetPWM - _currentPWM, -PWM_SLEW, PWM_SLEW);
    _currentPWM = constrain(_currentPWM + delta, 0, PWM_MAX);

    if (err > 0) _hwUp((uint8_t)_currentPWM);
    else         _hwDown((uint8_t)_currentPWM);
}

// ─── API pública ─────────────────────────────────────────────
namespace Motor {

void init() {   // ← quitar Motor:: — estás dentro del namespace
    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    pinMode(MOTOR_EN,  OUTPUT);
    analogWriteFrequency(MOTOR_IN1, 20000);
    analogWriteFrequency(MOTOR_IN2, 20000);
    analogWriteResolution(MOTOR_IN1, 8);   // ← requiere pin en IDF5
    analogWriteResolution(MOTOR_IN2, 8);
    log_i("[MOTOR] PWM analogWrite @ 20kHz res=8 IN1=%d IN2=%d", MOTOR_IN1, MOTOR_IN2);
    _hwStop();
}

void begin() {
    faderADC.update();
    _adcFiltered = (float)faderADC.getFaderPos();
    _phase       = CalibPhase::IDLE;
}

void startCalib() {
    if (_phase == CalibPhase::GOING_UP ||
        _phase == CalibPhase::GOING_DOWN) return;

    faderADC.update();
    _stableRef          = (int)faderADC.getFaderPos();
    _stableStart        = millis();
    _calibStart         = millis();
    _calibMinDetectTime = millis() + 1000;  // mínimo 1s antes de detectar tope superior
    _motorActive        = false;
    _phase              = CalibPhase::GOING_UP;
    _hwUp(CALIB_KICK_PWM);
    delay(CALIB_KICK_MS);
    _hwUp(CALIB_PWM);
    log_i("[CALIB] Iniciada");
}

void update() {
    if (_phase == CalibPhase::GOING_UP ||
        _phase == CalibPhase::GOING_DOWN) {
        _calibTick();
    } else if (_phase == CalibPhase::DONE) {
        _positionTick();
    } else {
        _hwOff();
    }
}

void setADC(uint16_t v) {
    if (_adcFiltered > 0 && abs((int)v - (int)_adcFiltered) > 300) return;
    _adcFiltered = _adcFiltered * 0.6f  + (float)v * 0.4f;   // rápido
    _adcControl  = _adcControl  * 0.92f + (float)v * 0.08f;  // lento — muy suave
}

void setTarget(uint16_t midiTarget) {   // ← FIX BUG 2: única versión, guarda siempre
    _lastMidiTarget = midiTarget;
    if (_phase != CalibPhase::DONE) return;
    _targetADC = (uint16_t)map((long)midiTarget, 0, 14848, _adcMin, _adcMax);
    log_d("[TARGET] midi=%d → adc=%d (min=%d max=%d)",
          midiTarget, _targetADC, _adcMin, _adcMax);
}

void stop() {
    if (_phase == CalibPhase::GOING_UP ||
        _phase == CalibPhase::GOING_DOWN) return;
    _hwOff();
}

void off() {
    _hwOff();
    _motorActive = false;
    _currentPWM  = 0;
    if (_phase == CalibPhase::GOING_UP ||
        _phase == CalibPhase::GOING_DOWN) {
        _phase = CalibPhase::IDLE;   // aborta calibración en curso
        log_i("[MOTOR] off() — calibración abortada por desconexión");
    }
}


CalibState getCalibState() {
    switch (_phase) {
        case CalibPhase::GOING_UP:   return CalibState::CALIB_UP;
        case CalibPhase::GOING_DOWN: return CalibState::CALIB_DOWN;
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
    if (pwm == 0) { _hwOff(); return; }
    if (pwm > 0) _hwUp((uint8_t)constrain(pwm, 0, 255));
    else         _hwDown((uint8_t)constrain(-pwm, 0, 255));
}

} // namespace Motor