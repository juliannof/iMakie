#include "NVSValidator.h"
#include "../config.h"
#include "../display/LovyanGFX_config.h"

extern LGFX tft;

// Valores por defecto desde config.h
#define EXPECTED_SSID   WIFI_SSID_DEFAULT
#define EXPECTED_PASS   WIFI_PASS_DEFAULT
#define EXPECTED_OTA    OTA_PASS_DEFAULT

// Variable estática para guardar el status
NVSStatus NVSValidator::lastStatus = NVSStatus::VALID;

NVSStatus NVSValidator::validate() {
    if (!checkNamespace()) {
        Serial.println("[NVS] Namespace corrupto o inexistente");
        lastStatus = NVSStatus::CORRUPTED;
        return NVSStatus::CORRUPTED;
    }

    if (!checkCriticalKeys()) {
        Serial.println("[NVS] Claves críticas inválidas o corregidas");
        lastStatus = NVSStatus::CORRUPTED;
        return NVSStatus::CORRUPTED;
    }

    Serial.println("[NVS] ✓ Válido");
    lastStatus = NVSStatus::VALID;
    return NVSStatus::VALID;
}

NVSStatus NVSValidator::getLastStatus() {
    return lastStatus;
}

bool NVSValidator::checkNamespace() {
    Preferences prefs;
    prefs.begin(NVS_NS, true);

    // Intenta leer una clave - si falla, namespace está corrupto
    uint8_t trackId = prefs.getUChar("trackId", 255);
    String ssid = prefs.getString("wifiSsid", "");

    prefs.end();

    // trackId 255 = nunca fue escrito (OK, primer boot)
    // Si trackId es válido (0-9) u otro rango, es OK
    // Si ssid está basura (0xFF...), entonces corrupto

    if (ssid.length() > 0 && ssid[0] == 0xFF) {
        return false;  // Basura en ssid
    }

    return true;  // OK
}

bool NVSValidator::checkCriticalKeys() {
    Preferences prefs;
    prefs.begin(NVS_NS, true);

    uint8_t trackId = prefs.getUChar("trackId", 255);
    String ssid = prefs.getString("wifiSsid", "");
    String pass = prefs.getString("wifiPass", "");
    String otaPass = prefs.getString("otaPass", "");

    prefs.end();

    bool needsCorrection = false;

    // Verificar trackId: debe ser 0-9 o 255 (sin asignar)
    if (trackId != 255 && trackId > 9) {
        Serial.printf("[NVS] trackId inválido: %u\n", trackId);
        needsCorrection = true;
    }

    // Verificar valores exactos WiFi
    if (ssid != EXPECTED_SSID) {
        Serial.printf("[NVS] wifiSsid incorrecto: '%s' → '%s'\n", ssid.c_str(), EXPECTED_SSID);
        needsCorrection = true;
    }
    if (pass != EXPECTED_PASS) {
        Serial.printf("[NVS] wifiPass incorrecto\n");
        needsCorrection = true;
    }
    if (otaPass != EXPECTED_OTA) {
        Serial.printf("[NVS] otaPass incorrecto\n");
        needsCorrection = true;
    }

    // Si hay basura (0xFF), es corruptible
    if (ssid.length() > 0 && (uint8_t)ssid[0] == 0xFF) {
        Serial.println("[NVS] wifiSsid contiene basura");
        needsCorrection = true;
    }

    if (needsCorrection) {
        Serial.println("[NVS] Corrigiendo valores...");
        Preferences prefs2;
        prefs2.begin(NVS_NS, false);
        prefs2.putString("wifiSsid", EXPECTED_SSID);
        prefs2.putString("wifiPass", EXPECTED_PASS);
        prefs2.putString("otaPass", EXPECTED_OTA);
        if (trackId == 255) {
            prefs2.putUChar("trackId", 0);  // Sin asignar = 0
        }
        prefs2.end();
        Serial.println("[NVS] ✓ Corregido");
        return false;  // Reportar que fue necesario corregir
    }

    return true;  // Todo OK
}

void NVSValidator::reset() {
    Serial.println("[NVS] Reescribiendo con valores por defecto...");

    // Mostrar en display
    displayError("NVS CORRUPTO\nReparando...");

    // Borrar namespace
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.clear();
    prefs.end();

    // Reescribir con valores por defecto
    prefs.begin(NVS_NS, false);
    prefs.putUChar("trackId", 0);
    prefs.putString("wifiSsid", EXPECTED_SSID);
    prefs.putString("wifiPass", EXPECTED_PASS);
    prefs.putString("otaPass", EXPECTED_OTA);
    prefs.end();

    Serial.println("[NVS] ✓ Reescrito con valores por defecto");
    delay(1500);

    // Reinicia limpio
    ESP.restart();
}

void NVSValidator::displayError(const char* msg) {
    // Intenta mostrar en display si está inicializado
    // Si no, solo va a Serial
    try {
        if (tft.width() > 0) {  // Verifica que display está inicializado
            tft.fillScreen(TFT_BLACK);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_RED);
            tft.setFont(&fonts::FreeSans12pt7b);
            tft.drawString(msg, tft.width() / 2, tft.height() / 2);
        }
    } catch (...) {
        // Display no inicializado, ignore
    }
}
