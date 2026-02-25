#include "MotorController.h"
  
MotorController::MotorController()  
    : _enabled(false), _moving(false), _currentSpeed(0), _lastDebugTime(0) {  
}  
  
void MotorController::begin() {  
    setupPWM();  
    pinMode(ENABLE_PIN, OUTPUT);  
    disable(); // Asegurarse de que el motor esté deshabilitado al inicio
    Serial.println("✅ MotorController inicializado");
}  
  
void MotorController::setupPWM() {  
    // Configurar LEDC (PWM de ESP32) para los pines del motor
    ledcSetup(0, PWM_FREQ, PWM_RESOLUTION); // Canal 0
    ledcSetup(1, PWM_FREQ, PWM_RESOLUTION); // Canal 1
    ledcAttachPin(MOTOR_IN1, 0); // Asignar pin MOTOR_IN1 al canal 0
    ledcAttachPin(MOTOR_IN2, 1); // Asignar pin MOTOR_IN2 al canal 1
}  
  
void MotorController::enable() {  
    digitalWrite(ENABLE_PIN, HIGH); // Habilitar el driver del motor
    _enabled = true;  
    Serial.println("   🟢 Motor HABILITADO");  
}  
  
void MotorController::disable() {  
    digitalWrite(ENABLE_PIN, LOW); // Deshabilitar el driver del motor
    ledcWrite(0, 0);               // Asegurar que el PWM sea 0 en ambos canales
    ledcWrite(1, 0);  
    _enabled = false;  
    _moving = false;  
    _currentSpeed = 0;  
    Serial.println("   🔴 Motor DESHABILITADO");  
}  
  
void MotorController::drive(int speed) {  
    if (!_enabled) {  
        // Serial.println("   ⚠️  Motor no habilitado para drive"); // Se comenta para evitar spam si es frecuente
        return;  
    }  
      
    // Limitar la velocidad al rango de PWM (-1023 a 1023 para 10 bits)
    _currentSpeed = constrain(speed, -1023, 1023);  
    int pwmValue = abs(_currentSpeed); // El valor de PWM es siempre positivo
    _moving = (pwmValue > 0);       // El motor se está moviendo si el PWM es > 0
      
    if (speed > 0) { // Movimiento hacia adelante
        ledcWrite(0, pwmValue);    // IN1 a pwmValue
        ledcWrite(1, 0);           // IN2 a 0
    } else if (speed < 0) { // Movimiento hacia atrás
        ledcWrite(0, 0);           // IN1 a 0
        ledcWrite(1, pwmValue);    // IN2 a pwmValue
    } else { // Parada (speed == 0)
        ledcWrite(0, 0);  
        ledcWrite(1, 0);  
        _moving = false;  
    }  
      
    // Debug cada 2 segundos mientras el motor está en movimiento
    if (millis() - _lastDebugTime > 2000 && _moving) {  
        _lastDebugTime = millis();  
        // Serial.printf("   🎛️  PWM: %d/%d (%d%%)\n",  // Comentado para evitar spam
        //              pwmValue, (1 << PWM_RESOLUTION) - 1, (pwmValue * 100) / ((1 << PWM_RESOLUTION) - 1));  
    }  
}  
  
void MotorController::emergencyStop() {  
    Serial.println("   🚨 PARADA DE EMERGENCIA");  
    drive(0); // Detener cualquier movimiento
    disable(); // Deshabilitar el driver
}  
  
bool MotorController::isMoving() {  
    return _moving;  
}  
  
bool MotorController::isEnabled() {  
    return _enabled;  
}