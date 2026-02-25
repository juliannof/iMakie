// src/comUA/UARTHandler.h
#pragma once
#include <Arduino.h>

void initUART();
void handleUART();
void sendToPico(byte* midi_message, byte len);

/**
 * @brief Devuelve si la conexión con Logic Pro está activa.
 * @return true si el handshake se ha completado, false en caso contrario.
 */
bool isLogicConnected();
void onHostQueryReceived(); // <-- AÑADE ESTA LÍNEA