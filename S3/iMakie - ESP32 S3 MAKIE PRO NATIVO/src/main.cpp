// src/main.cpp
#include "config.h"
#include <USB.h>
#include <USBMIDI.h>
#include <class/midi/midi_device.h>
#include "midi/MIDIProcessor.h"
#include "display/Display.h"
#include "hardware/Hardware.h"
#include "RS485/RS485.h"          // ← RS485 NUEVO

/* =========================================================
   USB MIDI
   ========================================================= */
USBMIDI MIDI;

// --- DEFINICIÓN DE OBJETOS Y VARIABLES GLOBALES ---
LGFX tft;
LGFX_Sprite header(&tft), mainArea(&tft), vuSprite(&tft);

Adafruit_NeoTrellis t_array[Y_DIM/4][X_DIM/4] = {{ 
    Adafruit_NeoTrellis(0x2F),   // ← izquierdo
    Adafruit_NeoTrellis(0x2E)    // ← derecho
}};
Adafruit_MultiTrellis trellis((Adafruit_NeoTrellis *)t_array, Y_DIM / 4, X_DIM / 4);

// Arrays de estado para los botones de las pistas
String trackNames[9];
bool recStates[8]={false}, soloStates[8]={false}, muteStates[8]={false}, selectStates[8]={false};

// Variables para los vúmetros
float vuLevels[9] = {0.0f};
bool vuClipState[9] = {false};
unsigned long vuLastUpdateTime[9] = {0};
float vuPeakLevels[9] = {0.0f};
unsigned long vuPeakLastUpdateTime[9] = {0};

// Strings para display
String timeCodeString = "00:00:00:00";
String beatsString = "  1. 1. 1.  ";
String assignmentString = "--";

// Posiciones de faders
float faderPositions[9] = {0.0f};

// Banderas de redibujo
bool needsTOTALRedraw = true;
bool needsMainAreaRedraw = true;
bool needsHeaderRedraw = true;
bool needsVUMetersRedraw = true;

// Estado de conexión
extern volatile ConnectionState logicConnectionState;

// BPM y Time Display
float projectTempo = 120.0f;
char tempoString[12];
DisplayMode currentTimecodeMode = MODE_BEATS;

// Sincronización del display de tiempo
unsigned long lastDisplayCC_Time = 0;
bool displayDataUpdated = false;

// Buffers de caracteres para el display de tiempo
char timeCodeChars[13];
char beatsChars[13];
char timeCodeChars_dirty[13];
char beatsChars_dirty[13];
char timeCodeChars_clean[13];
char beatsChars_clean[13];

// Estado de botones y UI
bool btnStatePG1[32] = {false};
bool btnStatePG2[32] = {false};
bool globalShiftPressed = false;
int currentPage = 1;
int currentMasterFader = 0;
int currentMeterValue = 0;

uint8_t g_logicConnected = 0;

// Handles de tareas dual core
TaskHandle_t taskCore0Handle = NULL;
TaskHandle_t taskCore1Handle = NULL;

/* =========================================================
   HELPER RS485 → MIDI
   Convierte respuesta de un slave en mensajes MIDI para Logic
   ========================================================= */
static void processSlaveResponse(uint8_t slaveId) {
    const ChannelData& ch = rs485.getChannel(slaveId);
    uint8_t midiCh = slaveId - 1;

    // --- Fader → Pitch Bend (solo si el usuario lo toca) ---
    if (ch.touchState) {
        uint16_t pb = map(ch.faderPos, 0, 4095, 0, 16383);
        byte msg[3] = { (byte)(0xE0 | midiCh), (byte)(pb & 0x7F), (byte)(pb >> 7) };
        extern void sendMIDIBytes(const byte* data, size_t len);
        sendMIDIBytes(msg, 3);
        log_v("[RS485→MIDI] PitchBend ch%u: %u", slaveId, pb);
    }

    // --- Botones físicos → Note On/Off ---
    uint8_t changed = ch.buttons ^ ch.prevButtons;
    if (changed) {
        const uint8_t noteBase[4] = { 0, 8, 16, 24 };
        for (uint8_t bit = 0; bit < 4; bit++) {
            if (changed & (1 << bit)) {
                bool isOn    = (ch.buttons & (1 << bit)) != 0;
                uint8_t note = noteBase[bit] + midiCh;
                uint8_t vel  = isOn ? 127 : 0;
                byte msg[3]  = { (byte)(isOn ? 0x90 : 0x80), note, vel };
                extern void sendMIDIBytes(const byte* data, size_t len);
                sendMIDIBytes(msg, 3);
                log_v("[RS485→MIDI] Note %s ch%u note=%u", isOn ? "On" : "Off", slaveId, note);
            }
        }
    }

    // --- Encoder → CC (modo relativo Mackie) ---
    if (ch.encoderDelta != 0) {
        uint8_t cc      = 16 + midiCh;
        int8_t  delta   = ch.encoderDelta;
        int     total   = abs(delta) * 6;
        uint8_t dir_bit = (delta > 0) ? 0x40 : 0x00;

    while (total > 0) {
        uint8_t chunk = (uint8_t)min(total, 4);
        byte msg[3] = { (byte)(0xB0 | midiCh), cc, (uint8_t)(dir_bit | chunk) };
        sendMIDIBytes(msg, 3);
        total -= chunk;
    }
    }

    // --- Encoder button → V-Select (nota 32+ch), una por pulsación ---
    if (ch.encoderButton > 0) {
        uint8_t note = 32 + midiCh;
        byte msgOn[3]  = { 0x90, note, 127 };
        byte msgOff[3] = { 0x80, note, 0   };
        extern void sendMIDIBytes(const byte* data, size_t len);
        for (uint8_t i = 0; i < ch.encoderButton; i++) {
            sendMIDIBytes(msgOn,  3);
            sendMIDIBytes(msgOff, 3);
        }
        log_v("[RS485→MIDI] VSelect ch%u x%u", slaveId, ch.encoderButton);
    }
}

/* =========================================================
   TAREA CORE 0 — MIDI (tiempo real) + leer slaves RS485
   ========================================================= */
void taskCore0(void* pvParameters) {
    log_e("MIDI task arrancando en Core %d", xPortGetCoreID());
    for (;;) {
        // --- MIDI IN desde Logic ---
        uint8_t rx_buf[64];
        uint32_t count = tud_midi_stream_read(rx_buf, sizeof(rx_buf));
        if (count > 0) {
            log_v("[MIDI IN] %d bytes", count);
            for (uint32_t i = 0; i < count; i++) {
                processMidiByte(rx_buf[i]);
            }
        }

        // --- RS485: leer respuestas de slaves → enviar a Logic ---
        if (logicConnectionState == ConnectionState::CONNECTED) {
            for (uint8_t id = 1; id <= NUM_SLAVES; id++) {
                if (rs485.hasNewSlaveData(id)) {
                    processSlaveResponse(id);
                }
            }
        }

        vTaskDelay(1);
    }
}

/* =========================================================
   TAREA CORE 1 — UI
   ========================================================= */
void taskCore1(void* pvParameters) {
    for (;;) {
        static bool wasConnected = false;

        if (logicConnectionState == ConnectionState::CONNECTED) {
            if (!wasConnected) {
                wasConnected = true;
                needsTOTALRedraw = true;
                log_e("Conectado a DAW. Forzando redibujo completo.");
            }
            handleVUMeterDecay();
            updateLeds();
            handleHardwareInputs();
        } else {
            if (wasConnected) {
                resetToStandbyState();
                wasConnected = false;
                log_i("Desconectado de DAW. Transicionando a modo standby.");
                needsTOTALRedraw = true;
            }
        }

        updateDisplay();
        vTaskDelay(10);
    }
}

/* =========================================================
   SETUP
   ========================================================= */
void setup() {
    Serial.begin(115200);
    log_e("Iniciando setup...");

    memset(timeCodeChars_clean, ' ', 12); timeCodeChars_clean[12] = '\0';
    memset(beatsChars_clean,   ' ', 12); beatsChars_clean[12]   = '\0';

    snprintf(tempoString, sizeof(tempoString), "%.2f BPM", projectTempo);
    registerCalibrateRequestCallback([](uint8_t id) {
    rs485.setCalibrate(id);});

    log_e("Inicializando USB stack...");
    USB.begin();
    delay(200);

    log_e("Inicializando USB MIDI...");
    MIDI.begin();

    log_e("Esperando enumeracion USB del host...");
    delay(2000);

    initDisplay();
    log_e("initDisplay() completado.");
    initHardware();
    log_e("initHardware() completado.");

    // ← RS485 NUEVO: iniciar tras hardware, antes de crear tareas
    rs485.begin(NUM_SLAVES);
    log_e("rs485.begin() completado. Slaves: %d", NUM_SLAVES);

    log_e("PSRAM: %d bytes total, %d bytes libre",
          ESP.getPsramSize(), ESP.getFreePsram());
    log_e("Flash: %d bytes", ESP.getFlashChipSize());

    // Test DMA
    unsigned long t = millis();
    for (int i = 0; i < 10; i++) tft.fillScreen(TFT_BLACK);
    unsigned long elapsed = millis() - t;
    log_i("[DMA] 10x fillScreen en %lu ms — %.1f ms/frame", elapsed, elapsed / 10.0f);

    // Crear tareas pinadas a sus cores
    xTaskCreatePinnedToCore(taskCore0, "MIDI", 4096, NULL, 2, &taskCore0Handle, 0);
    xTaskCreatePinnedToCore(taskCore1, "UI",   8192, NULL, 1, &taskCore1Handle, 1);

    log_e("--- V0.2 * Dual core + RS485 activo. ---");
}

void loop() { vTaskDelay(portMAX_DELAY); }