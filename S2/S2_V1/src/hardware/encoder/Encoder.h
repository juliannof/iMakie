#pragma once
#include <Arduino.h>

class Encoder {
public:
    static void begin();
    static void update();           // vacío — ISR lo maneja todo
    static long getCount();
    static bool hasChanged();
    static void reset();            // solo resetea delta, NO currentVPotLevel
    static int  currentVPotLevel;

private:
    static volatile long  _counter;
    static long           _lastReported;
    static volatile uint8_t _state;

    static void IRAM_ATTR _isr();
};