// src/RS485/RS485Handler.cpp
#include "RS485Handler.h"
#include "RS485.h"
#include "../display/Display.h"
#include "../hardware/Hardware.h"
#include "../hardware/Motor/Motor.h"
#include "../hardware/encoder/Encoder.h"
#include "../button/ButtonManager.h"
#include "../config.h"

// ─── Externs de estado global (definidos en main.cpp) ────────
extern String trackName;
extern bool   recStates, soloStates, muteStates, selectStates;
extern bool   vuClipState;
extern float  vuPeakLevels, faderPositions, vuLevels;
extern unsigned long vuLastUpdateTime, vuPeakLastUpdateTime;

namespace RS485Handler {

// =============================================================
//  onMasterData
// =============================================================
void onMasterData(const MasterPacket& pkt) {

    // ── Conexión ──────────────────────────────────────────────
    ConnectionState newState = pkt.connected ?
        ConnectionState::CONNECTED : ConnectionState::DISCONNECTED;
    if (newState != logicConnectionState) {
        logicConnectionState = newState;
        needsTOTALRedraw = true;
    }
    if (logicConnectionState != ConnectionState::CONNECTED) return;

    // ── Nombre de pista ───────────────────────────────────────
    char nameBuf[8] = {};
    memcpy(nameBuf, pkt.trackName, 7);
    nameBuf[7] = '\0';
    if (trackName != nameBuf) {
        trackName = String(nameBuf);
        needsHeaderRedraw = true;
    }

    // ── Flags de botones ──────────────────────────────────────
    bool nr = (pkt.flags & FLAG_REC)    != 0;
    bool ns = (pkt.flags & FLAG_SOLO)   != 0;
    bool nm = (pkt.flags & FLAG_MUTE)   != 0;
    bool nq = (pkt.flags & FLAG_SELECT) != 0;

    if (recStates    != nr) { recStates    = nr; handleButtonLedState(ButtonId::REC);    needsMainAreaRedraw = true; }
    if (soloStates   != ns) { soloStates   = ns; handleButtonLedState(ButtonId::SOLO);   needsMainAreaRedraw = true; }
    if (muteStates   != nm) { muteStates   = nm; handleButtonLedState(ButtonId::MUTE);   needsMainAreaRedraw = true; }
    if (selectStates != nq) { selectStates = nq; handleButtonLedState(ButtonId::SELECT); needsHeaderRedraw   = true; }

    // ── VU meter ──────────────────────────────────────────────
    float newVu = pkt.vuLevel / 127.0f;
    if (fabsf(vuLevels - newVu) > 0.01f) {
        vuLevels = newVu;
        if (vuLevels > vuPeakLevels) {
            vuPeakLevels = vuLevels;
            vuPeakLastUpdateTime = millis();
        }
        vuLastUpdateTime    = millis();
        needsVUMetersRedraw = true;
    }

    // ── Fader / Motor ─────────────────────────────────────────
    float newFader = pkt.faderTarget / 16383.0f;
    if (fabsf(faderPositions - newFader) > 0.001f) {
        faderPositions = newFader;
        Motor::setTarget(pkt.faderTarget);
    }

    if (pkt.flags & FLAG_CALIB) {
        Motor::startCalib();
    }

    // ── Modo de automatización (bits 5-7) ─────────────────────
    uint8_t newAutoMode = (pkt.flags >> 5) & 0x07;
    if (newAutoMode != currentAutoMode) {
        setAutoMode(newAutoMode);
        needsVPotRedraw   = true;
        needsHeaderRedraw = true;
    }

    // ── VPot ──────────────────────────────────────────────────
    setVPotRaw(pkt.vpotValue);
}

// =============================================================
//  buildResponse
// =============================================================
SlavePacket buildResponse(FaderADC& faderADC, SatMenu& satMenu) {
    SlavePacket resp = {};
    resp.faderPos      = Motor::getRawADC();
    resp.touchState    = isFaderTouched ? 1 : 0;
    resp.buttons       = ButtonManager::getButtonFlags();
    resp.encoderDelta  = (int8_t)constrain(Encoder::getCount(), -127, 127);
    resp.encoderButton = ButtonManager::getEncoderButton();
    return resp;
}

// =============================================================
//  checkTimeout
// =============================================================
void checkTimeout(unsigned long lastRxTime) {
    if (millis() - lastRxTime <= 500) return;

    if (vuLevels > 0.0f) {
        vuLevels = 0.0f;
        vuPeakLevels = 0.0f;
        needsVUMetersRedraw = true;
    }
    if (logicConnectionState != ConnectionState::DISCONNECTED) {
        logicConnectionState = ConnectionState::DISCONNECTED;
        needsTOTALRedraw = true;
    }
}

} // namespace RS485Handler