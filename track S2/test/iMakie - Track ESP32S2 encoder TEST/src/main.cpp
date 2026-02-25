#include <Arduino.h>
#if CONFIG_IDF_TARGET_ESP32S2
  #include "USB.h"
  #include "USBCDC.h"
  USBCDC USBSerial;
#endif

// --- VARIABLES ENCAPSULADAS ---
namespace Encoder {
  const int PIN_A = 13;
  const int PIN_B = 12;
  const unsigned long DEBOUNCE_DELAY = 5;
  
  int lastStateA = HIGH;
  int lastStateB = HIGH;
  unsigned long lastDebounceTime = 0;
  long counter = 0;
  long lastPrintedValue = 0;
  bool initialized = false;

  // --- FUNCIONES PÚBLICAS ---
  void begin() {
    pinMode(PIN_A, INPUT);
    pinMode(PIN_B, INPUT);
    lastStateA = digitalRead(PIN_A);
    lastStateB = digitalRead(PIN_B);
    lastDebounceTime = millis();
    initialized = true;
    Serial.println("✅ Encoder inicializado - Izquierda:1 Derecha:2");
  }

  void update() {
    if (!initialized) return;
    
    int currentStateA = digitalRead(PIN_A);
    int currentStateB = digitalRead(PIN_B);
    
    if (millis() - lastDebounceTime > DEBOUNCE_DELAY) {
      if (currentStateA != lastStateA) {
        if (currentStateA == LOW) {
          // CORRECCIÓN: Invertir la dirección según tu hardware
          if (currentStateB == HIGH) {
            counter++;  // DERECHA (2)
          } else {
            counter--;  // IZQUIERDA (1)
          }
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

  long getCount() {
    return counter;
  }

  void reset() {
    counter = 0;
    lastPrintedValue = 0;
  }

  bool hasChanged() {
    bool changed = (counter != lastPrintedValue);
    if (changed) {
      lastPrintedValue = counter;
    }
    return changed;
  }

  String getDirection() {
    // Retorna la dirección del último movimiento
    // Para uso futuro si necesitas saber específicamente la dirección
    return "N/A";
  }

  void printStatus() {
    if (hasChanged()) {
      Serial.print("Encoder: ");
      Serial.print(counter);
      
      // Indicar dirección del último cambio
      if (counter > lastPrintedValue) {
        Serial.println(" ➡️ (Derecha)");
      } else {
        Serial.println(" ⬅️ (Izquierda)");
      }
    }
  }
}

// --- SETUP Y LOOP ---
void setup() {
  #if CONFIG_IDF_TARGET_ESP32S2
    USB.begin();
    USBSerial.begin(115200);
    while (!USBSerial) delay(10);
    #define Serial USBSerial
  #else
    Serial.begin(115200);
  #endif
  
  Encoder::begin();
  Serial.println("\n--- ENCODER CON DIRECCIONES CORRECTAS ---");
  Serial.println("Izquierda: -1, Derecha: +1");
}

void loop() {
  Encoder::update();
  Encoder::printStatus();
  delay(1);
}