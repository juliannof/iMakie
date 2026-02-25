#ifndef AHT20_BMP280_H
#define AHT20_BMP280_H

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

class AHT20_BME280 {
public:
    bool begin();
    void update(); // No arguments, just updates internal state
    float getTemperature() const;
    float getHumidity() const;
    float getPressure() const;

private:
    Adafruit_BME280 bme;
    float temperature;
    float humidity;
    float pressure;
};

#endif

