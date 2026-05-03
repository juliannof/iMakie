// ============================================================
//  OtaManager.cpp  —  iMakie PTxx Track S2 (SINGLE-CORE)
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

    // ┌─ LIMPIEZA RADICAL Y PROFUNDA ─────────────────────────────────
    Serial.printf("[OTA] === LIMPIEZA RADICAL DEL SISTEMA ===\n");
    Serial.flush();

    // 1. APAGAR DISPLAY COMPLETAMENTE
    setScreenBrightness(0);
    Serial.flush();
    delay(50);
    pinMode(33, OUTPUT);  // Display RST
    digitalWrite(33, LOW);
    delay(100);
    Serial.printf("[OTA] Display apagado (RST LOW)\n");
    Serial.flush();

    // 2. BORRAR TODOS LOS SPRITES DE PSRAM
    extern LGFX tft;
    extern LGFX_Sprite header, mainArea, vuSprite, vPotSprite;
    header.deleteSprite();
    mainArea.deleteSprite();
    vuSprite.deleteSprite();
    vPotSprite.deleteSprite();
    Serial.printf("[OTA] Sprites borrados (PSRAM liberado)\n");
    Serial.flush();

    // 3. APAGAR PERIFÉRICOS
    clearAllNeopixels();
    showNeopixels();
    Motor::stop();
    Serial.printf("[OTA] NeoPixels + Motor apagados\n");
    Serial.flush();
    delay(50);

    // 4. DESHABILITAR TODAS LAS ISRs DE USUARIO
    detachInterrupt(digitalPinToInterrupt(12));  // Encoder A
    detachInterrupt(digitalPinToInterrupt(13));  // Encoder B
    detachInterrupt(digitalPinToInterrupt(21));  // Encoder BTN (si existe)
    for (uint8_t pin : {37, 38, 39, 40}) {
        detachInterrupt(digitalPinToInterrupt(pin));  // Buttons
    }
    Serial.printf("[OTA] Encoder + Button ISRs deshabilitadas\n");
    Serial.flush();

    // 5. DESABILITAR RS485 CON DRENAJE PROFUNDO
    Serial.printf("[OTA] Deshabilitando RS485...\n");
    Serial.flush();
    delay(100);
    Serial1.flush();
    delay(100);
    Serial1.onReceive(nullptr);  // ← CRÍTICO: Desregistrar ISR ANTES de end()
    delay(50);
    Serial1.end();
    delay(100);
    pinMode(RS485_TX_PIN, OUTPUT);
    digitalWrite(RS485_TX_PIN, LOW);
    pinMode(RS485_RX_PIN, INPUT);
    digitalWrite(RS485_ENABLE_PIN, HIGH);
    Serial.printf("[OTA] RS485 deshabilitado (drenaje profundo)\n");
    Serial.flush();

    // 6. YIELD MASIVO PARA QUE FreeRTOS LIMPIE
    Serial.printf("[OTA] Liberando FreeRTOS...\n");
    Serial.flush();
    for (int i = 0; i < 50; i++) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    // 7. VERIFICAR HEAP RECUPERADO
    uint32_t heapBefore = ESP.getFreeHeap();
    Serial.printf("[OTA] Heap post-limpieza: %lu bytes\n", heapBefore);
    Serial.flush();
    Serial.printf("[OTA] === LIMPIEZA COMPLETA ===\n");
    Serial.flush();
    // └─

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
        vTaskDelay(pdMS_TO_TICKS(250));
        if ((millis() - t0) % 1000 < 250) {
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

    // Esperar a que WiFi se estabilice completamente
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

    // Sistema ya está limpio — NO deshabilitar más
    // Configurar ArduinoOTA CON CALLBACKS ANTES de begin()
    ArduinoOTA.setPort(OTA_PORT);
    ArduinoOTA.setHostname(PORTAL_SSID);
    ArduinoOTA.setRebootOnSuccess(false);  // CRÍTICO: reboot manual después de ACK final
    Serial.printf("[OTA] ArduinoOTA config | port=%d | host=%s\n", OTA_PORT, PORTAL_SSID);
    Serial.flush();

    if (strlen(otaPass) > 0) {
        ArduinoOTA.setPassword(otaPass);
        Serial.printf("[OTA] OTA password establecida\n");
        Serial.flush();
    }
    ArduinoOTA.setTimeout(30000);  // Timeout 30s para WiFi lento
    ArduinoOTA.onStart([this]() {
        Serial.printf("[OTA] === UPLOAD INICIANDO ===\n");
        Serial.flush();
        // CRÍTICO: Deshabilitar logging de Arduino/IDF durante upload
        // CORE_DEBUG_LEVEL=3 imprime MILES de logs que bloquean Serial
        Serial.setDebugOutput(false);
    });

    ArduinoOTA.onEnd([this]() {
        Serial.printf("[OTA] ← onEnd() INVOCADO\n");
        Serial.flush();
        Serial.setDebugOutput(true);
        Serial.printf("[OTA] === UPLOAD COMPLETADO ===\n");
        Serial.flush();
        
        // Múltiples confirmaciones + delays para asegurar que logs se envíen
        for (int i = 0; i < 5; i++) {
            Serial.printf("[OTA] ACK %d/5 - esperando...\n", i+1);
            Serial.flush();
            delay(200);
        }
        
        Serial.printf("[OTA] === REBOOT INICIADO ===\n");
        Serial.flush();
        delay(500);  // 500ms final de buffer flush
        esp_restart();
    });

    // NO usar onProgress() — cada callback suma latencia
    // ArduinoOTA.onProgress(...) deliberadamente NO configurado

    ArduinoOTA.onError([this](ota_error_t err) {
        Serial.printf("[OTA] ← onError() INVOCADO - code=%u\n", err);
        Serial.flush();
        Serial.setDebugOutput(true);
        Serial.printf("[OTA] ERROR: %u\n", err);
        Serial.flush();
        _otaActive = false;
    });

    // ArduinoOTA.begin()
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

    vTaskDelay(pdMS_TO_TICKS(100));

    _otaActive = true;

    static char buf[64];
    snprintf(buf, sizeof(buf), "OTA LISTO | IP:%s | Puerto:%d",
             WiFi.localIP().toString().c_str(), OTA_PORT);
    Serial.printf("[OTA] %s\n", buf);
    Serial.flush();

    Serial.printf("[OTA] === LOOP DEDICADO A OTA INICIADO ===\n");
    Serial.flush();

    // Loop bloqueante dedicado SOLO a OTA — no regresa hasta reiniciar
    while (_otaActive) {
        ArduinoOTA.handle();
        delay(1);  // Permite que WiFi se ejecute en background
    }
}

// ─────────────────────────────────────────────────────────────
void OtaManager::disable() {
    Serial.printf("[OTA] disable() — desconectando WiFi\n");
    Serial.flush();
    if (_otaActive) {
        ArduinoOTA.end();
        _otaActive = false;
    }
    WiFi.disconnect(true);
    Serial.printf("[OTA] WiFi desconectado\n");
    Serial.flush();
    if (_cbStatus) _cbStatus("WiFi apagado.");
}

// ─────────────────────────────────────────────────────────────
void OtaManager::_showOtaScreen() {
    extern void setScreenBrightness(uint8_t brightness);
    extern LGFX tft;

    uint8_t lastOctet = WiFi.localIP()[3];

    tft.setTextFont(0);
    tft.setTextColor(TFT_WHITE);

    char buf[4];
    snprintf(buf, sizeof(buf), ".%u", lastOctet);
    tft.drawString(buf, tft.width() / 2, tft.height() / 2);

    setScreenBrightness(5);
}

// ─────────────────────────────────────────────────────────────
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
