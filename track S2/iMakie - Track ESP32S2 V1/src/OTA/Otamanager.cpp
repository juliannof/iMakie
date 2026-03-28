// ============================================================
//  OtaManager.cpp  —  iMakie PTxx Track S2
// ============================================================
#include "OtaManager.h"

#include <WiFi.h>
#include <WiFiManager.h>   // tzapu/WiFiManager
#include <ArduinoOTA.h>
#include <Preferences.h>

// ── Constantes ───────────────────────────────────────────────
#define NVS_NS          "ptxx"
#define NVS_SSID        "wifiSsid"
#define NVS_PASS        "wifiPass"
#define NVS_OTA_PASS    "otaPass"

#define PORTAL_SSID     "iMakie-PTxx"   // Nombre del AP captive portal
#define PORTAL_TIMEOUT  120             // segundos antes de abortar portal
#define OTA_PORT        3232

// Instancia global
OtaManager otaManager;

// ─────────────────────────────────────────────────────────────
void OtaManager::begin() {
    WiFi.mode(WIFI_OFF);
    WiFi.disconnect(true);
    _otaActive = false;
}

// ─────────────────────────────────────────────────────────────
void OtaManager::tick() {
    if (_otaActive) {
        log_i("OTA tick");   // ← temporal
        ArduinoOTA.handle();
    }
}

// ─────────────────────────────────────────────────────────────
//  launchPortal()
//  Abre un AP "iMakie-PTxx" con portal captive.
//  El portal incluye un campo extra para la contraseña OTA.
//  Bloqueante hasta que el usuario guarda o se agota el timeout.
// ─────────────────────────────────────────────────────────────
void OtaManager::launchPortal() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(200);
    WiFi.mode(WIFI_STA);
    delay(200);

    _status("AP: iMakie-PTxx");
    delay(50);
    _status("Conecta y abre 192.168.4.1");
    delay(50);

    // ── AÑADIR: leer trackId actual para mostrarlo en el formulario ──
    Preferences prefs;
    prefs.begin(NVS_NS, true);
    uint8_t currentId = prefs.getUChar("trackId", 0);
    prefs.end();
    char defaultId[4];
    snprintf(defaultId, sizeof(defaultId), "%u", currentId);
    // ────────────────────────────────────────────────────────────────

    WiFiManager wm;
    wm.setConfigPortalTimeout(PORTAL_TIMEOUT);
    wm.setConnectTimeout(15);
    wm.setBreakAfterConfig(true);

    WiFiManagerParameter paramOtaPass("otapass", "OTA Password", "", 32);
    WiFiManagerParameter paramTrackId("trackid", "Track ID (1-17)", defaultId, 3); // ← AÑADIR
    wm.addParameter(&paramOtaPass);
    wm.addParameter(&paramTrackId); // ← AÑADIR

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

    // ── AÑADIR: guardar trackId si es válido ─────────────────────────
    uint8_t newId = (uint8_t)atoi(paramTrackId.getValue());
    if (newId >= 1 && newId <= 9) {
        Preferences prefs;
        prefs.begin(NVS_NS, false);
        prefs.putUChar("trackId", newId);
        prefs.end();
        static char buf[32];
        snprintf(buf, sizeof(buf), "Track ID: %u guardado.", newId);
        _status(buf);
    }
    // ────────────────────────────────────────────────────────────────

    WiFi.mode(WIFI_OFF);
    WiFi.disconnect(true);
    _otaActive = false;
}

// ─────────────────────────────────────────────────────────────
//  enableForUpload()
//  Conecta la red guardada e inicia ArduinoOTA.
//  No bloqueante: el loop() maneja OTA via tick().
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

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    // Espera hasta 10 s
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
        delay(250);
    }

    if (WiFi.status() != WL_CONNECTED) {
        _status("WiFi: no conectado.");
        WiFi.mode(WIFI_OFF);
        return;
    }

    // ── ArduinoOTA ───────────────────────────────────────────
    ArduinoOTA.setPort(OTA_PORT);
    ArduinoOTA.setHostname(PORTAL_SSID);
    ArduinoOTA.setRebootOnSuccess(true);   // ← añadir

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
    WiFi.mode(WIFI_OFF);
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
//  NVS helpers
// ─────────────────────────────────────────────────────────────
bool OtaManager::_loadCredentials(char* ssid, char* pass, char* otaPass) {
    Preferences prefs;
    prefs.begin(NVS_NS, true);  // read-only
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
    prefs.begin(NVS_NS, false);  // read-write
    prefs.putString(NVS_SSID,     ssid);
    prefs.putString(NVS_PASS,     pass);
    prefs.putString(NVS_OTA_PASS, otaPass);
    prefs.end();
}

void OtaManager::_status(const char* msg) {
    Serial.printf("[OTA] %s\n", msg);   // S2: Serial.printf, no log_i
    if (_cbStatus) _cbStatus(msg);
}