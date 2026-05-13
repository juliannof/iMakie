// src/RS485/RS485Handler.cpp
#include "RS485Handler.h"
#include "RS485.h"
#include "../display/Display.h"
#include "../hardware/Hardware.h"
#include "../hardware/Neopixels/Neopixel.h"   // ← neoWaitingHandshake, updateAllNeopixels, showNeopixels
#include "../hardware/Motor/Motor.h"
#include "../hardware/encoder/Encoder.h"
#include "../hardware/fader/FaderTouch.h"
#include "../hardware/button/ButtonManager.h"
#include "../config.h"

// ─── Externs de estado global (definidos en main.cpp) ────────
extern String trackName;
extern bool   recStates, soloStates, muteStates, selectStates;
extern bool   vuClipState;
extern float  vuPeakLevels, faderPositions, vuLevels;
extern unsigned long vuLastUpdateTime, vuPeakLastUpdateTime;

// ─── handleButtonLedState definida en Hardware.cpp ───────────
extern void handleButtonLedState(ButtonId id);

namespace RS485Handler {

// =============================================================
//  onMasterData
// =============================================================
void onMasterData(const MasterPacket& pkt) {
    // Log no bloqueante cada 1s — diagnóstico RS485 recepción
    static unsigned long lastLog = 0;
    if (millis() - lastLog > 1000) {
        log_i("[RS485 RX] Master packet: id=%d target=%d connected=%d", pkt.id, pkt.faderTarget, pkt.connected);
        lastLog = millis();
    }

    // ── Conexión ──────────────────────────────────────────────
    ConnectionState newState = pkt.connected ?
        ConnectionState::CONNECTED : ConnectionState::DISCONNECTED;
    if (newState != logicConnectionState) {
    logicConnectionState = newState;
    needsTOTALRedraw = true;

    if (newState == ConnectionState::CONNECTED) {
        neoWaitingHandshake = false;
        // Cambio de azul a colores tenues
        // ¡CRÍTICO! NO llamar updateAllNeopixels() aquí — retarda RS485 response
        // Neopixels se actualizan en main.cpp DESPUÉS de sendResponse()
    } else {
        // ── Desconexión limpia ────────────────────────────
        Motor::off();
        Motor::setTarget(Motor::getRawADC());
        recStates = soloStates = muteStates = selectStates = false;
        vuLevels = vuPeakLevels = 0.0f;
        //setScreenBrightness(0);
        neoWaitingHandshake = true;
        // Cambio a azul
        // ¡CRÍTICO! NO llamar updateAllNeopixels() aquí — retarda RS485 response
        // Neopixels se actualizan en main.cpp DESPUÉS de sendResponse()
    }
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
    if (selectStates != nq) {
        selectStates = nq;
        handleButtonLedState(ButtonId::SELECT);
        handleButtonLedState(ButtonId::REC);
        handleButtonLedState(ButtonId::SOLO);
        handleButtonLedState(ButtonId::MUTE);
        //showNeopixels();
        needsHeaderRedraw = true;
           }

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
    static uint8_t _calib_send_state = 0;  // 0=normal, 1=enviando min, 2=enviando max

    SlavePacket resp = {};
    resp.touchState    = FaderTouch::isTouched() ? 1 : 0;
    resp.buttons       = ButtonManager::getButtonFlags();
    resp.encoderDelta  = (int8_t)constrain(Encoder::getCount(), -127, 127);
    resp.encoderButton = ButtonManager::getEncoderButton();

    Motor::CalibState cs = Motor::getCalibState();

    // Máquina de estado: enviar min/max tras calibración
    if (cs == Motor::CalibState::DONE && _calib_send_state < 2) {
        if (_calib_send_state == 0) {
            // Paquete 1: enviar MIN
            resp.faderPos = Motor::getADCMin();
            resp.buttons |= SLAVE_FLAG_CALIB_DONE | SLAVE_FLAG_CALIB_SENDING | SLAVE_FLAG_CALIB_IS_MIN;
            _calib_send_state = 1;
        } else if (_calib_send_state == 1) {
            // Paquete 2: enviar MAX
            resp.faderPos = Motor::getADCMax();
            resp.buttons |= SLAVE_FLAG_CALIB_DONE | SLAVE_FLAG_CALIB_SENDING;  // sin IS_MIN = es MAX
            _calib_send_state = 2;
        }
    } else {
        // Normal: enviar posición actual
        resp.faderPos = Motor::getRawADC();
        if (cs == Motor::CalibState::DONE)  resp.buttons |= SLAVE_FLAG_CALIB_DONE;
    }

    if (cs == Motor::CalibState::ERROR) resp.buttons |= SLAVE_FLAG_CALIB_ERROR;
    // NOT_CALIBRATED no se necesita — CALIB_DONE lo indica suficientemente

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
        Motor::off();
        Motor::setTarget(Motor::getRawADC());
        recStates = soloStates = muteStates = selectStates = false;
        setScreenBrightness(0);
        neoWaitingHandshake = true;
        // Volver a azul en timeout
        // ¡CRÍTICO! NO actualizar neopixels aquí — timeout podría estar en path RS485
        // Los neopixels se actualizarán en el siguiente ciclo main.cpp
        needsTOTALRedraw = true;
    }
}

} // namespace RS485Handler