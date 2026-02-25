#ifndef BLE_H
#define BLE_H

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// Definiciones de funciones y variables
extern bool deviceConnected;
extern BLEServer *pServer;

// Declaraciones de las funciones
void setupBLE();
void onConnect();
void onDisconnect();
void startAdvertising();
void sendData();

#endif
