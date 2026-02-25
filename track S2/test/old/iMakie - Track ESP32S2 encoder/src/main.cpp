#include <Arduino.h>
#if CONFIG_IDF_TARGET_ESP32S2
  #include "USB.h"
  #include "USBCDC.h"
  USBCDC USBSerial;
#endif

// ¡Pines del ESP32 a conectar según la especificación del fabricante (A, C, B)!
const int ENCODER_PIN_A = 12; // Pin A del encoder
const int ENCODER_PIN_B = 13; // Pin B del encoder

volatile long currentCounter = 0;          // Contador final de "clicks" del encoder
int lastKnownState = 0;                    // Último estado (combinación A+B) conocido del encoder
unsigned long lastDebounceTime = 0;        // Tiempo del último cambio válido para debounce
volatile unsigned int DEBOUNCE_DELAY_MS = 15; // Retardo de debounce inicial, ajusta con '+'/'='
volatile int stepsSinceLastClick = 0;      // Contador de las 4 sub-transiciones que forman un "click"

// --- Variable para la Polaridad ---
// Con la circuitería externa (pull-ups a 3.3V y el encoder cerrando a GND),
// el estado "activo" es LOW y el "inactivo" es HIGH.
// La lógica del switch espera esta polaridad, por lo que INVERTED_POLARITY debe ser 'false'.
const bool INVERTED_POLARITY = false; 
// ---

void setup() {
  #if CONFIG_IDF_TARGET_ESP32S2
    USB.begin();
    USBSerial.begin(115200);
    while (!USBSerial) delay(10);
    #define Serial USBSerial
  #else
    Serial.begin(115200);
  #endif
  
  pinMode(ENCODER_PIN_A, INPUT); // PinMode INPUT es correcto con pull-ups EXTERNOS
  pinMode(ENCODER_PIN_B, INPUT);
  
  Serial.println("\n--- CONTADOR Y AJUSTE DE DEBOUNCE (PINOUT CONOCIDO) ---");
  Serial.println(String("Encoder: A en GPIO") + ENCODER_PIN_A + ", B en GPIO" + ENCODER_PIN_B + " (fabricante A,C,B)");
  Serial.println("Asegurate que pull-ups (4.7k), series (470R) y condensadores (470nF) estan en cada via.");
  
  // Leer estado inicial (directamente, ya que INVERTED_POLARITY es false)
  lastKnownState = (digitalRead(ENCODER_PIN_A) << 1) | digitalRead(ENCODER_PIN_B);
  
  lastDebounceTime = millis();
  Serial.println(String("\nDebounce inicial en ") + DEBOUNCE_DELAY_MS + "ms. Usa '+' o '-' para ajustar.");
  Serial.println("El contador final se actualiza cada 4 sub-pasos (un 'click' del encoder).");
}

void loop() {
  // Leer los pines
  // Con INVERTED_POLARITY = false, currentStateA/B son las lecturas directas.
  int currentStateA = digitalRead(ENCODER_PIN_A);
  int currentStateB = digitalRead(ENCODER_PIN_B);
  
  int newCurrentState = (currentStateA << 1) | currentStateB;
  
  // Solo procesamos si el estado ha cambiado y ha pasado suficiente tiempo de debounce
  if (newCurrentState != lastKnownState && (millis() - lastDebounceTime > DEBOUNCE_DELAY_MS)) {
    Serial.print("D("); Serial.print(DEBOUNCE_DELAY_MS); Serial.print("ms) ");
    Serial.print("Raw A:"); Serial.print(currentStateA); Serial.print(" Raw B:"); Serial.print(currentStateB);
    Serial.print(" -> Patron:"); Serial.print(newCurrentState); 
    
    int changeOneStep = 0; // Para el cambio de UNA de las 4 sub-transiciones
    
    // Lógica estándar del encoder de cuadratura para determinar la dirección de la sub-transición
    switch(lastKnownState) {
        case 0b00: if(newCurrentState == 0b01) changeOneStep = 1; else if(newCurrentState == 0b10) changeOneStep = -1; break;
        case 0b01: if(newCurrentState == 0b11) changeOneStep = 1; else if(newCurrentState == 0b00) changeOneStep = -1; break;
        case 0b10: if(newCurrentState == 0b00) changeOneStep = 1; else if(newCurrentState == 0b11) changeOneStep = -1; break;
        case 0b11: if(newCurrentState == 0b10) changeOneStep = 1; else if(newCurrentState == 0b01) changeOneStep = -1; break;
    }

    if (changeOneStep != 0) { // Si es una transición válida (no "Ninguna/Invalida" en el switch)
        stepsSinceLastClick += changeOneStep; // Acumula el cambio de la sub-transición
        Serial.print(" | Sub-pasos: "); Serial.print(stepsSinceLastClick);

        // Si hemos acumulado 4 sub-transiciones en cualquier dirección, es un "click" completo
        if (abs(stepsSinceLastClick) >= 4) {
             // Determina la dirección del click y actualiza el contador final
             // El signo de stepsSinceLastClick indica la dirección predominante
             currentCounter += (stepsSinceLastClick > 0) ? 1 : -1; 
             Serial.print(" | Cont Final: "); Serial.print(currentCounter);
             Serial.print(" | Dir: "); Serial.println((stepsSinceLastClick > 0) ? "Derecha" : "Izquierda");
             stepsSinceLastClick = 0; // Reinicia el contador de sub-pasos para el siguiente click
        } else {
            // Este es un paso intermedio dentro de un click completo.
            // No actualiza el contador 'final' todavía.
            Serial.println(" | - sub -"); 
        }
    } else { // Si changeOneStep es 0 (transición no válida según la lógica del switch)
        // Esto indica una transición "ilegal" o ruido en la señal.
        Serial.print(" | Cont Actual: "); Serial.print(currentCounter); // Muestra el contador final sin cambios
        Serial.println(" | Dir: Ninguna/Invalida");
    }
        
    lastKnownState = newCurrentState; // Actualiza el estado conocido después de procesar
    lastDebounceTime = millis();     // Reinicia el temporizador de debounce
  }
  
  // Ajuste dinámico del retardo de debounce a través del Monitor Serial
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == '+') {
      DEBOUNCE_DELAY_MS += 10;
      Serial.print("-> Debounce+ a "); Serial.print(DEBOUNCE_DELAY_MS); Serial.println("ms");
    } else if (cmd == '-') {
      if (DEBOUNCE_DELAY_MS > 10) DEBOUNCE_DELAY_MS -= 10; // Asegura que no baje de 10ms
      Serial.print("-> Debounce- a "); Serial.print(DEBOUNCE_DELAY_MS); Serial.println("ms");
    }
  }
  delay(1); // Pequeño retardo para ceder ciclos de CPU y estabilidad
}
