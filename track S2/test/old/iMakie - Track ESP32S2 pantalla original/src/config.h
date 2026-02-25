// src/config.h
#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>


// ====================================================================
// --- 1. DEFINICIONES (Macros) ---
// ====================================================================
// --- PINES Y HARDWARE ---

#define ENCODER_CLK_PIN     13 // GPIO para el CLK del encoder (A)
#define ENCODER_DT_PIN      12 // GPIO para el DT del encoder (B)
#define ENCODER_SW_PIN      11 // GPIO para el Switch (botón) del encoder

// --- PROTOCOLO UART ---
#define USB_TO_UART_START 0xAA
#define USB_TO_UART_END   0xBB
#define ESP_TO_USB_START  0xCC
#define ESP_TO_USB_END    0xDD
#define CMD_RAW_SYSEX     0x50
#define CMD_REC_ARM       0x10
#define CMD_SOLO          0x11

// --- INTERFAZ GRÁFICA (TFT) ---
//#define TOP_ZONE_HEIGHT 30
#define MAINAREA_WIDTH 190          // Reducido
#define MAINAREA_HEIGHT 220          // Reducido
#define HEADER_HEIGHT 30

#define V_POT_HEIGHT 30
#define BUTTON_HEIGHT 30
#define BUTTON_WIDTH (TRACK_WIDTH - 8)
#define BUTTON_SPACING 5
#define VU_METER_WIDTH 150

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
// Estos #define deberían ir al principio de src/display/Display.cpp
// o en un archivo de configuración compartido si van a usarse en otros lugares también.
#define TFT_MCU_BLUE 0x025D    // Azul oscuro MCU
#define TFT_MCU_GREEN 0x3F20   // Verde MCU 
#define TFT_MCU_YELLOW 0xFFC0  // Amarillo MCU
#define TFT_MCU_RED 0xF800     // Rojo MCU
#define TFT_MCU_GRAY 0x4A69    // Gris MCU
#define TFT_MCU_DARKGRAY 0x18C3 // Gris oscuro MCU



// --- CONSTANTES PARA EL DISPLAY DE TIEMPO (de tu config.h original) ---

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

// --- VARIABLES DE ESTADO DE CANAL ---
extern String trackNames;
extern bool recStates, soloStates, muteStates, selectStates;
extern float vuLevels;
extern bool vuClipState; 
extern unsigned long vuLastUpdateTime;
extern float vuPeakLevels;             // <-- NUEVO: Nivel del pico
extern unsigned long vuPeakLastUpdateTime; // <-- NUEVO: Timer para el pico
extern float faderPositions; // <-- Para los faders



// --- BANDERAS DE REDIBUJO ---
extern bool needsTOTALRedraw;      // <-- Para forzar el redibujo completo
extern bool needsMainAreaRedraw;   // <-- Bandera para MainArea
extern bool needsHeaderRedraw;     // <-- Bandera para Header
extern bool needsVUMetersRedraw;   // <-- Bandera para Vúmetros

// --- VARIABLES DE ESTADO GENERAL ---
extern volatile ConnectionState logicConnectionState;
extern String assignmentString; // <-- Tu String de asignación original



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