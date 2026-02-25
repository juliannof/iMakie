// src/config.h
#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Adafruit_NeoTrellis.h"
// ====================================================================
// --- 1. DEFINICIONES (Macros) ---
// ====================================================================
// --- PINES Y HARDWARE ---
#define PICO_TX_PIN   1
#define PICO_RX_PIN   2
#define UART_BAUDRATE 921600
#define Y_DIM 4
#define X_DIM 8
// --- PROTOCOLO UART ---
#define USB_TO_UART_START 0xAA
#define USB_TO_UART_END   0xBB
#define ESP_TO_USB_START  0xCC
#define ESP_TO_USB_END    0xDD
#define CMD_RAW_SYSEX     0x50
#define CMD_REC_ARM       0x10
#define CMD_SOLO          0x11
// ... (otros CMD que envíe el ESP32)
// --- INTERFAZ GRÁFICA (TFT) ---
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320
#define TRACK_WIDTH (SCREEN_WIDTH / 8)
#define BUTTON_HEIGHT 30
#define BUTTON_WIDTH (TRACK_WIDTH - 8)
#define BUTTON_SPACING 5
#define HEADER_HEIGHT 30
#define VU_METER_HEIGHT 150
// NO DEFINIR VU_METER_AREA_Y AQUÍ. Se define en Display.h o Display.cpp o main.cpp donde se ensambla.
// Mantendré la definición anterior si la necesitas, ajustándola.
#define VU_METER_AREA_Y (HEADER_HEIGHT + 140) // Este macro estaba antes, lo mantengo.

// --- COLORES ---
#define TFT_BG_COLOR TFT_BLACK
#define TFT_TEXT_COLOR TFT_WHITE
#define TFT_BUTTON_COLOR TFT_DARKGREY
#define TFT_REC_COLOR TFT_RED
#define TFT_SOLO_COLOR TFT_ORANGE
#define TFT_MUTE_COLOR TFT_RED
#define TFT_BUTTON_TEXT TFT_WHITE
#define TFT_HEADER_COLOR tft.color565(0, 0, 80)
#define TFT_SELECT_BG_COLOR tft.color565(25, 25, 35)

// --- CONSTANTES PARA EL DISPLAY DE TIEMPO (de tu config.h original) ---
#define TIME_DISPLAY_DIGIT_COUNT 10 // Número de dígitos para Timecode/Beats
#define FONT_CHAR_WIDTH_MONO 12     // Ancho estimado de un carácter en FreeMonoBold12pt7b (ajustar si es necesario)
#define FONT_HEIGHT_MONO 24         // Altura estimada de un carácter en FreeMonoBold12pt7b (ajustar si es necesario)
#define FONT_CENTER_Y_OFFSET_HEADER 5 // Ajuste fino para centrar la fuente en HEADER_HEIGHT/2
// --- Posicionamiento del punto decimal (de tu config.h original) ---
#define DOT_OFFSET_X  -2 // Offset X desde el borde derecho del carácter.
#define DOT_OFFSET_Y   6 // Offset Y desde la base del carácter (hacia abajo).
#define DOT_RADIUS     2 // Radio del círculo del punto.
#define DISPLAY_COPY_TIMEOUT 25 // 25ms para sincronización de display

// ====================================================================
// --- 2. ENUMERACIONES (Tipos de Datos) ---
// ====================================================================
enum class ConnectionState {
    DISCONNECTED,
    AWAITING_SESSION,
    MIDI_HANDSHAKE_COMPLETE,
    CONNECTED
};
enum DisplayMode { MODE_BEATS, MODE_SMPTE };

// ====================================================================
// --- 3. DECLARACIONES GLOBALES (con 'extern') ---

// --- OBJETOS DE HARDWARE ---
extern TFT_eSPI tft;
extern TFT_eSprite header, mainArea, vuSprite;
extern Adafruit_MultiTrellis trellis;

// --- VARIABLES DE ESTADO DE CANAL ---
extern String trackNames[8];
extern bool recStates[8], soloStates[8], muteStates[8], selectStates[8];
extern float vuLevels[8];
extern bool vuClipState[8]; 
extern unsigned long vuLastUpdateTime[8];
extern float vuPeakLevels[8];             // <-- NUEVO: Nivel del pico
extern unsigned long vuPeakLastUpdateTime[8]; // <-- NUEVO: Timer para el pico
extern float faderPositions[8]; // <-- Para los faders

// --- BANDERAS DE REDIBUJO ---
extern bool needsTOTALRedraw;      // <-- Para forzar el redibujo completo
extern bool needsMainAreaRedraw;   // <-- Bandera para MainArea
extern bool needsHeaderRedraw;     // <-- Bandera para Header
extern bool needsVUMetersRedraw;   // <-- Bandera para Vúmetros

// --- VARIABLES DE ESTADO GENERAL ---
extern volatile ConnectionState logicConnectionState;
extern String assignmentString; // <-- Tu String de asignación original
extern float projectTempo; // <-- Tu float de tempo original
extern String timeCodeString; // <-- Tu String original para TimeCode
extern String beatsString;    // <-- Tu String original para Beats
extern char tempoString[12];  // <-- Tu char array original para tempo

// --- Arrays de caracteres para el sistema de display por CC (dirty/clean) ---
extern char timeCodeChars[13];
extern char beatsChars[13];
extern char timeCodeChars_dirty[13];
extern char beatsChars_dirty[13];
extern char timeCodeChars_clean[13];
extern char beatsChars_clean[13];

// --- Variable para saber en qué modo está el display de tiempo ---
extern DisplayMode currentTimecodeMode;

// --- Flags para sincronizar el dibujado del display (de tu config.h original) ---
extern unsigned long lastDisplayCC_Time;
extern bool displayDataUpdated;


// Constante para el mapeo de caracteres Mackie --> ¡DEFINIDA DIRECTAMENTE AQUÍ!
const char MACKIE_CHAR_MAP[64] = {
    // Código Mackie (0x00 - 0x0F) -> Carácter ASCII de display
    ' ', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', // 0-15
    // Código Mackie (0x10 - 0x1F) -> Carácter ASCII de display (continuación A-Z y otros)
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^', '_', // 16-31
    // Código Mackie (0x20 - 0x2F) -> Carácter ASCII de display (directo ASCII 32-47)
    ' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/', // 32-47
    // Código Mackie (0x30 - 0x3F) -> Carácter ASCII de display (directo ASCII 48-63)
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?'  // 48-63
};