#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

#define SD_MISO 8              // Pin MISO de la tarjeta SD
#define SD_SCLK 9              // Pin SCLK de la tarjeta SD
#define SD_MOSI 20             // Pin MOSI de la tarjeta SD
#define SD_CS 7                // Pin CS de la tarjeta SD

bool testWritten = false; // Bandera para asegurar que solo se escriba una vez

void setup() {
  Serial.begin(115200);
  while (!Serial);
  // Inicializa el bus SPI
  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);

  // Inicializa la tarjeta SD
  if (!SD.begin(SD_CS)) {
    Serial.println("Error al inicializar la tarjeta SD.");
    while (true); // Detiene el programa en caso de error
  }
  
  Serial.println("Tarjeta SD inicializada correctamente.");
}

void loop() {
  if (!testWritten) {
    // Prueba de escritura en la tarjeta SD
    File file = SD.open("/test.txt", FILE_WRITE);
    if (file) {
      file.println("Hola, tarjeta SD!");
      file.close();
      Serial.println("Archivo escrito correctamente.");
      testWritten = true; // Establece la bandera para evitar escrituras repetidas
    } else {
      Serial.println("Error al abrir el archivo para escribir.");
    }
  }
  
  // No se necesita más código en el loop
  while (true); // Detiene el programa después de la prueba
}
