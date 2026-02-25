#ifndef CONFIG_H
#define CONFIG_H

// config.h
#pragma once

// Define el pin del botón
#define BUTTON_PIN  0

// Configuración de WiFi
#define WLAN_SSID       "Julianno-WiFi"
#define WLAN_PASS       "JULIANf1"

// Configuración de MQTT
#define MQTT_SERVER "192.168.1.200"
#define MQTT_SERVERPORT 1883 // Usa 8883 para SSL
#define MQTT_USERNAME ""
#define MQTT_KEY ""
#define DEVICE_ID "iRoomba"

// Servidor NTP
extern const char* ntpServer; // Solo declaración
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 0;

#endif // CONFIG_H
