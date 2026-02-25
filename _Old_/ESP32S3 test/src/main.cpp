#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>

#define BME_ADDR 0x76 // Dirección I2C (0x77 si no funciona)

Adafruit_BME280 bme;

void setup() {
  Serial.begin(115200);
  #if ARDUINO_USB_CDC_ON_BOOT
    delay(3000); // Espera para USB-CDC
  #endif

  Wire.begin(5, 6); // SDA=GPIO5 (SDO), SCL=GPIO6 (SCK)
  
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