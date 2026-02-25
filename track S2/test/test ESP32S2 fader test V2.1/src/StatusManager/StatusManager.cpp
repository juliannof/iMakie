#include "StatusManager.h"
  
StatusManager::StatusManager()  
    : _sistemaListo(false),  
      _calibracionEnCurso(false),  
      _ultimoTiempoParpadeo(0),  
      _estadoLED(false),  
      _contadorParpadeo(0) {  
}  
  
void StatusManager::begin() {  
    configurarLED();  
    Serial.println("✅ StatusManager inicializado");  
    // iniciar con un parpadeo de indicación de encendido/reinicio
    parpadearLED(2, 50); // Dos parpadeos rápidos
}  
  
void StatusManager::configurarLED() {  
    pinMode(STATUS_LED, OUTPUT);  
    digitalWrite(STATUS_LED, LOW); // Asegurarse de que el LED esté apagado al inicio
}  
  
void StatusManager::setEstadoSistema(bool sistemaListo) {  
    _sistemaListo = sistemaListo;  
    _calibracionEnCurso = false; // Detener cualquier indicación de calibración
      
    if (_sistemaListo) {  
        digitalWrite(STATUS_LED, HIGH); // LED encendido = sistema listo  
        Serial.println("💡 Estado: Sistema LISTO");  
    } else {  
        digitalWrite(STATUS_LED, LOW);  // LED apagado = sistema no listo  
        Serial.println("💡 Estado: Sistema NO listo");  
    }  
}  
  
void StatusManager::indicarCalibracionEnCurso() {  
    _calibracionEnCurso = true;  
    _sistemaListo = false; // El sistema no está completamente listo mientras se calibra
    Serial.println("💡 Estado: Calibración EN CURSO");  
}  
  
void StatusManager::indicarCalibracionCompletada() {  
    _calibracionEnCurso = false; // Ya no está en curso
    _sistemaListo = true;        // El sistema ahora está listo
      
    // Patrón de parpadeo de confirmación
    parpadearLED(3, 200);  
    digitalWrite(STATUS_LED, HIGH); // LED fijo encendido al finalizar  
      
    //Serial.println("💡 Estado: Calibración COMPLETADA");  
}  
  
void StatusManager::indicarError() {  
    _calibracionEnCurso = false;  
    _sistemaListo = false;  
      
    // Parpadeo rápido de error
    parpadearLED(5, 100);  
    digitalWrite(STATUS_LED, LOW); // LED apagado o patrón de error final
      
    Serial.println("💡 Estado: ERROR del sistema");  
}  
  
void StatusManager::indicarEmergencia() {  
    _calibracionEnCurso = false;  
    _sistemaListo = false;  
      
    // Patrón de parpadeo muy rápido y repetitivo
    for(int i = 0; i < 3; i++) {  
        parpadearLED(2, 80);  
        //delay(150); // Pausa entre series de parpadeos
    }  
    digitalWrite(STATUS_LED, LOW); // LED apagado al finalizar
      
    Serial.println("💡 Estado: EMERGENCIA - Sistema detenido");  
}  
  
void StatusManager::actualizar() {  
    // Solo actualiza el patrón de "calibración en curso" de forma no bloqueante
    if (_calibracionEnCurso) {  
        if (millis() - _ultimoTiempoParpadeo > 500) { // Alternar cada 500ms
            _ultimoTiempoParpadeo = millis();  
            _estadoLED = !_estadoLED;  
            digitalWrite(STATUS_LED, _estadoLED);  
        }  
    }  
}  
  
void StatusManager::parpadearLED(int veces, int duracion) {  
    // Esta función es bloqueante y solo para patrones específicos puntuales
    for (int i = 0; i < veces; i++) {  
        digitalWrite(STATUS_LED, HIGH);  
        //delay(duracion);  
        digitalWrite(STATUS_LED, LOW);  
        //if (i < veces - 1) delay(duracion); // Evitar un delay extra al final del último parpadeo
    }  
}