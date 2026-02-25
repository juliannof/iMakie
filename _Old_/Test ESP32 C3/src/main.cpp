#include <Arduino.h> // Incluye el core de Arduino
#include <Wire.h>
#include <Adafruit_BME280.h>

#define SDA_PIN 8  // GPIO8 (conectado a SDO en tu placa)
#define SCL_PIN 9  // GPIO9 (conectado a SCK en tu placa)

#include <Wire.h>
#include <Adafruit_BME280.h>

#define BME_ADDR 0x76 // Dirección I2C (0x77 si no funciona)

Adafruit_BME280 bme;

void setup() {
  Serial.begin(115200);
  #if ARDUINO_USB_CDC_ON_BOOT
    delay(3000); // Espera para USB-CDC
  #endif

  Wire.begin(SDA_PIN, SCL_PIN); // ¡Configuración correcta!
  
  if (!bme.begin(BME_ADDR)) {
    Serial.println("¡Error al iniciar BME280!");
    while (1);
  }
  Serial.println("BME280 listo");
}

void loop() {
  Serial.print("Temperatura: "); Serial.print(bme.readTemperature()); Serial.println(" °C");
  Serial.print("Humedad: "); Serial.print(bme.readHumidity()); Serial.println(" %");
  Serial.print("Presión: "); Serial.print(bme.readPressure() / 100.0); Serial.println(" hPa");
  Serial.println("-------------------");
  delay(2000);
}