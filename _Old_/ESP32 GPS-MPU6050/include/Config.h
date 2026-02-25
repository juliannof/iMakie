#ifndef CONFIG_H
#define CONFIG_H

#include <Wire.h>
#include <SPI.h>

const float stopThreshold = 0.1; // Definir el umbral de parada


// Pines para el GPS
#define GPS_RX_PIN  16
#define GPS_TX_PIN  17

// Bateria
#define ADC_PIN             39

// Botones fisicos
#define BUTTON_1            35


#endif // CONFIG_H