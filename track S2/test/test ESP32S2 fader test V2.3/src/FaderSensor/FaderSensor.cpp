#include "FaderSensor.h"
  
FaderSensor::FaderSensor()  
    : _adcIndex(0) {  
    // Inicializar el buffer con ceros
    memset(_adcBuffer, 0, sizeof(_adcBuffer));  
}  
  
void FaderSensor::begin() {  
    pinMode(POT_PIN, INPUT); // Configurar el pin del potenciómetro como entrada
    initADC();               // Inicializar la configuración del ADC
      
    // Llenar el buffer inicialmente para tener un valor suavizado desde el principio
    for (int i = 0; i < ADC_BUFFER_SIZE; i++) {  
        _adcBuffer[i] = analogRead(POT_PIN);  
        delay(10); // Pequeña pausa entre lecturas
    }  
    Serial.println("✅ FaderSensor inicializado");
}  
  
void FaderSensor::initADC() {  
    analogReadResolution(12); // Configurar resolución a 12 bits (0-4095)
    analogSetAttenuation(ADC_11db); // Rango de entrada de 0 a 3.6V para ~3.3V
}  
  
int FaderSensor::readRaw() {  
    return analogRead(POT_PIN); // Lectura directa del ADC
}  
  
int FaderSensor::readSmoothed() {  
    // Almacenar la nueva lectura en el buffer
    _adcBuffer[_adcIndex] = readRaw();  
    // Mover el índice al siguiente, cíclicamente
    _adcIndex = (_adcIndex + 1) % ADC_BUFFER_SIZE;  
      
    // Calcular la suma de todos los elementos del buffer
    long sum = 0;  
    for (int i = 0; i < ADC_BUFFER_SIZE; i++) {  
        sum += _adcBuffer[i];  
    }  
    // Devolver el promedio
    return sum / ADC_BUFFER_SIZE;  
}  
  
bool FaderSensor::isStable(int currentValue, int lastValue) {  
    // Determina si el valor actual y el último están dentro de un umbral de estabilidad
    return abs(currentValue - lastValue) <= ADC_STABILITY_THRESHOLD;  
}