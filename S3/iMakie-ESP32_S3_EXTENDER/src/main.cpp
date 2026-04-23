#include <Arduino.h>
#include <USB.h>
#include <USBMIDI.h>
#include "tusb.h"
#include "config.h"
#include "midi/MIDIProcessor.h"
#include "RS485/RS485.h"
#include "hardware/Transporte.h"

// ====================================================================
// --- MIDI ---
// ====================================================================
USBMIDI MIDI;

// ====================================================================
// --- ESTADO GLOBAL ---
// ====================================================================
volatile ConnectionState logicConnectionState = ConnectionState::DISCONNECTED;
uint8_t g_logicConnected = 0;

// --- Redraw flags (stubs — sin pantalla) ---
bool needsTOTALRedraw    = false;
bool needsMainAreaRedraw = false;
bool needsHeaderRedraw   = false;
bool needsVUMetersRedraw = false;
bool needsButtonsRedraw  = false;
bool needsTimecodeRedraw = false;

// --- UI flags (stubs) ---
volatile bool g_switchToOffline = false;
volatile bool g_switchToPage3   = false;

// --- Timecode (stubs) ---
DisplayMode currentTimecodeMode = MODE_BEATS;
char timeCodeChars_clean[13]    = {};
char beatsChars_clean[13]       = {};

// --- Estados de canales ---
bool recStates[9]    = {false};
bool soloStates[9]   = {false};
bool muteStates[9]   = {false};
bool selectStates[9] = {false};

float vuLevels[9]                    = {0};
float vuPeakLevels[9]                = {0};
bool  vuClipState[9]                 = {false};
unsigned long vuLastUpdateTime[9]    = {0};
unsigned long vuPeakLastUpdateTime[9]= {0};
float faderPositions[9]              = {0};

// --- Botones (stubs — sin NeoTrellis) ---
bool btnStatePG1[32]  = {false};
bool btnStatePG2[32]  = {false};
bool btnFlashPG1[32]  = {false};
bool btnFlashPG2[32]  = {false};

// --- Track info ---
String trackNames[8];
String assignmentString = "--";
uint8_t vpotValues[8]   = {0};

// --- Handles de tareas ---
TaskHandle_t taskCore0Handle = nullptr;
TaskHandle_t taskCore1Handle = nullptr;

// ====================================================================
// --- HELPER RS485 → MIDI ---
// ====================================================================
static void processSlaveResponse(uint8_t slaveId) {
    const ChannelData& ch = rs485.getChannel(slaveId);
    uint8_t midiCh = slaveId - 1;

    // --- Fader → Pitch Bend ---
    if (ch.touchState) {
        uint16_t pb  = ch.faderPos & 0x3FFF;
        byte msg[3]  = { (byte)(0xE0 | midiCh), (byte)(pb & 0x7F), (byte)(pb >> 7) };
        sendMIDIBytes(msg, 3);
    }

    // --- Botones → Note On/Off ---
    uint8_t changed = ch.buttons ^ ch.prevButtons;
    if (changed) {
        const uint8_t noteBase[4] = { 0, 8, 16, 24 };
        for (uint8_t bit = 0; bit < 4; bit++) {
            if (changed & (1 << bit)) {
                bool    isOn = (ch.buttons & (1 << bit)) != 0;
                uint8_t note = noteBase[bit] + midiCh;
                uint8_t vel  = isOn ? 127 : 0;
                byte msg[3]  = { (byte)(isOn ? 0x90 : 0x80), note, vel };
                sendMIDIBytes(msg, 3);
            }
        }
    }

    // --- Encoder → CC ---
    if (ch.encoderDelta != 0) {
        uint8_t cc  = 16 + midiCh;
        uint8_t val = (ch.encoderDelta > 0) ? 65 : 63;
        byte msg[3] = { (byte)(0xB0 | midiCh), cc, val };
        sendMIDIBytes(msg, 3);
    }
}

// ====================================================================
// --- TAREA CORE 0 — MIDI + RS485 ---
// ====================================================================
void taskCore0(void* pvParameters) {
    log_e("MIDI task arrancando en Core %d", xPortGetCoreID());
    static unsigned long lastStatusLog = 0;  // ← MOVER AQUÍ
    
    for (;;) {
        uint8_t rx_buf[64];
        uint32_t count = tud_midi_stream_read(rx_buf, sizeof(rx_buf));
        if (count > 0) {
            for (uint32_t i = 0; i < count; i++)
                processMidiByte(rx_buf[i]);
        }

        if (logicConnectionState == ConnectionState::CONNECTED) {
            for (uint8_t id = 1; id <= NUM_SLAVES; id++) {
                if (rs485.hasNewSlaveData(id))
                    processSlaveResponse(id);
            }
        }

        tickCalibracion();
        
        // ← LOG DE ESTADO (DENTRO DEL LOOP):
        if (millis() - lastStatusLog > 2000) {
            lastStatusLog = millis();
            
            const char* stateStr = "UNKNOWN";
            if (logicConnectionState == ConnectionState::DISCONNECTED) stateStr = "DISCONNECTED";
            else if (logicConnectionState == ConnectionState::MIDI_HANDSHAKE_COMPLETE) stateStr = "HANDSHAKE_OK";
            else if (logicConnectionState == ConnectionState::CONNECTED) stateStr = "CONNECTED";
            
            log_i("[STATUS] %s | g_logicConnected=%d", stateStr, g_logicConnected);
        }
        
        vTaskDelay(1);
    }
}

// ====================================================================
// --- TAREA CORE 1 — TRANSPORTE ---
// ====================================================================
void taskCore1(void* pvParameters) {
    for (;;) {
        Transporte::update();
        vTaskDelay(10);
    }
}

// ====================================================================
// --- SETUP ---
// ====================================================================
void setup() {
    randomSeed(esp_random());
    Serial.begin(115200);
    log_i("=== BOOT S3-02 Extender ===");

    // 1. USB
    log_i("1. USB.begin()...");
    USB.begin();
    delay(100);
    log_i("   USB OK");

    // 2. Transporte
    log_i("2. Transporte::begin()...");
    Transporte::begin();
    log_i("   Transporte OK");

    // 3. RS485
    log_i("3. rs485.begin(%d)...", NUM_SLAVES);
    rs485.begin(NUM_SLAVES);
    log_i("   RS485 OK. Slaves: %d", NUM_SLAVES);

    // 4. MIDI (sin delay largo)
    log_i("4. MIDI.begin()...");
    MIDI.begin();
    log_i("   MIDI OK");

    // 5. Info PSRAM
    log_i("PSRAM: %d bytes total, %d bytes libre",
          ESP.getPsramSize(), ESP.getFreePsram());

    // 6. Crear tareas
    log_i("5. Creando tareas...");
    xTaskCreatePinnedToCore(taskCore0, "MIDI", 4096, NULL, 2, &taskCore0Handle, 0);
    xTaskCreatePinnedToCore(taskCore1, "TRANSP", 4096, NULL, 1, &taskCore1Handle, 1);
    log_i("   Tareas creadas");

    log_i("=== S3-02 Extender ACTIVO. Slaves: %d ===", NUM_SLAVES);
}

void loop() { vTaskDelay(portMAX_DELAY); }