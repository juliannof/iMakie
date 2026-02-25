#ifndef CONFIG_H
#define CONFIG_H

#include <Wire.h>
#include <SPI.h>

const float stopThreshold = 0.1; // Definir el umbral de parada

// Definiciones de pines para la tarjeta SD
#define SD_CS       7
#define SD_MOSI     8
#define SD_SCLK     12
#define SD_MISO     4


// Pines para el GPS
#define GPS_RX_PIN  5
#define GPS_TX_PIN  6



// Bateria
#define ADC_PIN             14

// Botones fisicos
#define BUTTON_1            35


#endif // CONFIG_H