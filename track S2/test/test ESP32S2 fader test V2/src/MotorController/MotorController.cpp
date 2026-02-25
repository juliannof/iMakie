#include "MotorController.h"

MotorController::MotorController() 
    : _enabled(false), _moving(false), _currentSpeed(0), _lastDebugTime(0) {
}

void MotorController::begin() {
    setupPWM();
    pinMode(ENABLE_PIN, OUTPUT);
    deshabilitar();
}

void MotorController::setupPWM() {
    ledcSetup(0, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(1, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(MOTOR_IN1, 0);
    ledcAttachPin(MOTOR_IN2, 1);
}

// Métodos en español
void MotorController::habilitar() {
    digitalWrite(ENABLE_PIN, HIGH);
    _enabled = true;
    Serial.println("   🟢 Motor HABILITADO");
}

void MotorController::deshabilitar() {
    digitalWrite(ENABLE_PIN, LOW);
    ledcWrite(0, 0);
    ledcWrite(1, 0);
    _enabled = false;
    _moving = false;
    _currentSpeed = 0;
    Serial.println("   🔴 Motor DESHABILITADO");
}

void MotorController::mover(int speed) {
    if (!_enabled) {
        Serial.println("   ⚠️  Motor no habilitado");
        return;
    }
    
    _currentSpeed = constrain(speed, -1023, 1023);
    int pwmValue = abs(_currentSpeed);
    _moving = (pwmValue > 0);
    
    if (speed > 0) {
        ledcWrite(0, 0);
        ledcWrite(1, pwmValue);
    } else if (speed < 0) {
        ledcWrite(0, pwmValue);
        ledcWrite(1, 0);
    } else {
        ledcWrite(0, 0);
        ledcWrite(1, 0);
        _moving = false;
    }
    
    // Debug cada 2 segundos
    if (millis() - _lastDebugTime > 2000 && _moving) {
        _lastDebugTime = millis();
        Serial.printf("   🎛️  PWM: %d/1023 (%d%%)\n", 
                     pwmValue, (pwmValue * 100) / 1023);
    }
}

void MotorController::paradaEmergencia() {
    Serial.println("   🚨 PARADA DE EMERGENCIA");
    deshabilitar();
}

bool MotorController::estaHabilitado() {
    return _enabled;
}

bool MotorController::estaMoviendo() {
    return _moving;
}

// Métodos en inglés (aliases)
void MotorController::enable() { habilitar(); }
void MotorController::disable() { deshabilitar(); }
void MotorController::drive(int speed) { mover(speed); }
void MotorController::emergencyStop() { paradaEmergencia(); }
bool MotorController::isEnabled() { return estaHabilitado(); }
bool MotorController::isMoving() { return estaMoviendo(); }