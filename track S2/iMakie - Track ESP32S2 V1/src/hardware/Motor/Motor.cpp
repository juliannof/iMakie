// ============================================================
//  Motor.cpp  —  Control de fader motorizado PTxx Track S2
//  DRV8833  |  ADC GPIO10  |  DAC ref 1.1V GPIO17
//
//  Estrategia de control (3 zonas, sin PID):
//    Far zone   → velocidad alta con curva no lineal
//    Brake zone → frenado progresivo
//    Dead zone  → motor parado
// ============================================================
#include "Motor.h"
#include "../../config.h"

// ─── Parámetros de control ────────────────────────────────────
static constexpr uint8_t  PWM_MIN     = 60;    // mínimo PWM que mueve el motor
static constexpr uint8_t  PWM_MAX     = 220;   // máximo (margen térmico)
static constexpr uint8_t  PWM_SLEW    = 12;    // máx cambio de PWM por ciclo
static constexpr uint8_t  DEAD_ZONE   = 15;     // ADC counts — no mover
static constexpr uint8_t  BRAKE_DIST  = 18;    // ADC counts — frenar progresivo
static constexpr float    EMA_ALPHA   = 0.12f; // suavizado ADC
static constexpr float    CURVE_GAMMA = 0.6f;  // curva no lineal (< 1 = más suave)

// ─── Parámetros de calibración ───────────────────────────────
static constexpr uint8_t  CALIB_PWM     = 130;  // velocidad durante calibración
static constexpr uint16_t STALL_COUNTS  = 6;    // ADC counts mínimo de movimiento
static constexpr uint32_t STALL_WINDOW  = 250;  // ms sin movimiento → tope detectado
static constexpr uint32_t CALIB_TIMEOUT = 3500; // ms máximo por dirección

// ─── Estado interno ───────────────────────────────────────────
static uint16_t _adcMin      = 100;
static uint16_t _adcMax      = 3900;
static uint16_t _adcSpan     = 3800;
static float    _adcFiltered = 0.0f;
static uint16_t _targetADC   = 0;
static int      _currentPWM  = 0;
static bool     _calibrated  = false;

// ─── Helpers de hardware ─────────────────────────────────────
static void _hwStop() {
    digitalWrite(MOTOR_EN,  LOW);
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    _currentPWM = 0;
}

static void _hwDrive(int pwm) {
    // pwm positivo → sube fader, negativo → baja
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



static float _updateFilter() {
    float raw = (float)_readADC();
    _adcFiltered = EMA_ALPHA * raw + (1.0f - EMA_ALPHA) * _adcFiltered;
    return _adcFiltered;
}

// ─── Calibración: busca un tope físico ───────────────────────
// direction: +1 = sube (ADC crece), -1 = baja (ADC decrece)
// Devuelve el valor ADC del tope.
static uint16_t _findTope(int direction) {
    _hwDrive(direction * CALIB_PWM);

    uint16_t lastADC   = _readADC();
    uint32_t stallStart = millis();
    uint32_t startTime  = millis();

    while ((millis() - startTime) < CALIB_TIMEOUT) {
        delay(20);
        uint16_t nowADC = _readADC();
        uint16_t delta  = (nowADC > lastADC) ? (nowADC - lastADC) : (lastADC - nowADC);

        if (delta >= STALL_COUNTS) {
            // hay movimiento → reiniciar ventana de stall
            lastADC    = nowADC;
            stallStart = millis();
        } else if ((millis() - stallStart) >= STALL_WINDOW) {
            // sin movimiento durante STALL_WINDOW → tope alcanzado
            break;
        }
    }

    _hwStop();
    delay(80); // deja asentar el ADC
    return _readADC();
}

// ─── API pública ─────────────────────────────────────────────
namespace Motor {

void begin() {
    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    pinMode(MOTOR_EN,  OUTPUT);
    _hwStop();

    

    // ADC: rango 0–1.1V
    analogReadResolution(12);
    analogSetPinAttenuation(FADER_POT_PIN, ADC_0db);

    // Seed del filtro con lectura real
    _adcFiltered = (float)_readADC();

    // ── Calibración de boot ──────────────────────────────────
    // Igual que la Mackie: baja a tope, lee MIN → sube a tope, lee MAX
    log_i("Motor: calibrando...");

    uint16_t topeAbajo = _findTope(-1);  // baja
    delay(100);
    uint16_t topeArriba = _findTope(+1); // sube
    delay(100);

    // Sanity check: los topes deben tener un span mínimo razonable
    if (topeArriba > topeAbajo && (topeArriba - topeAbajo) > 200) {
        _adcMin  = topeAbajo  + 10; // pequeño margen en los extremos
        _adcMax  = topeArriba - 10;
        _adcSpan = _adcMax - _adcMin;
        _calibrated = true;
        log_i("Motor: calibrado OK  MIN=%d  MAX=%d  span=%d",
              _adcMin, _adcMax, _adcSpan);
    } else {
        // Calibración fallida → usar valores hardcodeados de config.h
        _adcMin     = FADER_ADC_MIN;
        _adcMax     = FADER_ADC_MAX;
        _adcSpan    = _adcMax - _adcMin;
        _calibrated = false;
        log_e("Motor: calibracion fallida  raw_bajo=%d  raw_arriba=%d  — usando config.h",
              topeAbajo, topeArriba);
    }

    // Dejar el fader en reposo (donde quedó tras subir)
    _hwStop();
    _adcFiltered = (float)_readADC();
    _targetADC   = (uint16_t)_adcFiltered;
}

void Motor::setADC(uint16_t value) {
    _adcFiltered = EMA_ALPHA * value + (1.0f - EMA_ALPHA) * _adcFiltered;
}

void setTarget(uint16_t midiTarget) {
    // midiTarget: Pitch Bend 14-bit (0–16383) → ADC counts
    _targetADC = (uint16_t)map((long)midiTarget, 0, 16383, _adcMin, _adcMax);
}

void update() {
    float pos = _updateFilter();
    int   err = (int)_targetADC - (int)pos;
    int   absErr = abs(err);

    // ── Dead zone ────────────────────────────────────────────
    if (absErr <= DEAD_ZONE) {
        _hwStop();
        return;
    }

    // ── Calcular velocidad deseada ───────────────────────────
    int targetPWM = 0;

    if (absErr <= BRAKE_DIST) {
        // Zona de frenado: velocidad proporcional al error
        float frac = (float)absErr / BRAKE_DIST;
        targetPWM = PWM_MIN + (int)(frac * (PWM_MIN * 0.6f));
    } else {
        // Zona normal: curva no lineal
        float norm = (float)absErr / (float)_adcSpan;
        float vel  = powf(norm, CURVE_GAMMA);
        targetPWM  = PWM_MIN + (int)(vel * (PWM_MAX - PWM_MIN));
        targetPWM  = constrain(targetPWM, PWM_MIN, PWM_MAX);
    }

    if (err < 0) targetPWM = -targetPWM; // dirección

    // ── Slew rate limit ──────────────────────────────────────
    int delta = targetPWM - _currentPWM;
    if (delta >  PWM_SLEW) delta =  PWM_SLEW;
    if (delta < -PWM_SLEW) delta = -PWM_SLEW;
    _currentPWM += delta;

    _hwDrive(_currentPWM);
}

void stop() {
    _hwStop();
}

bool     isCalibrated() { return _calibrated; }
uint16_t getADCMin()    { return _adcMin; }
uint16_t getADCMax()    { return _adcMax; }
uint16_t getRawADC()    { return (uint16_t)_adcFiltered; }
float    getPosition()  {
    if (_adcSpan == 0) return 0.0f;
    return constrain((_adcFiltered - _adcMin) / (float)_adcSpan, 0.0f, 1.0f);
}

} // namespace Motor