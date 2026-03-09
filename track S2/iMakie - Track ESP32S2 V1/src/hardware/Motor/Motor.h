#pragma once
#include <Arduino.h>

// ============================================================
//  Motor.h  —  Control de fader motorizado PTxx Track S2
//  DRV8833  |  ADC GPIO10  |  DAC ref 1.1V GPIO17
// ============================================================

namespace Motor {

    // --- Inicialización y calibración de boot ---
    // Mueve el fader a los dos topes físicos y determina
    // ADC_MIN y ADC_MAX reales. Bloquea ~2 s en boot.
    void begin();
    
    void read();

    // --- Target MIDI → posición fader ---
    // target: valor Pitch Bend 14-bit (0–16383)
    void setTarget(uint16_t target);

    // --- Loop de control (llamar cada ~5 ms) ---
    void update();

    // --- Parar el motor inmediatamente ---
    void stop();

    // --- Estado ---
    bool     isCalibrated();
    uint16_t getADCMin();
    uint16_t getADCMax();
    uint16_t getRawADC();    // lectura filtrada actual
    float    getPosition();  // 0.0–1.0 posición real
    void setADC(uint16_t value);


} // namespace Motor