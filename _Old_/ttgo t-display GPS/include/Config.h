#ifndef CONFIG_H
#define CONFIG_H

#include <Wire.h>
#include <SPI.h>

const float stopThreshold = 0.1; // Definir el umbral de parada



// Definiciones de pines para la tarjeta SD
#define SD_CS      33
#define SD_SCLK    25
#define SD_MISO    27
#define SD_MOSI    26

// Pines para el GPS
#define GPS_RX_PIN 15
#define GPS_TX_PIN 2

// Bateria
#define ADC_EN              14  //ADC_EN is the ADC detection enable port
#define ADC_PIN             34

// Botones fisicos
#define BUTTON_1            35
#define BUTTON_2            0

#endif // CONFIG_H