#ifndef FUNCTIONS_H
#define FUNCTIONS_H

// functions.h
#pragma once

// Declaración adelantada de ButtonHandler
class ButtonHandler;
#include <cstdint>

// Estructura para almacenar datos de los sensores
struct SensorData {
    float temperatureAHT20;
    float humidityAHT20;
    float temperatureBME280;
    float humidityBME280;
    float pressure;
    int aqi;
    uint32_t tvoc;
    uint32_t eco2;
};

// Declaraciones de funciones
void setupDisplay();
void setupWiFi();
void setupSensors();
void setupClock();
SensorData collectSensorData();
void displaySensorData(const SensorData& data);
void logSensorData(const SensorData& data); // Asegúrate de que esta declaración esté presente
void readAndDisplayData();

// Declaración de la instancia global del botón
extern ButtonHandler buttonHandler;

#endif