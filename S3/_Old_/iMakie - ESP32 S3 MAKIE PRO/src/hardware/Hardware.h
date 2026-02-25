// src/hardware/Hardware.h
#pragma once
#include <Arduino.h>

// --- DECLARACIONES DE FUNCIONES PÚBLICAS ---
// Aquí solo "prometemos" que estas funciones existen en alguna parte.

void initHardware();
void handleHardwareInputs();
void handleVUMeterDecay();
void updateLeds();
void setTrellisBrightness(uint8_t newBrightness);
void resetToStandbyState(); 