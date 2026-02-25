#include "Battery.h"
#include "Config.h"
#include <Arduino.h>

Battery::Battery() {}

float Battery::getVoltage() const {
    uint16_t v = analogRead(ADC_PIN);
    float voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
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
