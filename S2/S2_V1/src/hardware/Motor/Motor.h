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
    void init();        // configurar pines + PWM — llamar ANTES de Serial.begin()
    void initPWM();     // leer pwmMin/Max de NVS — si inválido, pwmMin=pwmMax=0 (2026-05-10 20:20)
    void update();      // tick de calibración + control de posición

    // Entrada ADC (desde FaderADC)
    void setADC(uint16_t raw);
    void setADCDelta(uint16_t currentADC);  // detecta movimiento manual por delta rápido, detiene motor si >500 cuentas

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
    void       goToMin();  // Baja motor a posición 0 (mínimo), espera órdenes de S3
    CalibState getCalibState();
    bool       isCalibrated();

    // Test mode — control directo (2026-05-10 19:54)
    void testUp(uint8_t pwm);
    void testDown(uint8_t pwm);
    void testOff();

    // PWM range (lee de NVS, 0=inválido) (2026-05-10 20:20)
    uint8_t getPWMMin();
    uint8_t getPWMMax();

} // namespace Motor
