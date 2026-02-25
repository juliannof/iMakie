#include "Encoder.h"

// Variables estáticas
int Encoder::lastStateA = HIGH;
int Encoder::lastStateB = HIGH;
unsigned long Encoder::lastDebounceTime = 0;
volatile long Encoder::counter = 0;
long Encoder::lastPrintedValue = 0;
bool Encoder::initialized = false;
int currentVPotLevel = 0;

// ¡Definición de la variable estática de la clase!
int Encoder::currentVPotLevel = 0;
// =======================
// Implementación
// =======================

void Encoder::begin() {
    pinMode(PIN_A, INPUT);
    pinMode(PIN_B, INPUT);
    lastStateA = digitalRead(PIN_A);
    lastStateB = digitalRead(PIN_B);
    lastDebounceTime = millis();
    initialized = true;
    Serial.println("✅ Encoder inicializado - Izquierda:-1 Derecha:+1");
}

void Encoder::update() {
    if (!initialized) return;

    int currentStateA = digitalRead(PIN_A);
    int currentStateB = digitalRead(PIN_B);

    if (millis() - lastDebounceTime > DEBOUNCE_DELAY) {
        if (currentStateA != lastStateA) {
            if (currentStateA == LOW) {

                if (currentStateB == HIGH) {
                    counter--;   // Derecha
                } else {
                    counter++;   // Izquierda
                }

                // --- LIMITAR RANGO ---
                if (counter > 7)  counter = 7;
                if (counter < -7) counter = -7;
            }

            lastStateA = currentStateA;
            lastDebounceTime = millis();
        }

        if (currentStateB != lastStateB) {
            lastStateB = currentStateB;
            lastDebounceTime = millis();
        }
    }
}


long Encoder::getCount() {
    return counter;
}

void Encoder::reset() {
    Encoder::currentVPotLevel = 0;  // <<<< importante
    counter = 0;
    lastPrintedValue = 0;
}

bool Encoder::hasChanged() {
    bool changed = (counter != lastPrintedValue);
    if (changed) lastPrintedValue = counter;
    return changed;
}

String Encoder::getDirection() {
    // Por ahora solo devuelve N/A, puedes implementarlo si quieres
    return "N/A";
}

void Encoder::printStatus() {
    if (hasChanged()) {
        Serial.print("Encoder: ");
        Serial.print(counter);

        if (counter > lastPrintedValue) {
            Serial.println(" ➡️ (Derecha)");
        } else {
            Serial.println(" ⬅️ (Izquierda)");
        }
    }
}
