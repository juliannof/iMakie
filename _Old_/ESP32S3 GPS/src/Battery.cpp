#include "Battery.h"
#include "Config.h"
#include <Arduino.h>

Battery::Battery() {}

float Battery::getVoltage() const {
    uint16_t v = analogRead(ADC_PIN);  // Leer valor del ADC (0-4095)
    float voltage = ((float)v / 4095.0) * vRange * (vref / 1000.0) * rFactor;  // Convertir ADC a voltaje real de la batería
    return voltage;
}


int Battery::getChargeLevel() {
    float voltage = getVoltage();  // Obtener el voltaje utilizando la función constante
    int chargeLevel = map(voltage * 1000, 3300, int(fullChargeVoltage * 1000), 0, 100);
    return constrain(chargeLevel, 0, 100);
}

bool Battery::isCharging() {
    float voltage = getVoltage();  // Obtener el voltaje utilizando la función constante
    return voltage > chargingVoltage;
}

float Battery::getEmptyVoltage() const {
    return emptyVoltage;
}
