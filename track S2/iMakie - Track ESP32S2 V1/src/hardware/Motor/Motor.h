#pragma once
#include <Arduino.h>

// ============================================================
//  Motor.h  —  Control de fader motorizado PTxx Track S2
//  DRV8833  |  ADC via FaderADC → setADC()  |  DAC ref GPIO17
// ============================================================

namespace Motor {

    // Estados de calibración
    enum class CalibState {
        IDLE,
        CALIB_DOWN,   // bajando a tope inferior
        CALIB_UP,     // subiendo a tope superior
        DONE,         // calibrado OK
        ERROR         // calibración fallida
    };

    // Llamar lo primero en setup() — silencia el motor antes de todo
    void init();

    // Arranca la calibración no bloqueante
    // Llamar después de faderADC.begin()
    void begin();

    // Avanza la máquina de calibración + control de posición
    // Llamar en cada loop()
    void update();

    // Recibe la lectura filtrada de FaderADC — llamar antes de update()
    void setADC(uint16_t value);

    // Target MIDI Pitch Bend 14-bit (0–16383)
    // Ignorado hasta que calibración esté DONE
    void setTarget(uint16_t midiTarget);

    // Parar el motor inmediatamente
    void stop();

    // Estado
    CalibState getCalibState();
    bool       isCalibrated();
    uint16_t   getADCMin();
    uint16_t   getADCMax();
    uint16_t   getRawADC();    // último valor recibido por setADC()
    float      getPosition();  // 0.0–1.0 posición real

} // namespace Motor