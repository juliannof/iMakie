// ============================================================
//  OtaManager.cpp  —  iMakie PTxx Track S2
// ============================================================
#include "OtaManager.h"
#include "../config.h"
#include "../hardware/Motor/Motor.h"
#include "../RS485/RS485.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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
    log_i("[OTA] launchPortal() — inicio");
    log_i("[OTA] Heap: %d  PSRAM: %d", ESP.getFreeHeap(), ESP.getFreePsram());   
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
    WiFiManagerParameter paramTrackId("trackid", "Track ID (1-9)", defaultId, 3);
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
    extern void setScreenBrightness(uint8_t brightness);
    extern void clearAllNeopixels();
    extern void showNeopixels();

    Serial.flush();
    Serial.printf("[OTA] enableForUpload() — inicio\n");
    Serial.flush();

    // Liberar máximo heap posible ANTES de WiFi
    setScreenBrightness(0);
    Serial.flush();
    delay(100);

    clearAllNeopixels();
    showNeopixels();
    Serial.printf("[OTA] NeoPixels apagados\n");
    Serial.flush();

    Motor::stop();
    Serial.printf("[OTA] Motor apagado\n");
    Serial.flush();
    delay(50);

    char ssid[64]    = {};
    char pass[64]    = {};
    char otaPass[33] = {};

    Serial.printf("[OTA] Heap libre: %d bytes | PSRAM: %d bytes\n",
                  ESP.getFreeHeap(), ESP.getFreePsram());
    Serial.flush();

    if (!_loadCredentials(ssid, pass, otaPass)) {
        Serial.printf("[OTA] ERROR: Sin credenciales en NVS\n");
        Serial.flush();
        _status("Sin credenciales. Config WiFi primero.");
        return;
    }

    Serial.printf("[OTA] Credenciales OK | SSID=%s | OTA pass=%s\n",
                  ssid, strlen(otaPass) > 0 ? "***" : "(vacío)");
    Serial.flush();

    _status("Conectando WiFi...");

    // Deshabilitar RS485 con drenaje seguro
    Serial.printf("[OTA] Deshabilitando RS485...\n");
    Serial.flush();
    delay(100);
    Serial1.flush();
    delay(50);
    Serial1.end();
    delay(50);
    pinMode(RS485_TX_PIN, OUTPUT);
    digitalWrite(RS485_TX_PIN, LOW);
    digitalWrite(RS485_ENABLE_PIN, HIGH);
    Serial.printf("[OTA] RS485 deshabilitado\n");
    Serial.flush();

    Serial.printf("[OTA] HEAP: free=%d | minFree=%d | maxBlock=%d\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    Serial.flush();

    Serial.printf("[OTA] Llamando WiFi.begin(%s)...\n", ssid);
    Serial.flush();
    WiFi.begin(ssid, pass);
    Serial.printf("[OTA] WiFi.begin() retornó | status=%d\n", WiFi.status());
    Serial.flush();

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
        vTaskDelay(pdMS_TO_TICKS(250));  // Ceder CPU a WiFi
        if ((millis() - t0) % 1000 < 250) {  // Log cada ~1s
            Serial.printf("[OTA] WiFi status=%d | elapsed=%lums | heap=%d\n",
                          WiFi.status(), millis() - t0, ESP.getFreeHeap());
            Serial.flush();
        }
    }

    Serial.printf("[OTA] Bucle WiFi terminado | status=%d | elapsed=%lums\n",
                  WiFi.status(), millis() - t0);
    Serial.flush();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("[OTA] ERROR: WiFi no conectado después de 10s\n");
        Serial.flush();
        _status("WiFi: no conectado.");
        delay(50);
        digitalWrite(RS485_ENABLE_PIN, LOW);
        delay(50);
        Serial1.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
        Serial.printf("[OTA] RS485 restaurado\n");
        Serial.flush();
        return;
    }

    Serial.printf("[OTA] WiFi CONECTADO | IP=%s | RSSI=%d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    Serial.flush();

    // Esperar a que WiFi se estabilice completamente — crucial para upload estable
    Serial.printf("[OTA] Estabilizando WiFi (1s)...\n");
    Serial.flush();
    for (int i = 0; i < 20; i++) {
        vTaskDelay(pdMS_TO_TICKS(50));
        if (i % 4 == 0) {
            int rssi = WiFi.RSSI();
            Serial.printf("[OTA]   RSSI: %d dBm | Heap: %d bytes\n", rssi, ESP.getFreeHeap());
            Serial.flush();
        }
    }
    Serial.printf("[OTA] WiFi estable | Iniciando ArduinoOTA server\n");
    Serial.flush();

    // ┌─ CRÍTICO: Deshabilitar TODAS las ISRs ANTES de ArduinoOTA
    _disableAllInterrupts();
    // └─

    // Configurar ArduinoOTA CON CALLBACKS ANTES de begin()
    ArduinoOTA.setPort(OTA_PORT);
    ArduinoOTA.setHostname(PORTAL_SSID);
    ArduinoOTA.setRebootOnSuccess(true);
    Serial.printf("[OTA] ArduinoOTA config | port=%d | host=%s\n", OTA_PORT, PORTAL_SSID);
    Serial.flush();

    if (strlen(otaPass) > 0) {
        ArduinoOTA.setPassword(otaPass);
        Serial.printf("[OTA] OTA password establecida\n");
        Serial.flush();
    }

    ArduinoOTA.onStart([this]() {
        Serial.end();
        _status("OTA: iniciando...");
    });
    ArduinoOTA.onEnd([this]() {
        Serial.printf("[OTA] Upload completado, reiniciando en 2s...\n");
        Serial.flush();
        _status("OTA: completado. Reiniciando...");
        delay(2000);  // Dar tiempo a ver el mensaje
    });
    ArduinoOTA.onProgress([this](unsigned int prog, unsigned int total) {
        static uint8_t lastPct = 255;
        static uint32_t lastMs = 0;
        uint8_t pct = (prog * 100) / total;
        uint32_t nowMs = millis();

        if (pct / 10 != lastPct / 10 || (nowMs - lastMs > 5000)) {
            lastPct = pct;
            lastMs = nowMs;

            uint32_t speed = (prog / 1024) / ((nowMs - 100) / 1000 + 1);  // KB/s aproximado
            static char buf[48];
            snprintf(buf, sizeof(buf), "OTA: %u%% | %u KB/s", pct, speed);
            Serial.printf("[OTA-PROGRESS] %s\n", buf);
            Serial.flush();
            _status(buf);
        }
    });
    ArduinoOTA.onError([this](ota_error_t err) {
        Serial.printf("[OTA] ArduinoOTA ERROR: %u\n", err);
        Serial.flush();
        const char* errMsg = "Unknown";
        switch(err) {
            case OTA_AUTH_ERROR: errMsg = "Auth failed"; break;
            case OTA_BEGIN_ERROR: errMsg = "Begin failed"; break;
            case OTA_CONNECT_ERROR: errMsg = "Connect error"; break;
            case OTA_RECEIVE_ERROR: errMsg = "Receive error"; break;
            case OTA_END_ERROR: errMsg = "End error"; break;
            default: break;
        }
        Serial.printf("[OTA] Error type: %s\n", errMsg);
        Serial.flush();

        // Restaurar ISRs tras error
        _restoreAllInterrupts();

        static char buf[48];
        snprintf(buf, sizeof(buf), "OTA error: %s (%u)", errMsg, err);
        _status(buf);
        _otaActive = false;
    });

    // ArduinoOTA.begin() con TIMEOUT
    Serial.printf("[OTA] Llamando ArduinoOTA.begin()...\n");
    Serial.flush();
    uint32_t t1 = millis();
    ArduinoOTA.begin();
    uint32_t beginMs = millis() - t1;
    Serial.printf("[OTA] ArduinoOTA.begin() completado en %lu ms\n", beginMs);
    Serial.flush();

    if (beginMs > 5000) {
        Serial.printf("[OTA] ADVERTENCIA: ArduinoOTA.begin() tardó %lu ms (>5s)\n", beginMs);
        Serial.flush();
    }

    vTaskDelay(pdMS_TO_TICKS(100));  // Dar tiempo a que socket UDP se estabilice

    _showOtaScreen();
    Serial.printf("[OTA] Pantalla OTA mostrada\n");
    Serial.flush();

    _otaActive = true;

    static char buf[64];
    snprintf(buf, sizeof(buf), "OTA LISTO | IP:%s | Puerto:%d",
             WiFi.localIP().toString().c_str(), OTA_PORT);
    Serial.printf("[OTA] %s\n", buf);
    Serial.flush();
    _status(buf);

    Serial.printf("[OTA] === ESPERANDO conexión de cliente OTA ===\n");
    Serial.flush();
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
void OtaManager::_showOtaScreen() {
    extern void setScreenBrightness(uint8_t brightness);
    extern LGFX tft;

    uint8_t lastOctet = WiFi.localIP()[3];

    tft.setTextFont(0);  // Fuente de sistema (mínima, no bloquea)
    tft.setTextColor(TFT_WHITE);

    char buf[4];
    snprintf(buf, sizeof(buf), ".%u", lastOctet);
    tft.drawString(buf, tft.width() / 2, tft.height() / 2);

    setScreenBrightness(5);  // Brillo mínimo (~2%)
}

// ─────────────────────────────────────────────────────────────
void OtaManager::enableSerialMode() {
    _status("Conecta USB y ejecuta:");
    delay(400);
    _status("pio run -e serial --target upload");
    delay(1200);
    esp_restart();
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

// ─────────────────────────────────────────────────────────────
// Deshabilitar TODAS las interrupciones durante OTA
// ─────────────────────────────────────────────────────────────
void OtaManager::_disableAllInterrupts() {
    Serial.printf("[OTA] Deshabilitando todas las ISRs...\n");
    Serial.flush();

    // Serial1 ISR (RS485)
    Serial1.onReceive(nullptr);
    Serial.printf("[OTA]   - RS485 Serial1 ISR deshabilitada\n");
    Serial.flush();

    // Encoder ISRs (GPIO12 y GPIO13)
    detachInterrupt(digitalPinToInterrupt(12));  // ENCODER_PIN_A
    detachInterrupt(digitalPinToInterrupt(13));  // ENCODER_PIN_B
    Serial.printf("[OTA]   - Encoder ISRs deshabilitadas\n");
    Serial.flush();

    // ButtonManager ISRs (GPIO37, 38, 39, 40)
    // Asumiendo que ButtonManager usa attachInterrupt internamente
    for (uint8_t pin : {37, 38, 39, 40}) {
        detachInterrupt(digitalPinToInterrupt(pin));
    }
    Serial.printf("[OTA]   - Button ISRs deshabilitadas\n");
    Serial.flush();

    Serial.printf("[OTA] Todas las ISRs deshabilitadas\n");
    Serial.flush();
}

// ─────────────────────────────────────────────────────────────
// Restaurar interrupciones críticas después de OTA (si falla)
// ─────────────────────────────────────────────────────────────
void OtaManager::_restoreAllInterrupts() {
    Serial.printf("[OTA] Restaurando RS485 ISR...\n");
    Serial.flush();

    // Solo restauramos RS485 porque es crítico
    // Encoder y Buttons se requieren reinicio manual vía SAT si OTA falla
    Serial1.onReceive([](){ rs485._onReceiveISR(); });
    Serial.printf("[OTA]   - RS485 restaurada\n");
    Serial.flush();
    Serial.printf("[OTA] NOTA: Si OTA falló, presiona Encoder para abrir SAT y reinicia\n");
    Serial.flush();
}