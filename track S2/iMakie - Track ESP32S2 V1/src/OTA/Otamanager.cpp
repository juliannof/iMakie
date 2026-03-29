// ============================================================
//  OtaManager.cpp  —  iMakie PTxx Track S2
// ============================================================
#include "OtaManager.h"

#include <WiFi.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <Preferences.h>

#define NVS_NS          "ptxx"
#define NVS_SSID        "wifiSsid"
#define NVS_PASS        "wifiPass"
#define NVS_OTA_PASS    "otaPass"

#define PORTAL_SSID     "iMakie-PTxx"
#define PORTAL_TIMEOUT  120
#define OTA_PORT        3232

OtaManager otaManager;

// ─────────────────────────────────────────────────────────────
void OtaManager::begin() {
    // NO llamar WiFi.mode() aquí — bug GDMA en ESP32-S2 + IDF5
    _otaActive = false;
}

// ─────────────────────────────────────────────────────────────
void OtaManager::tick() {
    if (_otaActive) {
        ArduinoOTA.handle();
    }
}

// ─────────────────────────────────────────────────────────────
void OtaManager::launchPortal() {
    // NO llamar WiFi.mode() — WiFiManager gestiona el modo internamente
    _status("AP: iMakie-PTxx");
    delay(50);
    _status("Conecta y abre 192.168.4.1");
    delay(50);

    Preferences prefs;
    prefs.begin(NVS_NS, true);
    uint8_t currentId = prefs.getUChar("trackId", 0);
    prefs.end();
    char defaultId[4];
    snprintf(defaultId, sizeof(defaultId), "%u", currentId);

    WiFiManager wm;
    wm.setConfigPortalTimeout(PORTAL_TIMEOUT);
    wm.setConnectTimeout(15);
    wm.setBreakAfterConfig(true);

    WiFiManagerParameter paramOtaPass("otapass", "OTA Password", "", 32);
    WiFiManagerParameter paramTrackId("trackid", "Track ID (1-17)", defaultId, 3);
    wm.addParameter(&paramOtaPass);
    wm.addParameter(&paramTrackId);

    wm.setAPCallback([this](WiFiManager* wm) {
        _status("Portal activo...");
    });

    bool connected = wm.startConfigPortal(PORTAL_SSID);

    if (connected || wm.getWiFiSSID().length() > 0) {
        String ssid = wm.getWiFiSSID();
        String pass = wm.getWiFiPass();
        String ota  = String(paramOtaPass.getValue());
        _saveCredentials(ssid.c_str(), pass.c_str(), ota.c_str());
        _status("Credenciales guardadas.");
    } else {
        _status("Portal: sin cambios.");
    }

    uint8_t newId = (uint8_t)atoi(paramTrackId.getValue());
    if (newId >= 1 && newId <= 9) {
        Preferences prefs2;
        prefs2.begin(NVS_NS, false);
        prefs2.putUChar("trackId", newId);
        prefs2.end();
        static char buf[32];
        snprintf(buf, sizeof(buf), "Track ID: %u guardado.", newId);
        _status(buf);
    }

    // NO llamar WiFi.mode(WIFI_OFF) — bug GDMA
    _otaActive = false;
}

// ─────────────────────────────────────────────────────────────
void OtaManager::enableForUpload() {
    char ssid[64]    = {};
    char pass[64]    = {};
    char otaPass[33] = {};

    if (!_loadCredentials(ssid, pass, otaPass)) {
        _status("Sin credenciales. Config WiFi primero.");
        return;
    }

    _status("Conectando WiFi...");

    // NO llamar WiFi.mode(WIFI_STA) — WiFi.begin() lo gestiona
    WiFi.begin(ssid, pass);

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
        delay(250);
    }

    if (WiFi.status() != WL_CONNECTED) {
        _status("WiFi: no conectado.");
        // NO llamar WiFi.mode(WIFI_OFF) — bug GDMA
        return;
    }

    ArduinoOTA.setPort(OTA_PORT);
    ArduinoOTA.setHostname(PORTAL_SSID);
    ArduinoOTA.setRebootOnSuccess(true);

    if (strlen(otaPass) > 0) {
        ArduinoOTA.setPassword(otaPass);
    }

    ArduinoOTA.onStart([this]() {
        Serial.end();
        _status("OTA: iniciando...");
    });

    ArduinoOTA.onEnd([this]() {
        _status("OTA: completado. Reiniciando...");
    });

    ArduinoOTA.onProgress([this](unsigned int prog, unsigned int total) {
        static uint8_t lastPct = 255;
        uint8_t pct = (prog * 100) / total;
        if (pct / 20 != lastPct / 20) {
            lastPct = pct;
            static char buf[32];
            snprintf(buf, sizeof(buf), "OTA: %u%%", pct);
            _status(buf);
        }
    });

    ArduinoOTA.onError([this](ota_error_t err) {
        static char buf[32];
        snprintf(buf, sizeof(buf), "OTA error: %u", err);
        _status(buf);
        _otaActive = false;
    });

    ArduinoOTA.begin();
    _otaActive = true;

    static char buf[48];
    snprintf(buf, sizeof(buf), "OTA listo  IP:%s", WiFi.localIP().toString().c_str());
    _status(buf);
}

// ─────────────────────────────────────────────────────────────
void OtaManager::disable() {
    if (_otaActive) {
        ArduinoOTA.end();
        _otaActive = false;
    }
    // NO llamar WiFi.mode(WIFI_OFF) — bug GDMA
    WiFi.disconnect(true);
    _status("WiFi apagado.");
}

// ─────────────────────────────────────────────────────────────
bool OtaManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

bool OtaManager::hasCredentials() const {
    Preferences prefs;
    prefs.begin(NVS_NS, true);
    String s = prefs.getString(NVS_SSID, "");
    prefs.end();
    return s.length() > 0;
}

// ─────────────────────────────────────────────────────────────
bool OtaManager::_loadCredentials(char* ssid, char* pass, char* otaPass) {
    Preferences prefs;
    prefs.begin(NVS_NS, true);
    String s = prefs.getString(NVS_SSID, "");
    String p = prefs.getString(NVS_PASS, "");
    String o = prefs.getString(NVS_OTA_PASS, "");
    prefs.end();

    if (s.length() == 0) return false;

    strncpy(ssid,    s.c_str(), 63);
    strncpy(pass,    p.c_str(), 63);
    strncpy(otaPass, o.c_str(), 32);
    return true;
}

void OtaManager::_saveCredentials(const char* ssid, const char* pass, const char* otaPass) {
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.putString(NVS_SSID,     ssid);
    prefs.putString(NVS_PASS,     pass);
    prefs.putString(NVS_OTA_PASS, otaPass);
    prefs.end();
}

void OtaManager::_status(const char* msg) {
    Serial.printf("[OTA] %s\n", msg);
    if (_cbStatus) _cbStatus(msg);
}