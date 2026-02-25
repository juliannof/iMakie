#include "FaderSensor.h"

FaderSensor::FaderSensor() 
    : _adcIndex(0) {
    memset(_adcBuffer, 0, sizeof(_adcBuffer));
}

void FaderSensor::begin() {
    pinMode(POT_PIN, INPUT);
    initADC();
    
    // Inicializar buffer
    for (int i = 0; i < ADC_BUFFER_SIZE; i++) {
        _adcBuffer[i] = analogRead(POT_PIN);
        delay(10);
    }
}

void FaderSensor::initADC() {
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
}

int FaderSensor::readSmoothed() {
    _adcBuffer[_adcIndex] = analogRead(POT_PIN);
    _adcIndex = (_adcIndex + 1) % ADC_BUFFER_SIZE;
    
    long sum = 0;
    for (int i = 0; i < ADC_BUFFER_SIZE; i++) {
        sum += _adcBuffer[i];
    }
    return sum / ADC_BUFFER_SIZE;
}

int FaderSensor::leerSuavizado() {
    return readSmoothed();
}

bool FaderSensor::isStable(int currentValue, int lastValue) {
    return abs(currentValue - lastValue) <= ADC_STABILITY_THRESHOLD;
}