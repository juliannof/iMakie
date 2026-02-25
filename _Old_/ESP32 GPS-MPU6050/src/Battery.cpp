#include "Battery.h"
#include "Config.h"
#include <Arduino.h>

const float rFactor = 5.28; // Factor basado en el divisor de voltaje con resistencias 46kΩ y 99kΩ
const float vRange = 2.0;    // Rango del ADC (0-3.3V)
const float vref = 1100.0;   // Referencia del ADC en milivoltios (1.1V)


// Definir la instancia de la clase Battery
Battery battery;

// Definir el constructor de la clase Battery
Battery::Battery() {
    // Inicialización de variables, si es necesario
}

float Battery::getVoltage() const {
    uint16_t v = analogRead(ADC_PIN);
    float voltage = ((float)v / 4095.0) * vRange * (vref / 1000.0) * rFactor;  // Convertir ADC a voltaje real de la batería    
    return voltage;
}

int Battery::getBatteryLevel() {
    float voltage = getVoltage();  // Obtener el voltaje utilizando la función constante
    int chargeLevel = map(voltage * 1000, 3300, int(fullChargeVoltage * 1000), 0, 100);
    return constrain(chargeLevel, 0, 100);
}

