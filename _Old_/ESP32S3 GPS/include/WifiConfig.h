#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <WiFi.h>
#include <WiFiManager.h>

class WifiConfig {
public:
    WifiConfig();
    void begin();
    bool isConnected();
    void disconnect();

private:
    WiFiManager wm;  // Instancia privada de WiFiManager
};

#endif // WIFI_CONFIG_H
