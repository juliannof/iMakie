#pragma once

#include <Arduino.h>
#include "../config.h"

// Define la clase MotorController
class MotorController {
public: // Miembros públicos (interfaz)
    MotorController();  // Constructor
    void begin();       // Inicialización
    void enable();      // Habilita el driver H-Bridge
    void disable();     // Deshabilita el driver H-Bridge
    void drive(int speed);  // Mueve el motor con una velocidad (-1023 a 1023)
    void emergencyStop();   // Detiene el motor y deshabilita el driver
    bool isMoving();        // Verifica si el motor está en movimiento
    bool isEnabled();       // Verifica si el driver está habilitado
      
private: // Miembros privados (implementación interna)
    bool _enabled;      // Estado de habilitación del driver
    bool _moving;       // Estado de movimiento del motor
    int _currentSpeed;  // Velocidad actual aplicada
    unsigned long _lastDebugTime; // Para mensajes de depuración

    void setupPWM();    // Configura los canales PWM del ESP32
}; // <--- ¡MUY IMPORTANTE! Esta llave cierra la declaración de 