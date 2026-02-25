#include <Arduino.h>
#include <Wire.h>

#define PIN_SDA 4
#define PIN_SCL 5
#define SLAVE_ADDR 0x08

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(PIN_SDA, PIN_SCL); // Master
  Wire.setClock(100000);

  Serial.println("S3 MASTER OK");
  Serial.println("e -> escanear");
  Serial.println("r -> enviar 'Hola S2'");
}

void loop() {
  if (!Serial.available()) return;

  char cmd = Serial.read();

  if (cmd == 'e') {
    Serial.println("Escaneando I2C...");
    bool found = false;
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        Serial.print("Dispositivo en 0x"); Serial.println(addr, HEX);
        found = true;
      }
    }
    if (!found) Serial.println("No hay dispositivos");
    Serial.println("Fin escaneo\n");
  }

  if (cmd == 'r') {
    Wire.beginTransmission(SLAVE_ADDR);
    Wire.write("Hola S2");
    Wire.endTransmission();
    Serial.println("Mensaje enviado\n");
  }
}
