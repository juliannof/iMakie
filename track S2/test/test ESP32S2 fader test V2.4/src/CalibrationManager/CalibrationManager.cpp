#include "CalibrationManager.h"
  
CalibrationManager::CalibrationManager()  
    : _estado(INACTIVO),  
      _tiempoInicioCalibracion(0),  
      _ultimoTiempoEstable(0),  
      _ultimoValorEstable(0) {  
}  
  
void CalibrationManager::comenzar() {  
    _preferencias.begin("fader-calib", false); // Abrir espacio de preferencias "fader-calib" en modo lectura/escritura
    cargarCalibracion();                      // Intentar cargar datos al inicio
    Serial.println("✅ CalibrationManager inicializado");
}  
  
bool CalibrationManager::iniciarCalibracion() {  
    if (_estado != INACTIVO && _estado != COMPLETADO && _estado != ERROR) {  
        Serial.println("⚠️  Calibración ya en curso o en estado de transición.");  
        return false;  
    }  
      
    reiniciarEstadoCalibracion(); // Resetear los datos de calibración y el estado
    _estado = BUSCANDO_MAXIMO;  
    _tiempoInicioCalibracion = millis();  
    _ultimoTiempoEstable = millis();  
    _ultimoValorEstable = 0; // Se actualizará en la primera lectura
      
    Serial.println("🔄 CALIBRACIÓN INICIADA - Moviendo a límite SUPERIOR...");  
    return true;  
}  
  
// src/CalibrationManager/CalibrationManager.cpp - Dentro de la clase CalibrationManager

void CalibrationManager::actualizarCalibracion(int adcActual) {  
    // Si la calibración no está activa (está inactiva, completada o con error), simplemente salimos.
    if (_estado == INACTIVO || _estado == COMPLETADO || _estado == ERROR) return;  
      
    // --- Verificación de Timeout General ---
    // Si ha pasado más tiempo del permitido para toda la calibración, la marcamos como ERROR.
    if (millis() - _tiempoInicioCalibracion > CALIB_TIMEOUT) {  
        _datosCalibracion.mensajeError = "Timeout en calibración";  
        _estado = ERROR;  
        Serial.println("❌ TIMEOUT en calibración");  
        Serial.println(">>> DEBUG: CalibrationManager establece estado a ERROR por TIMEOUT"); // <-- MENSAJE DE DEBUG
        return; // Salir de la función ya que la calibración falló.
    }  
      
    // --- Lógica para Detectar Estabilidad del Sensor ---
    // Si el valor actual de ADC es estable (dentro del umbral) con respecto al último valor estable conocido...
    if (abs(adcActual - _ultimoValorEstable) <= ADC_STABILITY_THRESHOLD) {  
        // ... Y ha permanecido estable por un tiempo suficiente (CALIB_STABLE_TIME)...
        if (millis() - _ultimoTiempoEstable > CALIB_STABLE_TIME) {  
            // Estabilidad detectada, procedemos según la fase actual de la calibración.
            if (_estado == BUSCANDO_MAXIMO) {  
                // Se encontró el límite superior.
                _datosCalibracion.posicionMaximaADC = adcActual;  
                Serial.printf("✅ Límite SUPERIOR detectado: %d ADC\n", _datosCalibracion.posicionMaximaADC);  
                  
                _estado = BUSCANDO_MINIMO;             // Transición al siguiente estado: buscar el mínimo.
                _ultimoTiempoEstable = millis();       // Reiniciamos el temporizador de estabilidad para la nueva búsqueda.
                _ultimoValorEstable = adcActual;       // Establecemos el valor actual como el nuevo punto de referencia estable.
                Serial.println("🔄 Ahora moviendo a límite INFERIOR...");  
                Serial.println(">>> DEBUG: CalibrationManager establece estado a BUSCANDO_MINIMO"); // <-- MENSAJE DE DEBUG
            }  
            else if (_estado == BUSCANDO_MINIMO) {  
                // Se encontró el límite inferior.
                _datosCalibracion.posicionMinimaADC = adcActual;  
                Serial.printf("✅ Límite INFERIOR detectado: %d ADC\n", _datosCalibracion.posicionMinimaADC);  
                  
                // Una vez encontrados ambos límites, validamos si la calibración es físicamente lógica.
                if (validarCalibracion()) {  
                    _estado = COMPLETADO;              // Marcamos la calibración como exitosa.
                    _datosCalibracion.calibrado = true; // Establecemos el flag de calibrado.
                    guardarCalibracion();             // Guardamos los datos en la memoria no volátil (NVS).
                    Serial.println("🎉 CALIBRACIÓN COMPLETADA Y GUARDADA");  
                    Serial.println(">>> DEBUG: CalibrationManager INTERNAMENTE establece estado a COMPLETADO"); // <-- MENSAJE DE DEBUG
                } else {  
                    _estado = ERROR; // La calibración no pasó la validación.
                    _datosCalibracion.mensajeError = "Rangos de calibración inválidos.";
                    Serial.println("❌ Calibración no válida: Los rangos son muy pequeños o invertidos.");  
                    Serial.println(">>> DEBUG: CalibrationManager establece estado a ERROR por NO VÁLIDA"); // <-- MENSAJE DE DEBUG
                }  
            }  
        }  
    } else {  
        // Si el valor no es estable (se está moviendo o fluctuando demasiado), reiniciamos el conteo de tiempo estable.
        _ultimoTiempoEstable = millis();  
        _ultimoValorEstable = adcActual;  
    }  
    // DESCOMENTADA: Para ver el estado en CADA llamada a actualizarCalibracion (lo que queremos ahora)
    //Serial.printf(">>> DEBUG: CalibrationManager estado actual al salir: %d (INACTIVO: -1, COMPLETADO: 2)\n", _estado); 
}
  
void CalibrationManager::abortarCalibracion() {  
    if (_estado != INACTIVO && _estado != COMPLETADO && _estado != ERROR) {  
        Serial.println("🚨 CALIBRACIÓN ABORTADA POR USUARIO");  
        _estado = INACTIVO;  
        _datosCalibracion.mensajeError = "Calibración abortada por usuario.";  
    }  
}  
  
bool CalibrationManager::estaCalibrando() const {  
    return (_estado == BUSCANDO_MAXIMO || _estado == BUSCANDO_MINIMO);  
}  
  
bool CalibrationManager::validarCalibracion() {  
    // Simplemente verificar que el máximo es mayor que el mínimo y que haya un rango razonable
    if (_datosCalibracion.posicionMaximaADC > _datosCalibracion.posicionMinimaADC + 100) { // Al menos 100 unidades ADC de diferencia
        return true;  
    }  
    _datosCalibracion.mensajeError = "Rango calibrado demasiado pequeño o invertido.";
    return false;  
}  
  
void CalibrationManager::guardarCalibracion() {  
    _preferencias.putInt("minADC", _datosCalibracion.posicionMinimaADC);  
    _preferencias.putInt("maxADC", _datosCalibracion.posicionMaximaADC);  
    _preferencias.putBool("calibrado", _datosCalibracion.calibrado);  
    // No guardamos el mensaje de error, es transitorio
    _preferencias.end(); // Cerrar las preferencias después de escribir
    Serial.println("💾 Datos de calibración guardados.");
}  
  
bool CalibrationManager::cargarCalibracion() {  
    _datosCalibracion.posicionMinimaADC = _preferencias.getInt("minADC", 0); // 0 es el valor por defecto si no existe
    _datosCalibracion.posicionMaximaADC = _preferencias.getInt("maxADC", ADC_MAX_VALUE); // ADC_MAX_VALUE por defecto
    _datosCalibracion.calibrado = _preferencias.getBool("calibrado", false); // false por defecto
    _preferencias.end(); // Cerrar las preferencias después de leer
      
    if (_datosCalibracion.calibrado) {  
        Serial.printf("⚙️  Calibración cargada: Min=%d, Max=%d\n", _datosCalibracion.posicionMinimaADC, _datosCalibracion.posicionMaximaADC);  
        _estado = COMPLETADO; // Si los datos están cargados y válidos, el estado es COMPLETADO
        return true;  
    }  
    Serial.println("⚙️  No se encontraron datos de calibración guardados o no son válidos.");
    return false;  
}  
  
const DatosCalibracion& CalibrationManager::obtenerDatosCalibracion() const {  
    return _datosCalibracion;  
}  
  
CalibrationManager::EstadoCalibracion CalibrationManager::obtenerEstado() const {  
    return _estado;  
}  

unsigned long CalibrationManager::obtenerTiempoEstable() const {
    return _ultimoTiempoEstable;
}

int CalibrationManager::obtenerUltimoValorEstable() const {
    return _ultimoValorEstable;
}
  
void CalibrationManager::reiniciarEstadoCalibracion() {  
    _datosCalibracion.posicionMinimaADC = 0;  
    _datosCalibracion.posicionMaximaADC = ADC_MAX_VALUE;  
    _datosCalibracion.calibrado = false;  
    _datosCalibracion.mensajeError = "";  
    _estado = INACTIVO;  
    _tiempoInicioCalibracion = 0;  
    _ultimoTiempoEstable = 0;  
    _ultimoValorEstable = 0;  
}