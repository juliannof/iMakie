#include "StatusManager.h"

StatusManager::StatusManager() 
    : _sistemaListo(false), 
      _calibracionEnCurso(false),
      _ultimoTiempoParpadeo(0),
      _estadoLED(false) {
}

void StatusManager::begin() {
    configurarLED();
    Serial.println("✅ StatusManager inicializado");
}

void StatusManager::configurarLED() {
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, LOW);
}

void StatusManager::setEstadoSistema(bool sistemaListo) {
    _sistemaListo = sistemaListo;
    _calibracionEnCurso = false;
    
    if (_sistemaListo) {
        digitalWrite(STATUS_LED, HIGH);
        Serial.println("💡 Estado: Sistema LISTO");
    } else {
        digitalWrite(STATUS_LED, LOW);
        Serial.println("💡 Estado: Sistema NO listo");
    }
}

void StatusManager::indicarCalibracionEnCurso() {
    _calibracionEnCurso = true;
    _sistemaListo = false;
    Serial.println("💡 Estado: Calibración EN CURSO");
}

void StatusManager::indicarCalibracionCompletada() {
    _calibracionEnCurso = false;
    _sistemaListo = true;
    
    parpadearLED(3, 200);
    digitalWrite(STATUS_LED, HIGH);
    
    Serial.println("💡 Estado: Calibración COMPLETADA");
}

void StatusManager::indicarError() {
    _calibracionEnCurso = false;
    _sistemaListo = false;
    
    parpadearLED(5, 100);
    digitalWrite(STATUS_LED, LOW);
    
    Serial.println("💡 Estado: ERROR del sistema");
}

void StatusManager::indicarEmergencia() {
    _calibracionEnCurso = false;
    _sistemaListo = false;
    
    for(int i = 0; i < 3; i++) {
        parpadearLED(2, 80);
        delay(150);
    }
    digitalWrite(STATUS_LED, LOW);
    
    Serial.println("💡 Estado: EMERGENCIA - Sistema detenido");
}

void StatusManager::actualizar() {
    if (_calibracionEnCurso) {
        if (millis() - _ultimoTiempoParpadeo > 500) {
            _ultimoTiempoParpadeo = millis();
            _estadoLED = !_estadoLED;
            digitalWrite(STATUS_LED, _estadoLED);
        }
    }
}

void StatusManager::parpadearLED(int veces, int duracion) {
    for (int i = 0; i < veces; i++) {
        digitalWrite(STATUS_LED, HIGH);
        delay(duracion);
        digitalWrite(STATUS_LED, LOW);
        if (i < veces - 1) delay(duracion);
    }
}