#ifndef CONFIG_H
#define CONFIG_H

#include <Wire.h>
#include <SPI.h>

const float stopThreshold = 0.1; // Definir el umbral de parada



// Definiciones de pines para la tarjeta SD
#define SD_CS       4
#define SD_MOSI     1
#define SD_SCLK     2
#define SD_MISO     3


// Pines para el GPS
#define GPS_RX_PIN  5
#define GPS_TX_PIN  6



// Bateria
#define ADC_PIN             6

// Botones fisicos
#define BUTTON_1            35


#endif // CONFIG_H