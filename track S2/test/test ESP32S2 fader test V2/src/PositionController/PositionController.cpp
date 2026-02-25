#include "PositionController.h"

PositionController::PositionController() 
    : _objetivo(0), _moviendo(false), _salidaPWM(0),
      _ultimoPulso(0), _tiempoParada(0), _duracionPulso(60) {
}

void PositionController::comenzar() {
    Serial.println("✅ Controlador de posición inicializado");
}

void PositionController::moverAPosicion(int posicionObjetivoADC) {
    _objetivo = constrain(posicionObjetivoADC, 0, ADC_MAX_VALUE);
    _moviendo = true;
    _ultimoPulso = 0;
    
    Serial.printf("🎯 Moviendo a posición: %d ADC\n", _objetivo);
}

void PositionController::actualizar(int posicionActualADC) {
    if (!_moviendo) return;
    
    controlPorPasos(posicionActualADC);
    
    // Verificar si llegamos a la posición
    int error = _objetivo - posicionActualADC;
    if (abs(error) <= POSITION_TOLERANCE) {
        _moviendo = false;
        _salidaPWM = 0;
        Serial.printf("✅ Posición alcanzada: %d ADC\n", posicionActualADC);
    }
}

void PositionController::controlPorPasos(int posicionActualADC) {
    int error = _objetivo - posicionActualADC;
    int errorAbs = abs(error);
    
    // Determinar duración del pulso basado en el error
    if (errorAbs > 800) {
        _duracionPulso = 100;
    } else if (errorAbs > 300) {
        _duracionPulso = 70;
    } else {
        _duracionPulso = 40;
    }
    
    // Determinar intervalo entre pulsos
    unsigned long intervalo;
    if (errorAbs > 500) {
        intervalo = 100;
    } else if (errorAbs > 150) {
        intervalo = 200;
    } else {
        intervalo = 400;
    }
    
    // Ejecutar pulso si es tiempo
    if (millis() - _ultimoPulso > intervalo) {
        _ultimoPulso = millis();
        _salidaPWM = (error > 0 ? MIN_PWM_DRIVE : -MIN_PWM_DRIVE);
        _tiempoParada = millis() + _duracionPulso;
        
        Serial.printf("🎯 Pulso: %dms, Error: %d\n", _duracionPulso, error);
    }
    
    // Parar después de la duración del pulso
    if (millis() >= _tiempoParada) {
        _salidaPWM = 0;
    }
}

void PositionController::detener() {
    _moviendo = false;
    _salidaPWM = 0;
    Serial.println("⏹️  Movimiento detenido");
}

bool PositionController::estaEnPosicion() {
    return !_moviendo;
}

bool PositionController::estaMoviendo() {
    return _moviendo;
}

int PositionController::obtenerObjetivo() {
    return _objetivo;
}

int PositionController::obtenerSalidaPWM() {
    return _salidaPWM;
}