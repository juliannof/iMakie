#include <Arduino.h>
#include "FaderTouch.h"
#include "config.h"

// ─── Constantes internas ──────────────────────────────────────
static constexpr uint32_t POLL_MS        = 20;
static constexpr uint8_t  BASELINE_SAMPS = 16;
static constexpr float    THR_TOUCH      =
    FADER_TOUCH_THRESHOLD_PERCENTAGE / 100.0f;
static constexpr float    THR_RELEASE    =
    (FADER_TOUCH_THRESHOLD_PERCENTAGE + 5) / 100.0f;

// ─── Estado privado ───────────────────────────────────────────
static uint32_t _base     = 0;
static uint32_t _raw      = 0;
static bool     _touched  = false;
static unsigned long _lastPoll = 0;

static void (*_cbTouch)()   = nullptr;
static void (*_cbRelease)() = nullptr;

// ─── Helpers ─────────────────────────────────────────────────
static uint32_t _sample() {
    return touchRead(FADER_TOUCH_PIN);
}

static uint32_t _sampleAvg() {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < 16; i++) sum += touchRead(FADER_TOUCH_PIN);
    return sum / 16;
}

static void _captureBaseline() {
    delay(100);
    uint32_t sum = 0;
    for (uint8_t i = 0; i < BASELINE_SAMPS; i++) {
        sum += _sample();
        delay(3);
    }
    uint32_t avg = sum / BASELINE_SAMPS;
    if (avg > 50) _base = avg;
}

// ─── API pública ─────────────────────────────────────────────
namespace FaderTouch {

void init() {
    _touched = false;
    _captureBaseline();
}

bool update() {
    unsigned long now = millis();
    if (now - _lastPoll < POLL_MS) return false;
    _lastPoll = now;

    if (_base == 0) {
        uint32_t v = _sample();
        if (v > 50) _base = v;
        return false;
    }

    _raw = _sampleAvg();

    // AUTOCALIBRACION CONSTANTE (única fuente de verdad)
    // Baseline sigue el valor de reposo actual para adaptarse a:
    // - Variaciones del plástico
    // - Cambios de temperatura
    // - Envejecimiento del material
    // Solo se actualiza cuando NO hay toque (detección confiable)
    if (!_touched) {
        // IIR filter: alpha = 1/16 → sigue lentamente pero constantemente
        _base = (_base * 15 + _raw) / 16;
    }

    bool prev = _touched;
    if (!_touched && _raw > _base * THR_TOUCH)   _touched = true;
    if ( _touched && _raw < _base * THR_RELEASE) _touched = false;

    log_v("Touch raw=%lu base=%lu ratio=%.2f%% touch=%d", 
          _raw, _base, (_raw * 100.0f / _base), _touched);

    if (_touched != prev) {
        if (_touched  && _cbTouch)   _cbTouch();
        if (!_touched && _cbRelease) _cbRelease();
        return true;
    }
    return false;
}

bool     isTouched()  { return _touched; }
uint32_t getRaw()     { return _raw;     }
uint32_t getBase()    { return _base;    }

void resetBaseline()  { _base = 0; _captureBaseline(); }

void onTouch(void (*cb)())   { _cbTouch   = cb; }
void onRelease(void (*cb)()) { _cbRelease = cb; }

} // namespace FaderTouch
