#pragma once

#include <Arduino.h>
#include "../config.h" // Se asume que config.h está un nivel arriba
  
class FaderSensor {
public:
    FaderSensor();
    void begin();
    int readRaw();        // Lee el valor ADC crudo
    int readSmoothed();   // Lee el valor ADC suavizado por promedio móvil
    bool isStable(int currentValue, int lastValue); // Compara si dos valores son estables
      
private:
    int _adcBuffer[ADC_BUFFER_SIZE]; // Buffer para el promedio móvil
    int _adcIndex;                   // Índice actual del buffer
      
    void initADC(); // Configura el ADC del ESP32
};