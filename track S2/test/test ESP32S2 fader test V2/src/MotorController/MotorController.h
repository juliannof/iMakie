#pragma once

#include <Arduino.h>
#include "../config.h"

class MotorController {
public:
    MotorController();
    void begin();
    
    // Métodos en español
    void habilitar();
    void deshabilitar();
    void mover(int speed);
    void paradaEmergencia();
    bool estaHabilitado();
    bool estaMoviendo();
    
    // Métodos en inglés (alternativos)
    void enable();
    void disable();
    void drive(int speed);
    void emergencyStop();
    bool isEnabled();
    bool isMoving();
    
private:
    bool _enabled;
    bool _moving;
    int _currentSpeed;
    unsigned long _lastDebugTime;
    
    void setupPWM();
};
