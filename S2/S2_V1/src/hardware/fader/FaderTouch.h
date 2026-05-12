#pragma once
#include <Arduino.h>

// ─── FaderTouch ───────────────────────────────────────────────
// Módulo dedicado a la detección táctil del fader.
// ESP32-S2: touchRead() → valor ALTO = libre, BAJO = tocado.
// Baseline capturado en init(). Histéresis integrada (+5%).
// ─────────────────────────────────────────────────────────────

namespace FaderTouch {

    // Inicializa y captura baseline. Llamar en setup() tras initDisplay().
    void init();

    // Polling. Llamar en cada tick del loop.
    // Devuelve true si el estado cambió en esta llamada.
    bool update();

    bool     isTouched();
    uint32_t getRaw();    // valor crudo último — para SAT debug
    uint32_t getBase();   // baseline capturado   — para SAT debug

    // Fuerza re-captura del baseline (desde SAT menu).
    void resetBaseline();

    // Callbacks opcionales.
    void onTouch(void (*cb)());
    void onRelease(void (*cb)());
}