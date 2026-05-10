#include <Arduino.h>
#include "FaderTouch.h"
#include "config.h"

// ─── Constantes (desde config.h) ──────────────────────────────

// ─── Estado privado ───────────────────────────────────────────
static uint32_t _base     = 0;
static uint32_t _raw      = 0;
static bool     _touched  = false;
static unsigned long _lastPoll = 0;
static unsigned long _touchStartTime = 0;
static unsigned long _releaseStartTime = 0;

static void (*_cbTouch)()   = nullptr;
static void (*_cbRelease)() = nullptr;

// ─── Helpers ─────────────────────────────────────────────────
static uint32_t _sample() {
    return touchRead(FADER_TOUCH_PIN);
}

static uint32_t _sampleAvg() {
    uint32_t sum = 0;
    uint8_t count = 0;
    for (uint8_t i = 0; i < TOUCH_BASELINE_SAMPS; i++) {
        int val = touchRead(FADER_TOUCH_PIN);
        if (val >= 0) {
            sum += val;
            count++;
        }
    }
    return count > 0 ? sum / count : 0;
}

// ─── API pública ─────────────────────────────────────────────
namespace FaderTouch {

void init() {
    _touched = false;
    _touchStartTime = 0;
    _releaseStartTime = 0;
}

bool update() {
    unsigned long now = millis();
    if (now - _lastPoll < TOUCH_POLL_MS) return false;
    _lastPoll = now;

    if (_base == 0) {
        uint32_t v = _sample();
        if (v > TOUCH_BASE_MIN_VALUE) _base = v;
        else _base = TOUCH_BASE_MIN_VALUE;  // fallback — garantiza _base >= TOUCH_BASE_MIN_VALUE
        return false;
    }

    _raw = _sampleAvg();

    bool prev = _touched;

    // AUTOCALIBRACION (solo durante reposo)
    // Se pausa durante toque para evitar que baseline siga al dedo
    // Se reanuda cuando se libera, permitiendo adaptación a ambiente
    if (!prev) _base = (_base * 15 + _raw) / 16;  // IIR filter (alpha = 1/16)

    // Solo detectar si baseline está establecido (umbral mínimo para evitar falsos en arranque)
    if (_base > TOUCH_BASE_MIN_THRESHOLD) {
        float ratioThresholdTouch = _base * (1.0f + TOUCH_THR_TOUCH);
        float ratioThresholdRelease = _base * (1.0f + TOUCH_THR_RELEASE);

        // Detectar si raw está sostenidamente alto (tocado)
        if (_raw > ratioThresholdTouch) {
            if (_touchStartTime == 0) _touchStartTime = now;
            _releaseStartTime = 0;
        } else {
            _touchStartTime = 0;
            // Detectar si raw está sostenidamente bajo (libre)
            if (_raw < ratioThresholdRelease) {
                if (_releaseStartTime == 0) _releaseStartTime = now;
            } else {
                _releaseStartTime = 0;
            }
        }

        // TOUCH: raw se mantiene alto durante sostenimiento (ms)
        if (!_touched && _touchStartTime > 0 &&
            now - _touchStartTime >= (unsigned long)TOUCH_SOSTENIMIENTO * TOUCH_POLL_MS) {
            _touched = true;
            _touchStartTime = 0;
        }

        // RELEASE: raw se mantiene bajo durante sostenimiento (ms)
        if (_touched && _releaseStartTime > 0 &&
            now - _releaseStartTime >= (unsigned long)TOUCH_SOSTENIMIENTO * TOUCH_POLL_MS) {
            _touched = false;
            _releaseStartTime = 0;
        }
    } else {
        _touched = false;
        _touchStartTime = 0;
        _releaseStartTime = 0;
    }

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

void onTouch(void (*cb)())   { _cbTouch   = cb; }
void onRelease(void (*cb)()) { _cbRelease = cb; }

} // namespace FaderTouch
