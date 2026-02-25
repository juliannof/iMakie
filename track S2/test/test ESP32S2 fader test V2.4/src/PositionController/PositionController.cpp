#include "PositionController.h"

PositionController::PositionController(FaderSensor* faderSensor, MotorController* motorCtrl)
    : _targetADC(0),
      _moving(false),
      _outputPWM(0),
      _holding(false),                
      _sensor(faderSensor),         
      _motor(motorCtrl), 
      _lastPulseTime(0),  
      _pulseEndTime(0),  
      // --- CAMBIO AQUÍ: Usar las constantes de "LOW_ERROR" como valores iniciales ---
      _currentPulseDuration(PULSE_DURATION_LOW_ERROR),  // Usar la constante más baja como valor inicial
      _currentPulseInterval(PULSE_INTERVAL_LOW_ERROR),  // Usar la constante más baja como valor inicial
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
    
    if (!_motor->isEnabled()) {
        _motor->enable();
    }
}

void PositionController::setTargetPositionPercent(int targetPercent, int minADC, int maxADC) {
    int targetADC_calculated = map(targetPercent, 0, 100, minADC, maxADC);
    targetADC_calculated = constrain(targetADC_calculated, minADC, maxADC); 
    setTargetPosition(targetADC_calculated); 
}

// Implementación de las nuevas funciones de pulso (si no se van a usar, mejor eliminarlas del .h)
// Esto resuelve los errores de "undefined reference" si se declararon en el .h pero no se definieron en el .cpp
void PositionController::setPulseDuration(int duration) {
    _currentPulseDuration = duration;
    Serial.printf(">>> DEBUG: Duración de pulso establecida a %d ms.\n", duration);
}

void PositionController::setPulseInterval(int interval) {
    _currentPulseInterval = interval;
    Serial.printf(">>> DEBUG: Intervalo de pulso establecido a %d ms.\n", interval);
}


void PositionController::resetControllerState() {  
    _moving = false;  
    _holding = false;  
    _outputPWM = 0; 
    _errorAnterior = 0; 
    _integral = 0;  
    _lastPIDTime = 0; 
    // Asegúrarse de que el motor frena cuando el controlador se resetea.
    // Esto es un punto importante para la seguridad y control.
    if (_motor) { // Verificar que _motor no sea nullptr antes de usarlo.
        _motor->drive(0); 
    }
    Serial.println(">>> DEBUG: PositionController estado reseteado.");  
}


// src/PositionController/PositionController.cpp

// ... (todas las definiciones de funciones y includes anteriores) ...

// --- FUNCIÓN UPDATE PRINCIPAL CON LÓGICA DE DETECCIÓN Y TRANSICIÓN ---  
int PositionController::update(int currentADC) {  
    // Comprueba si el sensor y el motor son válidos
    if (!_sensor) {
        Serial.println("ERROR: FaderSensor no inicializado en PositionController::update.");
        if (_motor) _motor->drive(0); 
        return 0;
    }
    if (!_motor) {
        Serial.println("ERROR: MotorController no inicializado en PositionController::update.");
        return 0;
    }
    
    // Asegurarse de que el motor esté habilitado si se supone que debe moverse.
    if (!_motor->isEnabled() && (_moving || _holding)) {
         _motor->enable();
    }
    
    // Calcula el error
    int error = _targetADC - currentADC;  
    int errorAbs = abs(error);

    // --- LÓGICA DE REACTIVACIÓN ---
    if (_holding && errorAbs > RE_ACTIVATION_TOLERANCE) { 
        _moving = true;
        _holding = false;
        Serial.printf("[%lu] 📈 Fader se salió de la tolerancia (error: %d, currentADC: %d). Reactivando movimiento.\n", millis(), errorAbs, currentADC);
    }

    // --- LÓGICA DE ASENTAMIENTO ---
    else if (errorAbs <= POSITION_TOLERANCE) {
        if (_moving) {  
            _moving = false; 
            _motor->drive(0); // Usar drive(0) para frenado activo
            _outputPWM = 0;   // Para el registro (log)
            _holding = true;  
            Serial.printf("[%lu] ✅ Posición alcanzada (asentada): %d ADC (Objetivo: %d)\n", millis(), currentADC, _targetADC);  
            _errorAnterior = 0; 
            _integral = 0;      
        }  
        // Si _holding es true y estamos dentro de tolerancia normal, el motor debe estar frenado (drive(0)).
        if (_holding) {  
             _motor->drive(0); // Mantener el freno activo en modo holding.
             _outputPWM = 0; 
             return _outputPWM; 
        }  
    }  

    // --- LÓGICA DE MOVIMIENTO ACTIVO ---
    if (_moving) {  
        if (errorAbs <= PID_PULSE_THRESHOLD) { // Si el error es suficientemente pequeño, usa los pulsos
            controlPorPulsos(currentADC);
            // controlPorPulsos asigna el _outputPWM internamente.
            // No hay RawPID_Unconstrained ni pO, iO, dO directo en este modo, asi que adaptamos el log.
            Serial.printf("[%lu] [PULSE MODE] E:%d, Target:%d, Current:%d -> PWM:%d (Pulsed)\n", 
                          millis(), error, _targetADC, currentADC, _outputPWM);
        } else { // Si el error es grande, usa el PID normal
            float pidOutputFloat_unconstrained_val; 
            // controlPorPID ahora maneja la llamada a _motor->drive() internamente,
            // _outputPWM se actualiza en el último paso para reflejar la acción.
            int calculated_PID_PWM = controlPorPID(currentADC, pidOutputFloat_unconstrained_val);  
            _outputPWM = calculated_PID_PWM;   
             // DEBUG PID ya está dentro de controlPorPID, no duplicar aquí.
        }
    } else {  
        _motor->drive(0); // Frenar el motor.
        _outputPWM = 0; 
    }  
      
    return _outputPWM;  
}  

// ... (el resto de tu archivo .cpp, incluyendo controlPorPulsos y controlPorPID completas) ...

// **NOTA IMPORTANTE PARA controlPorPulsos**
// Dentro de controlPorPulsos, el Serial.printf actual es:
// Serial.printf("PULSO- Dir: %s, Dur: %dms, Int: %dms, Err: %d, Obj: %d, Act: %d\n", ...);
// Este es un buen log interno para los pulsos. No duplicaría el [PULSE MODE] log cada vez que se llama a controlPorPulsos.
// El log "[PULSE MODE] DEBUG PID" que puse en update() es solo para *confirmar el cambio al modo pulsos*.
// Puedes probar con el log de controlPorPulsos como está, o añadirle millis() si quieres.

// --- controlPorPulsos (Modificado para usar constantes de config.h) ---
void PositionController::controlPorPulsos(int currentADC) { 
    int error = _targetADC - currentADC;
    int errorAbs = abs(error);

    Serial.printf("[DEBUG PULSE CFG] ErrorAbs: %d, Dur: %dms, Int: %dms\n", errorAbs, _currentPulseDuration, _currentPulseInterval);

    
    if (!_motor->isEnabled()) {
        _motor->enable();
    }

    // --- Duración de Pulso (Usa las nuevas constantes de config.h) ---
    if (errorAbs > PULSE_ERROR_THRESHOLD_HIGH) {  
        _currentPulseDuration = PULSE_DURATION_HIGH_ERROR; 
    } else if (errorAbs > PULSE_ERROR_THRESHOLD_MEDIUM) { 
        _currentPulseDuration = PULSE_DURATION_MEDIUM_ERROR; 
    } else {  
        _currentPulseDuration = PULSE_DURATION_LOW_ERROR;  
    } 

    // --- Intervalo entre Pulsos (Usa las nuevas constantes de config.h) ---
    if (errorAbs > PULSE_ERROR_THRESHOLD_HIGH) { 
        _currentPulseInterval = PULSE_INTERVAL_HIGH_ERROR; 
    } else if (errorAbs > PULSE_ERROR_THRESHOLD_MEDIUM) { 
        _currentPulseInterval = PULSE_INTERVAL_MEDIUM_ERROR; 
    } else { 
        _currentPulseInterval = PULSE_INTERVAL_LOW_ERROR; 
    }
                                            
    if (_outputPWM != 0) { // Si el motor está actualmente encendido (en un pulso)
        if (millis() >= _pulseEndTime) {
            _motor->drive(0); // Apagar y frenar
            _outputPWM = 0;
        }
    } 
    else if (millis() - _lastPulseTime > _currentPulseInterval) { // Si el motor está apagado y es hora de otro pulso
        _lastPulseTime = millis(); 
        
        int pwmValue = abs(PWM_MIN_MOTION_THRESHOLD); // Siempre positivo
        if (error > 0) { // Mover adelante
            _motor->drive(pwmValue);
            _outputPWM = pwmValue;
        } else { // Mover atrás
            _motor->drive(-pwmValue);
            _outputPWM = -pwmValue; // Para el log
        }
        _pulseEndTime = millis() + _currentPulseDuration; 
          
        Serial.printf("PULSO- Dir: %s, Dur: %dms, Int: %dms, Err: %d, Obj: %d, Act: %d\n",  
                     (error > 0 ? "Fwd" : "Rev"), _currentPulseDuration, _currentPulseInterval, 
                     error, _targetADC, currentADC);
    } 
    else { 
        _motor->drive(0); // Asegurarse de que el motor está realmente parado y frenado.
        _outputPWM = 0; 
    }
}



// --- controlPorPID (Versión final con compensación mejorada) ---  
int PositionController::controlPorPID(int currentADC, float& outPidOutputFloat_unconstrained) {  
    long now = millis();  
    
    // Declaración de variables
    int error = _targetADC - currentADC;  
    float pOut = 0, iOut = 0, dOut = 0, dt = 0;
    
    // Cálculo de dt con límites seguros
    if (_lastPIDTime == 0) dt = 0.001f;
    else dt = constrain((float)(now - _lastPIDTime) / 1000.0f, 0.005f, 0.1f);
    _lastPIDTime = now;  
   
    // Componente Proporcional
    pOut = _kp * error;  
    
    // Componente Integral con anti-windup adaptativo
    if (_ki != 0) {
        float currentEstimate = pOut + iOut + dOut;
        if (currentEstimate < PWM_MAX_USABLE_DRIVE && currentEstimate > -PWM_MAX_USABLE_DRIVE) {
            _integral += error * dt;  
            _integral = constrain(_integral, -2000.0f, 2000.0f);
        }
        iOut = _ki * _integral;  
    }
    
    // Componente Derivativo con filtro robusto
    if (_kd != 0 && dt > 0) {
        static float lastDerivada = 0;
        const float alpha = 0.2f;
        float rawDerivada = constrain((error - _errorAnterior) / dt, -500.0f, 500.0f);
        float derivada = alpha * rawDerivada + (1 - alpha) * lastDerivada;
        lastDerivada = derivada;
        dOut = _kd * derivada;
    }
    
    // Cálculo de salida PID
    float pidOutputFloat_raw = pOut + iOut + dOut; 
    outPidOutputFloat_unconstrained = pidOutputFloat_raw;  
    
    // APLICACIÓN DE COMPENSACIÓN DE ZONA MUERTA (NUEVA)
    int pidOutputInt = static_cast<int>(pidOutputFloat_raw);
    applyDeadzoneCompensation(pidOutputInt);
    
    // Constrain final y aplicación al motor
    int finalPWM = constrain(pidOutputInt, -PWM_MAX_USABLE_DRIVE, PWM_MAX_USABLE_DRIVE);
    _motor->drive(finalPWM); 
    
    // Debugging
    Serial.printf("[%lu] DEBUG PID: E:%d, pO:%.2f, iO:%.2f, dO:%.2f, RawPID:%.2f -> PWM:%d (dt:%.3f)\n",  
                  millis(), error, pOut, iOut, dOut, outPidOutputFloat_unconstrained, finalPWM, dt);
         
    _errorAnterior = error; 
    return finalPWM;  
}

/**
 * Aplica compensación no lineal a la señal PID para manejar la zona muerta del motor
 * donde el PWM debe superar 640 unidades para iniciar movimiento.
 * 
 * @param pidOutput - Referencia a la salida PID calculada (se modifica in-place)
 */
void PositionController::applyDeadzoneCompensation(int& pidOutput) {
    // Valor absoluto para simplificar cálculos
    const int absOutput = abs(pidOutput);
    
    // Si la salida está por debajo del umbral mínimo para movimiento
    if (absOutput < PWM_MIN_MOTION_THRESHOLD) {
        if (pidOutput != 0) {  // Solo compensar si hay demanda de movimiento
            // Determinar dirección (signo)
            const int sign = pidOutput > 0 ? 1 : -1;
            
            // Factor de curvatura (0.0 = lineal, 1.0 = muy agresivo)
            const float nonLinearFactor = 0.7f; 
            
            // Calcular proporción dentro de la zona muerta [0.0-1.0]
            const float proportion = static_cast<float>(absOutput) / PWM_MIN_MOTION_THRESHOLD;
            
            // Aplicar curva exponencial para enfatizar bajas velocidades
            const float curvedProportion = pow(proportion, nonLinearFactor);
            
            // Escalar al rango [0, PWM_MIN_MOTION_THRESHOLD]
            int compensatedOutput = static_cast<int>(
                curvedProportion * PWM_MIN_MOTION_THRESHOLD
            );
            
            // Volver a aplicar signo
            pidOutput = sign * constrain(
                compensatedOutput, 
                0, 
                PWM_MIN_MOTION_THRESHOLD
            );
        }
    } else {
        // Compresión suave del rango máximo
        const int scaledOutput = map(
            absOutput,
            PWM_MIN_MOTION_THRESHOLD,
            PWM_MAX_USABLE_DRIVE,
            PWM_MIN_MOTION_THRESHOLD + 30, // Espacio extra
            PWM_MAX_USABLE_DRIVE
        );
        
        pidOutput = (pidOutput > 0) ? scaledOutput : -scaledOutput;
    }
}