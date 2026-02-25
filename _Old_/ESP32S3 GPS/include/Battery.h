#ifndef BATTERY_H
#define BATTERY_H

class Battery {
public:
    Battery();
    float getVoltage() const; // Marcado como constante
    int getChargeLevel();
    bool isCharging();
    float getEmptyVoltage() const;

private:
    mutable float battery_voltage; // Marcado como mutable para poder modificarlo en una función const
    const float vref = 1100.0;
    const float rFactor = 2.52;
    const float vRange = 3.3;
    const float fullChargeVoltage = 3.8;
    const float chargingVoltage = 4.5;
    const float emptyVoltage = 3.0;
};

#endif // BATTERY_H
