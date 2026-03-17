#pragma once
// ============================================================
//  OtaManager.h  —  iMakie PTxx Track S2
//
//  Gestiona WiFi, portal de configuración (WiFiManager) y OTA.
//
//  MODOS DE USO (desde SAT menu):
//   1. launchPortal()     → AP captive portal para guardar SSID/pass/OTA-pass
//   2. enableForUpload()  → conecta red guardada + ArduinoOTA activo
//
//  WiFi permanece APAGADO en operación normal.
//  NVS namespace: "ptxx"  (mismo que el resto del proyecto)
// ============================================================
#include <Arduino.h>
#include <functional>

using CbStatus = std::function<void(const char* msg)>;  // feedback al display

class OtaManager {
public:
    // ── Ciclo de vida ─────────────────────────────────────────
    void begin();                   // Llama en setup() — apaga WiFi
    void tick();                    // Llama en loop() — maneja ArduinoOTA

    // ── Acciones desde SAT ────────────────────────────────────
    void launchPortal();            // Abre AP + portal captive (bloqueante ~120 s)
    void enableForUpload();         // Conecta red + inicia ArduinoOTA (no bloqueante)
    void disable();                 // Desconecta y apaga WiFi

    // ── Callback de estado → display ─────────────────────────
    void onStatus(CbStatus cb) { _cbStatus = cb; }

    // ── Estado ────────────────────────────────────────────────
    bool isOtaActive()  const { return _otaActive; }
    bool isConnected()  const;

private:
    bool     _otaActive  = false;
    CbStatus _cbStatus;

    void _status(const char* msg);
    bool _loadCredentials(char* ssid, char* pass, char* otaPass);
    void _saveCredentials(const char* ssid, const char* pass, const char* otaPass);
};

extern OtaManager otaManager;