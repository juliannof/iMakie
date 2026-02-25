#include "CalibrationManager.h"

CalibrationManager::CalibrationManager() 
    : _estado(INACTIVO), 
      _tiempoInicioCalibracion(0), 
      _ultimoTiempoEstable(0), 
      _ultimoValorEstable(0) {
}

void CalibrationManager::comenzar() {
    _preferencias.begin("fader-calib", false);
    cargarCalibracion();
}

bool CalibrationManager::iniciarCalibracion() {
    if (_estado != INACTIVO) {
        Serial.println("⚠️  Calibración ya en curso");
        return false;
    }
    
    _estado = BUSCANDO_MAXIMO;
    _tiempoInicioCalibracion = millis();
    _ultimoTiempoEstable = millis();
    _ultimoValorEstable = 0;
    
    Serial.println("🔄 CALIBRACIÓN INICIADA - Buscando límite SUPERIOR...");
    return true;
}

void CalibrationManager::actualizarCalibracion(int adcActual) {
    if (_estado == INACTIVO || _estado == COMPLETADO || _estado == ERROR) return;
    
    // Timeout general
    if (millis() - _tiempoInicioCalibracion > CALIB_TIMEOUT) {
        _datosCalibracion.mensajeError = "Timeout en calibración";
        _estado = ERROR;
        Serial.println("❌ TIMEOUT en calibración");
        return;
    }
    
    // Detectar estabilidad
    if (abs(adcActual - _ultimoValorEstable) <= ADC_STABILITY_THRESHOLD) {
        if (millis() - _ultimoTiempoEstable > CALIB_STABLE_TIME) {
            // Estabilidad detectada
            if (_estado == BUSCANDO_MAXIMO) {
                _datosCalibracion.posicionMaximaADC = adcActual;
                Serial.printf("✅ Límite SUPERIOR encontrado: %d\n", _datosCalibracion.posicionMaximaADC);
                
                _estado = BUSCANDO_MINIMO;
                _ultimoTiempoEstable = millis();
                _ultimoValorEstable = adcActual;
                Serial.println("🔄 Buscando límite INFERIOR...");
            } 
            else if (_estado == BUSCANDO_MINIMO) {
                _datosCalibracion.posicionMinimaADC = adcActual;
                Serial.printf("✅ Límite INFERIOR encontrado: %d\n", _datosCalibracion.posicionMinimaADC);
                
                if (validarCalibracion()) {
                    _estado = COMPLETADO;
                    _datosCalibracion.calibrado = true;
                    guardarCalibracion();
                    Serial.println("🎉 CALIBRACIÓN COMPLETADA Y GUARDADA");
                } else {
                    _estado = ERROR;
                    Serial.println("❌ Calibración no válida");
                }
            }
        }
    } else {
        // Reiniciar temporizador de estabilidad
        _ultimoTiempoEstable = millis();
        _ultimoValorEstable = adcActual;
    }
}

void CalibrationManager::abortarCalibracion() {
    if (_estado != INACTIVO) {
        Serial.println("🚨 CALIBRACIÓN ABORTADA POR USUARIO");
        _estado = INACTIVO;
        _datosCalibracion.mensajeError = "Abortada por usuario";
    }
}

bool CalibrationManager::estaCalibrando() {
    return _estado == BUSCANDO_MAXIMO || _estado == BUSCANDO_MINIMO;
}

bool CalibrationManager::validarCalibracion() {
    // Verificar que min < max
    if (_datosCalibracion.posicionMinimaADC >= _datosCalibracion.posicionMaximaADC) {
        _datosCalibracion.mensajeError = "MIN >= MAX (" + String(_datosCalibracion.posicionMinimaADC) + " >= " + String(_datosCalibracion.posicionMaximaADC) + ")";
        Serial.println("   ❌ " + _datosCalibracion.mensajeError);
        return false;
    }
    
    // Verificar rango mínimo
    int range = _datosCalibracion.posicionMaximaADC - _datosCalibracion.posicionMinimaADC;
    int minRequiredRange = ADC_MAX_VALUE / 2;
    
    if (range < minRequiredRange) {
        _datosCalibracion.mensajeError = "Rango insuficiente: " + String(range) + " < " + String(minRequiredRange);
        Serial.println("   ❌ " + _datosCalibracion.mensajeError);
        return false;
    }
    
    Serial.print("   ✅ Rango válido: ");
    Serial.print(range);
    Serial.print(" (");
    Serial.print((range * 100) / ADC_MAX_VALUE);
    Serial.println("% del total)");
    
    _datosCalibracion.calibrado = true;
    return true;
}

void CalibrationManager::guardarCalibracion() {
    _preferencias.putBytes("calib", &_datosCalibracion, sizeof(_datosCalibracion));
    Serial.println("💾 Calibración guardada");
}

bool CalibrationManager::cargarCalibracion() {
    size_t loaded = _preferencias.getBytes("calib", &_datosCalibracion, sizeof(_datosCalibracion));
    return (loaded == sizeof(_datosCalibracion) && _datosCalibracion.calibrado);
}

const DatosCalibracion& CalibrationManager::obtenerDatosCalibracion() {
    return _datosCalibracion;
}

int CalibrationManager::obtenerMinimoADC() {
    return _datosCalibracion.posicionMinimaADC;
}

int CalibrationManager::obtenerMaximoADC() {
    return _datosCalibracion.posicionMaximaADC;
}

int CalibrationManager::porcentajeAADC(int porcentaje) {
    porcentaje = constrain(porcentaje, 0, 100);
    return map(porcentaje, 0, 100, 
               _datosCalibracion.posicionMinimaADC, 
               _datosCalibracion.posicionMaximaADC);
}

int CalibrationManager::adcAPorcentaje(int adc) {
    return map(adc, 
               _datosCalibracion.posicionMinimaADC,
               _datosCalibracion.posicionMaximaADC,
               0, 100);
}

CalibrationManager::EstadoCalibracion CalibrationManager::obtenerEstado() {
    return _estado;
}

unsigned long CalibrationManager::obtenerTiempoEstable() {
    return _ultimoTiempoEstable;
}

int CalibrationManager::obtenerUltimoValorEstable() {
    return _ultimoValorEstable;
}

void CalibrationManager::reiniciarEstadoCalibracion() {
    _estado = INACTIVO;
    _tiempoInicioCalibracion = 0;
    _ultimoTiempoEstable = 0;
    _ultimoValorEstable = 0;
}