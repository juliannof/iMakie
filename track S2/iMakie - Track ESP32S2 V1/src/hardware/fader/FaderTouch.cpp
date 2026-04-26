#include <Arduino.h>
#include "FaderTouch.h"
#include "config.h"

// ─── Constantes internas ──────────────────────────────────────
static constexpr uint32_t POLL_MS        = 20;
static constexpr uint8_t  BASELINE_SAMPS = 16;
static constexpr float    THR_TOUCH      = 0.015f;  // 1.5% por encima baseline
static constexpr float    THR_RELEASE    = 0.01f;   // 1.0% por encima baseline
static constexpr uint8_t  SOSTENIMIENTO  = 6;       // 6 frames (~120ms) para confirmar

// ─── Estado privado ───────────────────────────────────────────
static uint32_t _base     = 0;
static uint32_t _raw      = 0;
static bool     _touched  = false;
static unsigned long _lastPoll = 0;
static uint8_t  _sostenidoAlto = 0;
static uint8_t  _sostenidoBajo = 0;

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
    // - Variaciones del plástico, cambios de temperatura, envejecimiento
    // Se actualiza siempre con IIR filter (alpha = 1/16)
    _base = (_base * 15 + _raw) / 16;

    bool prev = _touched;
    
    // Solo detectar si baseline está establecido (>10000 para evitar falsos en arranque)
    if (_base > 10000) {
        float ratioThresholdTouch = _base * (1.0f + THR_TOUCH);
        float ratioThresholdRelease = _base * (1.0f + THR_RELEASE);
        
        // Contar frames donde raw está sostenidamente alto (tocado)
        if (_raw > ratioThresholdTouch) {
            _sostenidoAlto++;
            _sostenidoBajo = 0;
        } else {
            _sostenidoAlto = 0;
            // Contar frames donde raw está sostenidamente bajo (libre)
            if (_raw < ratioThresholdRelease) {
                _sostenidoBajo++;
            } else {
                _sostenidoBajo = 0;
            }
        }
        
        // TOUCH: raw se mantiene alto durante SOSTENIMIENTO frames
        if (!_touched && _sostenidoAlto >= SOSTENIMIENTO) {
            _touched = true;
            _sostenidoAlto = 0;
        }
        
        // RELEASE: raw se mantiene bajo durante SOSTENIMIENTO frames
        if (_touched && _sostenidoBajo >= SOSTENIMIENTO) {
            _touched = false;
            _sostenidoBajo = 0;
        }
    } else {
        _touched = false;
        _sostenidoAlto = 0;
        _sostenidoBajo = 0;
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

void resetBaseline()  { _base = 0; _captureBaseline(); }

void onTouch(void (*cb)())   { _cbTouch   = cb; }
void onRelease(void (*cb)()) { _cbRelease = cb; }

} // namespace FaderTouch
