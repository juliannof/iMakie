#include "PositionController.h"

// Constructor modificado para aceptar el puntero al sensor y con nuevas ganancias PID
PositionController::PositionController(FaderSensor* faderSensor) 
    : _targetADC(0), 
      _moving(false), 
      _outputPWM(0),
      _holding(false),              
      _sensor(faderSensor),         // Inicializar la referencia al sensor
      _lastPulseTime(0), 
      _pulseEndTime(0), 
      _currentPulseDuration(15),    // Ajuste de pulso inicial del constructor
      _currentPulseInterval(400),   // Ajuste de pulso inicial del constructor
      _kp(0.35),                     // Kp ajustado
      _ki(0.0),                      // Ki a CERO
      _kd(0.02),                     // Kd reducido
      _errorAnterior(0), _integral(0),
      _lastPIDTime(0)
{
    Serial.println("✅ PositionController inicializado");
}

// setTargetPosition ahora espera 1 argumento y usa _sensor internamente
void PositionController::setTargetPosition(int targetADC) {
    _targetADC = targetADC;
    _moving = true; 
    _holding = false;                
    Serial.printf("🎯 Objetivo ADC establecido: %d\n", _targetADC);
    _integral = 0;
    _errorAnterior = _sensor->readSmoothed(); // Lee el sensor internamente
    _lastPIDTime = millis();
}

void PositionController::setTargetPositionPercent(int targetPercent, int minADC, int maxADC) {
    int targetADC_calculated = map(targetPercent, 0, 100, minADC, maxADC);
    targetADC_calculated = constrain(targetADC_calculated, minADC, maxADC); 
    setTargetPosition(targetADC_calculated); // Llama con 1 argumento
}

// --- FUNCIÓN UPDATE PRINCIPAL CON LÓGICA DE DETECCIÓN Y TRANSICIÓN ---
int PositionController::update(int currentADC) {
    // --- DEBUG MUY VERBOSO (COMENTAR DESPUÉS DE LA PRUEBA) ---
    // En la versión final, esta línea podría comentarse para reducir el spam.
    // Aunque ahora es crucial para entender el "no para" o "loqueo".
    Serial.printf("DBG_UPD: E:%d, Tol:%d, Dir:%s, Move:%d, Hold:%d, PWM:%d\n", 
                  (_targetADC - currentADC), POSITION_TOLERANCE, 
                  ((_targetADC - currentADC) > 0 ? "Fwd" : ((_targetADC - currentADC) < 0 ? "Rev" : "Stop")),
                  _moving, _holding, _outputPWM);
    // --- FIN DEBUG ---

    if (!_moving && !_holding) { // Si no estamos moviendo ni manteniendo, no hacemos nada
        return 0; 
    }

    int error = _targetADC - currentADC;
    
    // --- LÓGICA DE DETECCIÓN DE POSICIÓN ALCANZADA ---
    if (abs(error) <= POSITION_TOLERANCE) {
        if (_moving) { // Si veníamos activamente moviéndonos y ahora estamos en tolerancia
            _moving = false; _outputPWM = 0; 
            _holding = true; // Entrar en modo mantenimiento
            Serial.printf("✅ Posición alcanzada (asentada): %d ADC (Objetivo: %d)\n", currentADC, _targetADC);
            _errorAnterior = 0; _integral = 0; // Reset PID para la próxima corrección
        }
        // Si _holding es true y estamos dentro de tolerancia, aplicar Holding PWM
        if (_holding) {
             // Aplicar un pequeño PWM en la dirección opuesta al error para "anclar" el fader
             if (abs(error) > 0) { // Si hay un mínimo error (no es perfectamente 0), aplicar HOLD_PWM
                 _outputPWM = (error > 0 ? HOLD_PWM_MAGNITUDE : -HOLD_PWM_MAGNITUDE);
                 Serial.printf(">>> DEBUG: MODO HOLDING ACTIVO (E:%d, PWM:%d)\n", error, _outputPWM);
             } else { // Si el error es 0, no aplicar nada.
                 _outputPWM = 0;
             }
             return 0; // Se requiere acción de holding, pero NO es un movimiento "Activo".
        }
    } 
    // --- FIN LÓGICA DE DETECCIÓN DE POSICIÓN ALCANZADA ---

    // --- LÓGICA DE CORRECCIÓN (SI ESTAMOS FUERA DE TOLERANCIA) ---
    if ((abs(error) > POSITION_TOLERANCE)) { // SIEMPRE que estemos fuera de tolerancia
        if (_holding && (!_moving)) { // Si estábamos en modo holding y nos movimos fuera de tolerancia, y _moving era false
            Serial.println("🔄 Salió de tolerancia en modo holding. Reactivando movimiento.");
            _moving = true; // Activar el modo movimiento
            _holding = false; // Ya no estamos solo "manteniendo"
            _lastPIDTime = millis(); // Resetear _lastPIDTime para un dt correcto.
        } // Si _moving ya es true, significa que estamos persiguiendo un nuevo objetivo y no hay cambio de estado.
    }
    // --- FIN LÓGICA DE CORRECCIÓN ---


    // --- CALCULO DE PWM (SOLO SI _moving = true) ---
    if (_moving) { // Solo si _moving es true, calculamos PWM
        float pidOutputFloat_raw = 0; // Para capturar la salida RAW del PID

        // Calcular siempre el PID, y obtener la salida RAW (antes del constrain a PID_MAX_DRIVE_HARDCODE)
        int calculated_PID_PWM = controlPorPID(currentADC, pidOutputFloat_raw);

        // === LÓGICA HÍBRIDA FINAL: PRIORIZAR PID PARA ERRORES GRANDES, PULSOS PARA AJUSTE ===
        // Si el error es grande (fuera de la zona de pulsos), SIEMPRE USA PID.
        // Si el error es menor o igual a PID_PULSE_THRESHOLD, USA PULSOS.
        if (abs(error) > PID_PULSE_THRESHOLD) { 
            _outputPWM = calculated_PID_PWM; // Si el error es grande, usa la salida del PID.
            Serial.printf(">>> DEBUG: MODO PID ACTIVO (Movimiento Suave - E:%d, RawPID:%.2f)\n", error, pidOutputFloat_raw); 
            // Si el PIDOutput_raw es < MIN_PWM_DRIVE aquí, el motor puede no moverse o moverse lento.
            // Es la zona de transición: el PID lo lleva hasta el umbral (PID_PULSE_THRESHOLD).
            // Si el PID_PULSE_THRESHOLD es grande, el PID sigue mandando aunque el RawPID sea bajo.
        } else { // Si el error es menor o igual a PID_PULSE_THRESHOLD (modo de ajuste fino)
            controlPorPulsos(currentADC); 
            // _outputPWM ya se establece dentro de controlPorPulsos (MIN_PWM_DRIVE o 0)
            Serial.printf(">>> DEBUG: MODO PULSOS ACTIVO (Ajuste Fino - E:%d, RawPID:%.2f)\n", error, pidOutputFloat_raw); 
        }
    } else {
        _outputPWM = 0; // Si _moving es false (se llegó al objetivo o está en holding y estable), motor parado.
    }
    
    return _outputPWM; 
}

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
        _outputPWM = (error > 0 ? MIN_PWM_DRIVE : -MIN_PWM_DRIVE); 
        _pulseEndTime = millis() + _currentPulseDuration; 
          
        Serial.printf("PULSO- Dir: %s, Dur: %dms, Int: %dms, Err: %d, Obj: %d, Act: %d\n",  
                     (error > 0 ? "Fwd" : "Rev"), _currentPulseDuration, _currentPulseInterval, 
                     error, _targetADC, currentADC);
    } 
    else { // Si no hay pulso activo y NO es hora de iniciar uno
        _outputPWM = 0; // Asegurarse de que el motor está realmente parado.
    }
}

// src/PositionController/PositionController.cpp - controlPorPID()
int PositionController::controlPorPID(int currentADC, float& outPidOutputFloat_raw) { 
    unsigned long now = millis();
    float dt = (now > _lastPIDTime) ? (float)(now - _lastPIDTime) / 1000.0f : 0.001f; 
    _lastPIDTime = now;

    if (dt < 0.005f) dt = 0.005f; 
    if (dt > 0.1f) dt = 0.1f;    

    int error = _targetADC - currentADC;

    float pOut = _kp * error;

    _integral += error * dt;
    _integral = constrain(_integral, -2000.0f, 2000.0f); 
    float iOut = _ki * _integral;

    float derivada = 0;
    if (dt > 0) {
        derivada = (error - _errorAnterior) / dt;
    }
    derivada = constrain(derivada, -2000.0f, 2000.0f); // Limitar la derivada para evitar picos
    float dOut = _kd * derivada;

    float pidOutputFloat_unconstrained = pOut + iOut + dOut; // EL REAL RawPID
    outPidOutputFloat_raw = pidOutputFloat_unconstrained; // Capturar este valor para `update()`

    // Aplicar el CONSTRAIN de RawPID_Unconstrained PARA EL CÁLCULO DEL PWM A ENVIAR A MOTOR
    float pidOutputFloat_constrained = constrain(pidOutputFloat_unconstrained, -PWM_MAX_DRIVE_HARDCODE, PWM_MAX_DRIVE_HARDCODE); 

    _errorAnterior = error; 

    // Aquí nos aseguramos DE QUE EL PWM MINIMO SEA RESPETADO cuando se envíe al motor.
    // La magnitud del PWM debe ser al menos MIN_PWM_DRIVE.
    int pwmMagnitude = abs(pidOutputFloat_constrained); 
    // Constreñir pwmMagnitude entre MIN_PWM_DRIVE y PWM_MAX_DRIVE_HARDCODE
    pwmMagnitude = constrain(pwmMagnitude, MIN_PWM_DRIVE, PWM_MAX_DRIVE_HARDCODE);

    int finalPWM = (error > 0) ? pwmMagnitude : -pwmMagnitude;

    // --- REVISIÓN DEL PRINTF ---
    Serial.printf("DEBUG PID: E:%d, pO:%.2f, iO:%.2f, dO:%.2f, RawPID:%.2f -> PWM:%d (dt:%.3f)\n", 
                  error, pOut, iOut, dOut, pidOutputFloat_unconstrained, finalPWM, dt);
    
    return finalPWM;
}

void PositionController::setPulseDuration(int duration) {
    _currentPulseDuration = constrain(duration, 10, 200); 
    Serial.printf("🔧 Duración de pulso establecida a %d ms\n", _currentPulseDuration);
}

void PositionController::setPulseInterval(int interval) {
    _currentPulseInterval = constrain(interval, 50, 1000); 
    Serial.printf("🔧 Intervalo de pulso establecido a %d ms\n", _currentPulseInterval);
}

// --- resetControllerState (Implementación) ---
void PositionController::resetControllerState() {
    _moving = false;
    _holding = false;
    _outputPWM = 0; // Asegurarse de que el motor se detenga.
    _errorAnterior = 0; // También limpiar el estado interno del PID.
    _integral = 0;
    Serial.println(">>> DEBUG: PositionController estado reseteado.");
}