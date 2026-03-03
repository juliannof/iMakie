// src/main.cpp  –  iMakie Slave ESP32-S2
// RS485 integrado — cambios marcados con // ← RS485

#include <Arduino.h>
#include "config.h"
#include "display/Display.h"
#include "hardware/encoder/Encoder.h"
#include "hardware/Hardware.h"
#include "RS485/RS485.h"        // ← RS485
#include "protocol.h"           // ← RS485

// ---- Display objects ----
#include "display/LovyanGFX_config.h"
LGFX        tft;
LGFX_Sprite header(&tft), mainArea(&tft), vuSprite(&tft), vPotSprite(&tft);

// --- DECLARACIONES EXTERNAS ---
extern void initDisplay();
extern void updateDisplay();
extern void setScreenBrightness(uint8_t brightness);
extern void setVPotLevel(int8_t level);
extern void handleVUMeterDecay();   // decay VU en loop
extern int  currentVPotLevel;
extern bool needsVPotRedraw;

// --- VARIABLES DE ESTADO DE CANAL ---
String assignmentString = "CH-01 ";
String trackName        = "Track  ";
bool recStates    = false;
bool soloStates   = false;
bool muteStates   = false;
bool selectStates = true;
bool vuClipState  = false;
float vuPeakLevels   = 0.0f;
float faderPositions = 0.0f;
float vuLevels       = 0.0f;
unsigned long vuLastUpdateTime     = 0;
unsigned long vuPeakLastUpdateTime = 0;

// ← RS485: estado de respuesta al master (actualizado desde hardware y callbacks)
static SlavePacket _resp = {};

// ← RS485: leer ADC del fader (stub — conectar a tu ADC real)
static uint16_t readFaderADC() {
    // TODO: sustituir por lectura ADC real
    // return analogRead(FADER_POT);
    return 2048;
}

// ← RS485: leer estado táctil del fader (stub)
static uint8_t readFaderTouch() {
    // TODO: leer pin táctil real
    // return (touchRead(FADER_TOUCH_PIN) < threshold) ? 1 : 0;
    return 0;
}


// ===================================
// --- CALLBACK DE BOTONES ---
// ===================================
void myButtonEventHandler(ButtonId id) {
    Serial.print("Evento de boton: ");   // ← RS485: Serial en S2

    switch (id) {
        case ButtonId::REC:
            recStates = !recStates;
            Serial.printf("REC -> %s\n", recStates ? "ON" : "OFF");
            needsMainAreaRedraw = true;
            break;
        case ButtonId::SOLO:
            soloStates = !soloStates;
            Serial.printf("SOLO -> %s\n", soloStates ? "ON" : "OFF");
            needsMainAreaRedraw = true;
            break;
        case ButtonId::MUTE:
            muteStates = !muteStates;
            Serial.printf("MUTE -> %s\n", muteStates ? "ON" : "OFF");
            needsMainAreaRedraw = true;
            break;
        case ButtonId::SELECT:
            selectStates = !selectStates;
            Serial.printf("SELECT -> %s\n", selectStates ? "ON" : "OFF");
            needsHeaderRedraw = true;
            break;
        case ButtonId::ENCODER_SELECT:
            Serial.println("ENCODER SELECT");
            Encoder::reset();
            needsVPotRedraw = true;
            break;
        case ButtonId::UNKNOWN:
            Serial.println("Boton desconocido");
            break;
    }

    // ← RS485: reconstruir byte de flags tras cualquier cambio de botón
    _resp.buttons = 0;
    if (recStates)    _resp.buttons |= FLAG_REC;
    if (soloStates)   _resp.buttons |= FLAG_SOLO;
    if (muteStates)   _resp.buttons |= FLAG_MUTE;
    if (selectStates) _resp.buttons |= FLAG_SELECT;
}


// Para llamar handleButtonLedState desde main.cpp
extern void handleButtonLedState(ButtonId id);

// ===================================
// --- HANDLER DE DATOS RS485 ---
// ===================================
static void onMasterData(const MasterPacket& pkt) {
    // --- Estado de conexión comandado por master ---
    ConnectionState newState = pkt.connected ? 
        ConnectionState::CONNECTED : ConnectionState::DISCONNECTED;
    if (newState != logicConnectionState) {
        logicConnectionState = newState;
        needsTOTALRedraw = true;
    }
    if (logicConnectionState != ConnectionState::CONNECTED) return;

    // --- Nombre de pista → header ---
    char nameBuf[8] = {};
    memcpy(nameBuf, pkt.trackName, 7);
    nameBuf[7] = '\0';
    if (trackName != nameBuf) {
        trackName = String(nameBuf);
        needsHeaderRedraw = true;
    }

    // --- Flags → display + neopixel por botón ---
    bool newRec    = (pkt.flags & FLAG_REC)    != 0;
    bool newSolo   = (pkt.flags & FLAG_SOLO)   != 0;
    bool newMute   = (pkt.flags & FLAG_MUTE)   != 0;
    bool newSelect = (pkt.flags & FLAG_SELECT) != 0;

    if (recStates != newRec) {
        recStates = newRec;
        handleButtonLedState(ButtonId::REC);
        needsMainAreaRedraw = true;
    }
    if (soloStates != newSolo) {
        soloStates = newSolo;
        handleButtonLedState(ButtonId::SOLO);
        needsMainAreaRedraw = true;
    }
    if (muteStates != newMute) {
        muteStates = newMute;
        handleButtonLedState(ButtonId::MUTE);
        needsMainAreaRedraw = true;
    }
    if (selectStates != newSelect) {
        selectStates = newSelect;
        handleButtonLedState(ButtonId::SELECT);
        needsHeaderRedraw = true;
    }

    // --- VU level (0-127 → 0.0-1.0) ---
    float newVu = pkt.vuLevel / 127.0f;
    if (abs(vuLevels - newVu) > 0.01f) {
        vuLevels = newVu;
        if (vuLevels > vuPeakLevels) {
            vuPeakLevels = vuLevels;
            vuPeakLastUpdateTime = millis();
        }
        vuLastUpdateTime = millis();
        needsVUMetersRedraw = true;
    }

    // --- Posición de fader objetivo (14-bit → 0.0-1.0) ---
    float newFader = pkt.faderTarget / 16383.0f;
    if (abs(faderPositions - newFader) > 0.001f) {
        faderPositions = newFader;
        // TODO: activar motor
    }

    Serial.printf("[RS485] RX | track:%.7s fader:%u vu:%u flags:0x%02X connected:%u\n",
                  pkt.trackName, pkt.faderTarget, pkt.vuLevel, pkt.flags, pkt.connected);
}


// ===================================
// --- SETUP ---
// ===================================
void setup() {
    Serial.begin(115200);    // ← RS485: Serial en lugar de Serial
    delay(1000);

    initDisplay();
    Serial.println("[SLAVE] Display OK");

    initHardware();
    Serial.println("[SLAVE] Hardware OK");

    setVPotLevel(VPOT_DEFAULT_LEVEL);
    registerButtonEventCallback(myButtonEventHandler);
    Encoder::begin();

    // ← RS485: iniciar bus
    rs485.begin(MY_SLAVE_ID);
    Serial.printf("[SLAVE] RS485 OK | ID:%d\n", MY_SLAVE_ID);
}


// ===================================
// --- LOOP ---
// ===================================
void loop() {
    // ← RS485: procesar buffer de recepción
    rs485.update();

    // ← RS485: si hay datos nuevos del master
    static unsigned long lastRxTime = 0;

    if (rs485.hasNewData()) {
        lastRxTime = millis();

        // Reconectar si estaba desconectado
        if (logicConnectionState != ConnectionState::CONNECTED) {
            logicConnectionState = ConnectionState::CONNECTED;
            needsTOTALRedraw = true;
        }

        const MasterPacket& pkt = rs485.getData();
        onMasterData(pkt);

        // Preparar respuesta con estado actual del hardware
        _resp.faderPos      = readFaderADC();
        _resp.touchState    = readFaderTouch();
        // _resp.buttons ya se actualiza en myButtonEventHandler
        _resp.encoderDelta  = (int8_t)constrain(Encoder::getCount(), -127, 127);
        _resp.encoderButton = 0;  // TODO: leer botón encoder si procede

        rs485.sendResponse(_resp);

        // Reset encoder delta tras enviarlo
        Encoder::reset();
        //Serial.printf("[RS485] RX | track:%.7s fader:%u vu:%u flags:0x%02X connected:%u state:%d\n",
        //      pkt.trackName, pkt.faderTarget, pkt.vuLevel, pkt.flags, 
        //      pkt.connected, (int)logicConnectionState);
    }

    // Timeout RS485: sin datos en 500ms → desconectar
    if (millis() - lastRxTime > 500) {
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

    // --- Encoder ---
    Encoder::update();
    if (Encoder::hasChanged()) {
        long value = Encoder::getCount();
        int newVPotLevel = constrain(value, -7, 7);
        if (newVPotLevel != Encoder::currentVPotLevel) {
            Encoder::currentVPotLevel = newVPotLevel;
            needsVPotRedraw = true;
        }
    }

    // --- Botones ---
    updateButtons();

    // --- VU decay ---
    //handleVUMeterDecay();

    // --- Display ---
    updateDisplay();

    delay(1);
}