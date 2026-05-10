#pragma once
#include <Arduino.h>

namespace Motor {

    enum class CalibState {
        IDLE,
        CALIB_UP,
        CALIB_DOWN,
        DONE,
        ERROR
    };

    // Ciclo de vida
    void init();    // configurar pines + PWM — llamar ANTES de Serial.begin()
    void update();  // tick de calibración + control de posición

    // Entrada ADC (desde FaderADC)
    void setADC(uint16_t raw);

    // Control
    void setTarget(uint16_t midiPB14);  // 0-16383, mapea internamente a ADC
    void off();                         // para motor + desactiva control
    void stop();                        // frena motor, mantiene target

    // Consultas
    uint16_t   getRawADC();
    float      getPosition();  // 0.0–1.0
    uint16_t   getADCMin();
    uint16_t   getADCMax();

    // Calibración
    void       startCalib();
    CalibState getCalibState();
    bool       isCalibrated();

} // namespace Motor
