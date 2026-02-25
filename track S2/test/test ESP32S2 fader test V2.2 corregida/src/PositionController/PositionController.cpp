#include "PositionController.h"
#include "config.h" 

// Constructor
PositionController::PositionController(FaderSensor* faderSensor)  
    : _targetADC(0),  
      _moving(false),  
      _outputPWM(0),  
      _holding(false),                
      _sensor(faderSensor),         
      _lastPulseTime(0),  
      _pulseEndTime(0),  
      _currentPulseDuration(DEFAULT_PULSE_DURATION), 
      _currentPulseInterval(DEFAULT_PULSE_INTERVAL), 
      _kp(DEFAULT_KP),                     
      _ki(DEFAULT_KI),                      
      _kd(DEFAULT_KD),                      
      _errorAnterior(0), _integral(0),  
      _lastPIDTime(0)  
{  
    Serial.println("✅ PositionController inicializado con valores DEFAULT de config.h.");  
}  

void PositionController::setTargetPosition(int targetADC) {
    _targetADC = targetADC;
    _moving = true; 
    _holding = false;                
    Serial.printf("🎯 Objetivo ADC establecido: %d\n", _targetADC);
    _integral = 0;
    _errorAnterior = _sensor->readSmoothed(); 
    _lastPIDTime = millis();
}

void PositionController::setTargetPositionPercent(int targetPercent, int minADC, int maxADC) {
    int targetADC_calculated = map(targetPercent, 0, 100, minADC, maxADC);
    targetADC_calculated = constrain(targetADC_calculated, minADC, maxADC); 
    setTargetPosition(targetADC_calculated); 
}

// --- FUNCIÓN UPDATE PRINCIPAL CON LÓGICA DE DETECCIÓN Y TRANSICIÓN ---  
int PositionController::update(int currentADC) {  
    // Comprueba si el sensor es válido
    if (!_sensor) {
        // En un entorno embebido, esto podría ser un error fatal o simplemente un warning.
        // Por ahora, devolvemos 0 que significa "no hacer nada".
        Serial.println("ERROR: FaderSensor no inicializado en PositionController::update.");
        return 0;
    }
    
    // Calcula el error
    int error = _targetADC - currentADC;  
    int errorAbs = abs(error);

    // --- LÓGICA DE DETECCIÓN DE POSICIÓN ALCANZADA Y ANCLAJE ---  
    // DBG_TOLERANCE_CHECK te ayuda a ver los valores, dejarlo en un comentario.
    // Serial.printf("DBG_TOLERANCE_CHECK: currentADC_for_check:%d, Target:%d, Error:%d, AbsError:%d, Tolerance:%d, Condition:%d\n",
    //    currentADC, _targetADC, error, errorAbs, POSITION_TOLERANCE, (errorAbs <= POSITION_TOLERANCE));

    // Si YA ESTOY en modo "holding" y el error es MAYOR que la tolerancia de reactivación,
    // significa que el fader se movió y necesito volver a moverme.
    if (_holding && errorAbs > RE_ACTIVATION_TOLERANCE) { // <-- Nueva lógica de reactivación
        _moving = true;
        _holding = false; // Ya no estamos "sólo manteniendo", vamos a corregir.
        Serial.printf("📈 Fader se salió de la tolerancia (error: %d). Reactivando movimiento.\n", errorAbs);
    }
    // Si estoy dentro de la tolerancia de posición...
    else if (errorAbs <= POSITION_TOLERANCE) {
        if (_moving) {  
            _moving = false; // Ya no estamos en movimiento activo
            _outputPWM = 0;   // Detener motor
            _holding = true; // Entrar en modo mantenimiento  
            Serial.printf("✅ Posición alcanzada (asentada): %d ADC (Objetivo: %d)\n", currentADC, _targetADC);  
            _errorAnterior = 0; // Limpiar estado del PID
            _integral = 0;      // Limpiar estado del PID
        }  
        // Si _holding es true y estamos dentro de tolerancia normal, el motor debe estar apagado.
        if (_holding) {  
             _outputPWM = 0; 
             // Serial.println(">>> DEBUG HOLDING: Motor APAGADO."); // Descomentar para debug específico de holding
             return _outputPWM; // Devolver 0 (motor parado) y no hacer más cálculo.
        }  
    }  
    // Si no estamos en modo "holding" Y _moving es true, o si _holding fue desactivado
    // porque el fader se movió demasiado (ver la primera condición 'if').
    // O si simplemente estamos acercándonos al objetivo por primera vez.
    if (_moving) {  
        float pidOutputFloat_unconstrained = 0;  
        int calculated_PID_PWM = controlPorPID(currentADC, pidOutputFloat_unconstrained);  
        _outputPWM = calculated_PID_PWM;   
        // Serial.printf(">>> DEBUG: MODO PID ACTIVO (E:%d, RawPID:%.2f) -> PWM:%d\n", error, pidOutputFloat_unconstrained, _outputPWM);  
    } else {  
        // Esto captura casos donde _moving es false pero no estamos en _holding
        // (por ejemplo, al inicio o si hay algún estado intermedio no previsto).
        // En general, si _moving es false y no _holding, debería estar parado.
        _outputPWM = 0; 
    }  
      
    return _outputPWM;  
}  

// ... (El resto de tus funciones controlPorPulsos, resetControllerState, controlPorPID quedan IGUAL) ...

// src/PositionController/PositionController.cpp - Función controlPorPulsos()
void PositionController::controlPorPulsos(int currentADC) { 
    int error = _targetADC - currentADC;
    int errorAbs = abs(error);
    // --- Duración de Pulso (Menos "fino", más efectivo) ---
    // Aumentamos los mínimos para asegurar que el motor reciba suficiente energía.
    if (errorAbs > 150) {  _currentPulseDuration = 20; }  
    else if (errorAbs > 50) { _currentPulseDuration = 10; } // Antes 5ms, ahora 10ms
    else {  _currentPulseDuration = 5;  } // Antes 1ms-2ms, ahora un mínimo de 5ms.
    // --- Intervalo entre Pulsos (Menos espaciados, para reaccionar antes) ---
    // Reducimos los máximos, para que la corrección no tarde tanto en llegar.
    if (errorAbs > 100) { _currentPulseInterval = 300; } // Mantener
    else if (errorAbs > 50) { _currentPulseInterval = 500; } // Mantener
    else { _currentPulseInterval = 1000; } // Antes 1500-2000ms, ahora un máximo de 1000ms.
                                            
    if (_outputPWM != 0) { // Si el motor está actualmente encendido (en un pulso)
        if (millis() >= _pulseEndTime) {
            _outputPWM = 0; // Apagar al finalizar la duración del pulso
        }
    } 
    else if (millis() - _lastPulseTime > _currentPulseInterval) { // Si el motor está apagado y es hora de otro pulso
        _lastPulseTime = millis(); 
        _outputPWM = (error > 0 ? PWM_MIN_MOTION_THRESHOLD : -PWM_MIN_MOTION_THRESHOLD); 
        _pulseEndTime = millis() + _currentPulseDuration; 
          
        Serial.printf("PULSO- Dir: %s, Dur: %dms, Int: %dms, Err: %d, Obj: %d, Act: %d\n",  
                     (error > 0 ? "Fwd" : "Rev"), _currentPulseDuration, _currentPulseInterval, 
                     error, _targetADC, currentADC);
    } 
    else { // Si no hay pulso activo y NO es hora de iniciar uno
        _outputPWM = 0; // Asegurarse de que el motor está realmente parado.
    }
}

void PositionController::resetControllerState() {  
    _moving = false;  
    _holding = false;  
    _outputPWM = 0; 
    _errorAnterior = 0; 
    _integral = 0;  
    _lastPIDTime = 0; 
    Serial.println(">>> DEBUG: PositionController estado reseteado.");  
}

// --- controlPorPID (completa con Debug y Banda Muerta) ---  
int PositionController::controlPorPID(int currentADC, float& outPidOutputFloat_unconstrained) {  
    // =======================================================================
    // DECLARACIONES Y CALCULOS INICIALES DEL PID
    // =======================================================================
    unsigned long now = millis();  
    float dt = (now > _lastPIDTime) ? (float)(now - _lastPIDTime) / 1000.0f : 0.001f;  
    if (_lastPIDTime == 0) dt = 0.001f; // Evitar dt grande o 0 en la primera pasada
    _lastPIDTime = now;  
   
    if (dt < 0.005f) dt = 0.005f;  
    if (dt > 0.1f) dt = 0.1f;      
   
    int error = _targetADC - currentADC;  
   
    // Componente Proporcional
    float pOut = _kp * error;  
   
    // Componente Integral (solo si _ki es diferente de 0)
    float iOut = 0; 
    if (_ki != 0) {
        _integral += error * dt;  
        _integral = constrain(_integral, -2000.0f, 2000.0f); 
        iOut = _ki * _integral;  
    }
    
    // Componente Derivativo (solo si _kd es diferente de 0)
    float dOut = 0; 
    if (_kd != 0 && dt > 0) {
       const float alpha = 0.2;  // Factor de filtro (ajustable)
       static float lastDerivada = 0;
       
       float rawDerivada = (error - _errorAnterior) / dt;
       rawDerivada = constrain(rawDerivada, -500.0f, 500.0f);  // Nuevo límite
       
       
       // Filtro pasa-bajos
       float derivada = alpha * rawDerivada + (1 - alpha) * lastDerivada;
       lastDerivada = derivada;
       
       dOut = _kd * derivada;
    }
    // =======================================================================
    
    float pidOutputFloat_raw = pOut + iOut + dOut; 
    outPidOutputFloat_unconstrained = pidOutputFloat_raw;  
     
    float pidOutputFloat_constrained = pidOutputFloat_raw; 
       
    // ====================================================================================
    // Lógica de Banda Muerta para los valores de PWM
    // ====================================================================================
    if (pidOutputFloat_constrained > 0) { 
      if (pidOutputFloat_constrained < PWM_MIN_MOTION_THRESHOLD) {
        pidOutputFloat_constrained = 0; 
      } else {
        pidOutputFloat_constrained = constrain(pidOutputFloat_constrained, PWM_MIN_MOTION_THRESHOLD, PWM_MAX_USABLE_DRIVE);
      }
    } else if (pidOutputFloat_constrained < 0) { 
      if (pidOutputFloat_constrained > -PWM_MIN_MOTION_THRESHOLD) { 
        pidOutputFloat_constrained = 0; 
      } else {
        pidOutputFloat_constrained = constrain(pidOutputFloat_constrained, -PWM_MAX_USABLE_DRIVE, -PWM_MIN_MOTION_THRESHOLD);
      }
    } else { 
      pidOutputFloat_constrained = 0;
    }
    // ====================================================================================
       
    int finalPWM = static_cast<int>(pidOutputFloat_constrained);
     
    // --- Debugging ---
    Serial.printf("DEBUG PID: E:%d, pO:%.2f, iO:%.2f, dO:%.2f, Final_PID_Constrained:%.2f (RawPID_Unconstrained:%.2f) -> PWM:%d (dt:%.3f)\n",  
                  error, pOut, iOut, dOut, pidOutputFloat_constrained, outPidOutputFloat_unconstrained, finalPWM, dt);  
         
    _errorAnterior = error; 
    return finalPWM;  
}  