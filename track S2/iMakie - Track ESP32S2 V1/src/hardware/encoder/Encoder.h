#pragma once
#include <Arduino.h>

class Encoder {
public:
    static void begin();        // Inicializa los pines y el estado
    static void update();       // Actualiza el estado del encoder
    static long getCount();     // Devuelve el contador
    static bool hasChanged();   // True si cambió desde la última vez
    static void reset();        // Resetea el contador
    static void printStatus();  // Imprime estado por Serial
    static String getDirection(); // Retorna dirección (opcional)

    // --- Nivel vPot integrado ---
    static int currentVPotLevel;   // Nivel de vPot de -7 a +7

private:
    static const int PIN_A = 13;
    static const int PIN_B = 12;
    static const unsigned long DEBOUNCE_DELAY = 3;

    static int lastStateA;
    static int lastStateB;
    static unsigned long lastDebounceTime;
    static volatile long counter;
    static long lastPrintedValue;
    static bool initialized;
};
