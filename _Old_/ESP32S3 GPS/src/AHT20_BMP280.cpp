#include "AHT20_BMP280.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// Crear una instancia del sensor BME280
Adafruit_BME280 bme;

bool AHT20_BME280::begin() {
    bool status = true;

    // Inicializar BME280
    if (!bme.begin(0x77)) {  // Buscar el sensor en la dirección 0x77 (por defecto)
        Serial.println("No se encontró el BME280. Verifique el cableado.");
        status = false;
    } else {
        Serial.println("BME280 encontrado.");
    }

    return status;
}

void AHT20_BME280::update() {
    // Read and store sensor values
    temperature = bme.readTemperature();
    humidity = bme.readHumidity();
    pressure = bme.readPressure() / 100.0F;

    // Print values to the serial console
    //Serial.println("Temperatura: " + String(temperature));
    //Serial.println("Humedad: " + String(humidity));
    //Serial.println("Presión: " + String(pressure));
}

float AHT20_BME280::getTemperature() const {
    return temperature;
}

float AHT20_BME280::getHumidity() const {
    return humidity;
}

float AHT20_BME280::getPressure() const {
    return pressure;
}