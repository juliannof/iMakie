#include "WifiConfig.h"

// Constructor
WifiConfig::WifiConfig() {
    // Configuración inicial si es necesaria
}

// Método para iniciar la conexión WiFi
void WifiConfig::begin() {
    // Cambia el nombre del punto de acceso a "AITEC iMoto AP"
    if (!wm.autoConnect("AITEC iMoto AP")) {
        Serial.println("No se pudo conectar a la red WiFi.");
    } else {
        Serial.println("Conectado a la red WiFi!");
    }
}

// Método para comprobar si está conectado a WiFi
bool WifiConfig::isConnected() {
    return WiFi.isConnected();
}

// Método para desconectar el WiFi y ahorrar batería
void WifiConfig::disconnect() {
    WiFi.disconnect(true);  // Desconecta y apaga el WiFi
    WiFi.mode(WIFI_OFF);    // Asegura que el WiFi esté apagado
    Serial.println("WiFi desconectado y apagado");
}
