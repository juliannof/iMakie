#ifndef CONFIG_H
#define CONFIG_H

#include <Wire.h>
#include <SPI.h>

const float stopThreshold = 0.1; // Definir el umbral de parada


// Pines para el GPS
#define GPS_RX_PIN  12
#define GPS_TX_PIN  13

// Bateria
#define ADC_PIN      6



#endif // CONFIG_H