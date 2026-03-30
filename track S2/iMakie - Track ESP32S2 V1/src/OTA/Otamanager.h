#pragma once
// ============================================================
//  OtaManager.h  —  iMakie PTxx Track S2
// ============================================================
#include <Arduino.h>
#include <functional>

using CbStatus = std::function<void(const char* msg)>;

class OtaManager {
public:
    // ── Ciclo de vida ─────────────────────────────────────────
    void begin();
    void tick();

    // ── Acciones desde SAT ────────────────────────────────────
    void launchPortal();
    void enableForUpload();
    void disable();

    // ── Boot OTA ─────────────────────────────────────────────
    void beginOtaFromBoot();
    bool isOtaPending();
    void clearOtaPending();

    // ── Callback de estado → display ─────────────────────────
    void onStatus(CbStatus cb) { _cbStatus = cb; }

    // ── Estado ────────────────────────────────────────────────
    bool isOtaActive()  const { return _otaActive; }
    bool isConnected()  const;
    bool hasCredentials() const;

private:
    bool     _otaActive  = false;
    CbStatus _cbStatus;

    void _status(const char* msg);
    bool _loadCredentials(char* ssid, char* pass, char* otaPass);
    void _saveCredentials(const char* ssid, const char* pass, const char* otaPass);
    void _startOta(const char* ssid, const char* pass, const char* otaPass);
};

extern OtaManager otaManager;