#pragma once

#include <Arduino.h>
#include "../config.h"

class FaderSensor {
public:
    FaderSensor();
    void begin();
    int leerSuavizado();
    int readSmoothed();
    bool isStable(int currentValue, int lastValue);
    
private:
    int _adcBuffer[ADC_BUFFER_SIZE];
    int _adcIndex;
    
    void initADC();
};