// include/config.h — P4 versión mínima
#pragma once
#include <Arduino.h>

// --- RS485 pines P4 ---
#define RS485_TX_PIN      50
#define RS485_RX_PIN      51
#define RS485_ENABLE_PIN  52
#define RS485_BAUD       500000

// --- Timing RS485 ---
#define RS485_TX_ENABLE_US    10
#define RS485_TX_DONE_US      10
#define RS485_RESP_TIMEOUT_US 5000
#define RS485_GAP_US          300
#define POLL_CYCLE_MS         20

// --- Slaves ---
#define NUM_SLAVES  9


// --- Enums ---
enum class ConnectionState {
    DISCONNECTED,
    AWAITING_SESSION,
    MIDI_HANDSHAKE_COMPLETE,
    CONNECTED
};

enum DisplayMode { MODE_BEATS, MODE_SMPTE };

// --- Estado de conexión global ---
extern volatile ConnectionState logicConnectionState;
extern uint8_t g_logicConnected;

// --- Variables de display ---
extern String trackNames[9];
extern bool recStates[8], soloStates[8], muteStates[8], selectStates[8];
extern uint8_t vpotValues[8];
extern float vuLevels[9];
extern bool vuClipState[9];
extern unsigned long vuLastUpdateTime[9];
extern float vuPeakLevels[9];
extern unsigned long vuPeakLastUpdateTime[9];
extern float faderPositions[9];
extern bool needsTOTALRedraw;
extern bool needsMainAreaRedraw;
extern bool needsTimecodeRedraw;
extern bool needsButtonsRedraw;
extern bool needsVUMetersRedraw;
extern bool needsHeaderRedraw;
extern String assignmentString;
extern bool btnStatePG1[32];
extern bool btnStatePG2[32];
extern bool btnFlashPG1[32];
extern bool btnFlashPG2[32];
extern char timeCodeChars_clean[13];
extern char beatsChars_clean[13];
extern DisplayMode currentTimecodeMode;

extern volatile bool g_switchToPage3;
extern volatile bool g_switchToOffline;

// --- Mackie char map ---
const char MACKIE_CHAR_MAP[64] = {
    ' ','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
    'P','Q','R','S','T','U','V','W','X','Y','Z','[','\\',']','^','_',
    ' ','!','"','#','$','%','&','\'','(',')','*','+',',','-','.','/',
    '0','1','2','3','4','5','6','7','8','9',':',';','<','=','>','?'
};

// --- Notas MIDI ---
static const byte MIDI_NOTES_PG1[32] = {
    0x28,0x2A,0x2C,0x29,0x2B,0x2D,0x32,0x33,
    0x4A,0x4B,0x4D,0x4E,0x4C,0x4F,0x57,0x35,
    0x00,0x65,0x66,0x54,0x30,0x31,0x2E,0x2F,
    0x51,0x50,0x46,0x47,0x48,0x49,0x53,0x00
};

static const byte MIDI_NOTES_PG2[32] = {
    0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,
    0x3E,0x3F,0x40,0x41,0x42,0x43,0x44,0x45,
    0x64,0x65,0x66,0x54,0x30,0x31,0x2E,0x2F,
    0x4C,0x50,0x46,0x47,0x48,0x49,0x52,0x00
};