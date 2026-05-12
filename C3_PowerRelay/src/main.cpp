#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <AccelStepper.h>
#include "config.h"

// ============================================================================
// OBJETO ACCELSTEPPER
// ============================================================================
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// ============================================================================
// VARIABLES GLOBALES
// ============================================================================
bool wledState = false;  // false = apagado, true = encendido
unsigned long lastSensorRead = 0;
unsigned long lastSensorTriggerTime = 0;
unsigned long systemStartTime = 0;
bool relayTurnedOn = false;

// ============================================================================
// SENSOR ULTRASÓNICO
// ============================================================================
float measureDistance() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    long duration = pulseIn(ECHO_PIN, HIGH, TIMEOUT_US);
    
    if (duration == 0 || duration > TIMEOUT_US) {
        return -1;
    }
    
    return (duration * 0.0343) / 2;
}

// ============================================================================
// CONTROL WLED (TOGGLE - Alterna encendido/apagado)
// ============================================================================
void toggleWLED() {
    if (wledState) {
        // APAGAR WLED
        Serial.println("[WLED] Apagando...");
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
            delay(1);
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            http.begin("http://" WLED_IP "/win&T=0");
            int code = http.GET();
            http.end();
            
            if (code == 200) {
                wledState = false;
                Serial.println("[WLED] ✓ Apagado");
            } else {
                Serial.println("[WLED] ✗ Error al apagar");
            }
        } else {
            Serial.println("[WLED] ✗ WiFi timeout");
        }
        
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    } else {
        // ENCENDER WLED
        Serial.println("[WLED] Encendiendo...");
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
            delay(1);
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            http.begin("http://" WLED_IP "/win&PL=2");
            int code = http.GET();
            http.end();
            
            if (code == 200) {
                wledState = true;
                Serial.println("[WLED] ✓ Encendido");
            } else {
                Serial.println("[WLED] ✗ Error al encender");
            }
        } else {
            Serial.println("[WLED] ✗ WiFi timeout");
        }
        
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    }
}

// ============================================================================
// CONTROL DEL RELÉ
// ============================================================================
void relayOn() {
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println("[RELAY] 10V ON");
    //triggerWLED_on();  // Encender WLED con playlist
}

void relayOff() {
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("[RELAY] 10V OFF");
    //triggerWLED_off();  // Apagar WLED con T=0
}


// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    Serial.println("------------------------------------");
    Serial.println("Control de WLED por sensor ultrasónico");
    Serial.println("------------------------------------");
    
    // Configurar pines del sensor
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    // Configurar pin del relé
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    // Configurar pin de LED indicador (opcional)
    pinMode(8, OUTPUT);
    digitalWrite(8, HIGH);
    
    // Configurar motor (inicializado pero no se usa)
    pinMode(ENABLE_PIN, OUTPUT);
    digitalWrite(ENABLE_PIN, HIGH);
    stepper.setEnablePin(ENABLE_PIN);
    stepper.setPinsInverted(true, false, true);
    stepper.setMaxSpeed(MAX_SPEED);
    stepper.setAcceleration(ACCELERATION);
    stepper.disableOutputs();
    
    Serial.println("Sistema listo. Esperando detección del sensor...");
    systemStartTime = millis();
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
    unsigned long currentMillis = millis();
    
    // Leer sensor cada SENSOR_DELAY ms
    if (currentMillis - lastSensorRead > SENSOR_DELAY) {
        lastSensorRead = currentMillis;
        float distance = measureDistance();
        
        if (distance > 0 && distance < UMBRAL_DISTANCIA) {
            // Debounce para evitar múltiples detecciones rápidas
            if (currentMillis - lastSensorTriggerTime > 500) {
                lastSensorTriggerTime = currentMillis;
                
                Serial.print("[SENSOR] Detectado a ");
                Serial.print(distance);
                Serial.println(" cm");
                
                // Alternar WLED (enciende si está apagado, apaga si está encendido)
                toggleWLED();
                
                // LED indicador (opcional)
                digitalWrite(8, LOW);
                delay(200);
                digitalWrite(8, HIGH);
            }
        }
    }

    static bool done = false;
    static unsigned long t0 = millis();
    if (!done && millis() - t0 >= (RELAY_ACTIVATION_TIME * 1000)) {
        relayOn();
        done = true;
    }

    
    // El motor no hace nada - está deshabilitado
    // Si quieres que el motor nunca se mueva, asegúrate de que nunca se llame a stepper.move()
}