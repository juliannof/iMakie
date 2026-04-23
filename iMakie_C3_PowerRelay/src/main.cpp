#include <Arduino.h>
#include <AccelStepper.h>
#include "config.h"

// ============================================================================
// OBJETO ACCELSTEPPER
// ============================================================================
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// ============================================================================
// MÁQUINA DE ESTADOS
// ============================================================================
typedef enum {
    SYSTEM_INIT,
    HOMING_START,
    HOMING_SEARCHING,
    HOMING_BACKOFF,
    IDLE_AT_HOME_POSITION,
    MOVING_AWAY_FROM_HOME,
    IDLE_AWAY_FROM_HOME,
    MOVING_TOWARDS_HOME,
    ERROR_ENDSTOP_UNEXPECTED,
    ERROR_HOME_NOT_FOUND,
    ERROR_GENERAL
} MotorState;

MotorState currentMotorState = SYSTEM_INIT;
MotorState lastStableState = IDLE_AT_HOME_POSITION;

// ============================================================================
// VARIABLES GLOBALES
// ============================================================================
volatile bool endstopTriggered = false;
unsigned long lastSensorRead = 0;
unsigned long errorStateStartTime = 0;

// ============================================================================
// CONTROL DEL RELÉ
// ============================================================================
void relayOn() {
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println("[RELAY] 10V ON");
}

void relayOff() {
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("[RELAY] 10V OFF");
}

// ============================================================================
// INTERRUPCIÓN FINAL DE CARRERA
// ============================================================================
void IRAM_ATTR handleEndstop() {
    endstopTriggered = true;
}

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
// RUTINA DE HOMING
// ============================================================================
void performHoming() {
    Serial.println("Iniciando secuencia de Homing...");
    currentMotorState = HOMING_SEARCHING;
    endstopTriggered = false;

    stepper.enableOutputs();
    stepper.setMaxSpeed(HOMING_SPEED);
    stepper.setAcceleration(ACCELERATION);
    stepper.moveTo(-MAX_TRAVEL_STEPS * 2);
    
    while (!endstopTriggered) {
        stepper.run();
    }

    Serial.println("Final de carrera encontrado.");
    stepper.stop();
    while (stepper.isRunning()) {
        stepper.run();
    }
    
    stepper.setCurrentPosition(0);
    Serial.println("Home establecido en posicion 0.");

    if (HOMING_BACK_OFF_STEPS > 0) {
        currentMotorState = HOMING_BACKOFF;
        Serial.print("Retrocediendo ");
        Serial.print(HOMING_BACK_OFF_STEPS);
        Serial.println(" pasos para liberar el final de carrera.");
        stepper.move(HOMING_BACK_OFF_STEPS);
        while (stepper.isRunning()) {
            stepper.run();
        }
        Serial.println("Retroceso completado.");
    }

    stepper.disableOutputs();
    stepper.setMaxSpeed(MAX_SPEED);
    currentMotorState = IDLE_AT_HOME_POSITION;
    Serial.println("Homing completado. Sistema en IDLE_AT_HOME_POSITION.");
    endstopTriggered = false;
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    Serial.println("------------------------------------");
    Serial.println("Sistema de control con AccelStepper");
    Serial.println("------------------------------------");
    
    // Configurar pines
    pinMode(ENABLE_PIN, OUTPUT);
    digitalWrite(ENABLE_PIN, HIGH);
    
    pinMode(ENDSTOP_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(ENDSTOP_PIN), handleEndstop, FALLING);
    
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);  // Relé apagado inicialmente
    
    // Configurar AccelStepper
    stepper.setEnablePin(ENABLE_PIN);
    stepper.setPinsInverted(true, false, true);
    stepper.setMaxSpeed(MAX_SPEED);
    stepper.setAcceleration(ACCELERATION);
    stepper.disableOutputs();

    // Ejecutar homing
    performHoming();
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
    // Manejar final de carrera
    if (endstopTriggered) {
        endstopTriggered = false;
        
        switch (currentMotorState) {
            case HOMING_SEARCHING:
                // Se maneja en performHoming()
                break;
                
            case MOVING_TOWARDS_HOME:
                Serial.println("FDC durante MOVING_TOWARDS_HOME: encontrado.");
                stepper.stop();
                while (stepper.isRunning()) {
                    stepper.run();
                }
                stepper.setCurrentPosition(0);
                
                if (HOMING_BACK_OFF_STEPS > 0) {
                    stepper.move(HOMING_BACK_OFF_STEPS);
                    Serial.println("FDC: Retrocediendo para liberar.");
                    while (stepper.isRunning()) {
                        stepper.run();
                    }
                }
                
                stepper.disableOutputs();
                currentMotorState = IDLE_AT_HOME_POSITION;
                Serial.println("Motor en IDLE_AT_HOME_POSITION despues de FDC.");
                
                // Apagar relé al volver a HOME
                relayOff();
                break;
                
            case MOVING_AWAY_FROM_HOME:
                Serial.println("ERROR: FDC activado mientras se movía lejos del Home!");
                stepper.stop();
                while (stepper.isRunning()) {
                    stepper.run();
                }
                stepper.disableOutputs();
                currentMotorState = ERROR_ENDSTOP_UNEXPECTED;
                errorStateStartTime = millis();
                break;
                
            case IDLE_AT_HOME_POSITION:
            case IDLE_AWAY_FROM_HOME:
                Serial.println("ADVERTENCIA: FDC activado en estado IDLE.");
                break;
                
            default:
                Serial.println("ERROR: FDC activado en estado desconocido/inesperado!");
                stepper.stop();
                while (stepper.isRunning()) {
                    stepper.run();
                }
                stepper.disableOutputs();
                currentMotorState = ERROR_GENERAL;
                errorStateStartTime = millis();
                break;
        }
        return;
    }

    // Ejecutar motor si está en movimiento
    if (stepper.isRunning()) {
        stepper.run();
    }

    // Lógica del sensor
    unsigned long currentMillis = millis();
    
    if (currentMillis - lastSensorRead > SENSOR_DELAY) {
        lastSensorRead = currentMillis;
        float distance = measureDistance();
        
        if (distance > 0 && distance < UMBRAL_DISTANCIA) {
            if (currentMotorState == IDLE_AT_HOME_POSITION || currentMotorState == IDLE_AWAY_FROM_HOME) {
                Serial.print("Sensor activado a ");
                Serial.print(distance);
                Serial.println(" cm. ");
                Serial.print("Estado actual: ");
                Serial.println(currentMotorState == IDLE_AT_HOME_POSITION ? "IDLE_AT_HOME_POSITION" : "IDLE_AWAY_FROM_HOME");

                stepper.enableOutputs();
                
                if (currentMotorState == IDLE_AT_HOME_POSITION) {
                    Serial.println("Inicio de MOVING_AWAY_FROM_HOME.");
                    currentMotorState = MOVING_AWAY_FROM_HOME;
                    stepper.moveTo(MAX_TRAVEL_STEPS);
                } else if (currentMotorState == IDLE_AWAY_FROM_HOME) {
                    Serial.println("Inicio de MOVING_TOWARDS_HOME.");
                    currentMotorState = MOVING_TOWARDS_HOME;
                    stepper.moveTo(0);
                }
            }
        }
    }

    // Máquina de estados para movimientos completados
    switch (currentMotorState) {
        case MOVING_AWAY_FROM_HOME:
            if (!stepper.isRunning() && stepper.currentPosition() == MAX_TRAVEL_STEPS) {
                Serial.println("Movimiento MOVING_AWAY_FROM_HOME completado.");
                stepper.disableOutputs();
                currentMotorState = IDLE_AWAY_FROM_HOME;
                Serial.println("Motor en IDLE_AWAY_FROM_HOME.");
                
                // *** ACTIVAR RELÉ AL COMPLETAR MOVIMIENTO ADELANTE ***
                relayOn();
                
            } else if (!stepper.isRunning() && stepper.currentPosition() != MAX_TRAVEL_STEPS) {
                Serial.print("ERROR: Movimiento MOVING_AWAY_FROM_HOME detenido inesperadamente en pos ");
                Serial.println(stepper.currentPosition());
                stepper.disableOutputs();
                currentMotorState = ERROR_GENERAL;
                errorStateStartTime = millis();
            }
            break;

        case MOVING_TOWARDS_HOME:
            if (!stepper.isRunning()) {
                Serial.println("ERROR: Movimiento MOVING_TOWARDS_HOME completado a 0 sin FDC!");
                stepper.disableOutputs();
                currentMotorState = ERROR_HOME_NOT_FOUND;
                errorStateStartTime = millis();
            }
            break;
        
        case ERROR_ENDSTOP_UNEXPECTED:
        case ERROR_HOME_NOT_FOUND:
        case ERROR_GENERAL:
            if (currentMillis - errorStateStartTime < 5000) {
                // Mostrar error por 5 segundos
            } else {
                // Recuperación o permanencia en error
            }
            break;

        case IDLE_AT_HOME_POSITION:
        case IDLE_AWAY_FROM_HOME:
            // Esperando activación del sensor
            break;
        
        default:
            Serial.println("ERROR: Estado del sistema desconocido. Reinicio necesario.");
            currentMotorState = ERROR_GENERAL;
            errorStateStartTime = millis();
            break;
    }
}
