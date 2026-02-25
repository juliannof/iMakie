#include <Arduino.h>
#include "functions.h"
#include "button.h"  // Incluir el archivo del botón

unsigned long previousMillis = 0;
const long interval = 100;
unsigned long lastButtonCheck = 0;
const long buttonInterval = 20;

// Nota: La instancia buttonHandler está definida en button.cpp

void setup() {
    Serial.begin(115200);
    setupDisplay();
    setupWiFi();
    setupClock();
    setupSensors();
    
    buttonHandler.begin(); // Inicializar la instancia global
    Serial.println("System initialized");
}

void loop() {
    unsigned long currentMillis = millis();
    
    // Comprobar el botón más frecuentemente
    if (currentMillis - lastButtonCheck >= buttonInterval) {
        lastButtonCheck = currentMillis;
        buttonHandler.update();
        
        if (buttonHandler.wasPressed()) {
            Serial.println("Button pressed!");
            // Aquí puedes añadir una función para cambiar modos de pantalla
        }
    }
    
    // Lectura de sensores a intervalo fijo
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        readAndDisplayData();
    }
}