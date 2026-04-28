#include "NVSValidator.h"
#include "../config.h"
#include "../display/LovyanGFX_config.h"

extern LGFX tft;

NVSStatus NVSValidator::validate() {
    if (!checkNamespace()) {
        Serial.println("[NVS] Namespace corrupto o inexistente");
        return NVSStatus::CORRUPTED;
    }

    if (!checkCriticalKeys()) {
        Serial.println("[NVS] Claves críticas inválidas");
        return NVSStatus::CORRUPTED;
    }

    Serial.println("[NVS] ✓ Válido");
    return NVSStatus::VALID;
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

    // Verificar que trackId es razonable (0-9 o sin asignar 255)
    if (trackId != 255 && trackId > 9) {
        Serial.printf("[NVS] trackId inválido: %u\n", trackId);
        return false;
    }

    // Verificar que strings no tienen basura (0xFF repetido)
    if (ssid.length() > 0 && (uint8_t)ssid[0] == 0xFF) {
        Serial.println("[NVS] wifiSsid contiene basura");
        return false;
    }

    return true;
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
    prefs.putUChar("trackId", 0);           // Sin ID asignado
    prefs.putString("wifiSsid", "");        // Vacío
    prefs.putString("wifiPass", "");        // Vacío
    prefs.putString("otaPass", "");         // Vacío
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
