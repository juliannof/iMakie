// src/config.h
#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Adafruit_NeoTrellis.h"
// ====================================================================
// --- 1. DEFINICIONES (Macros) ---
// ====================================================================
// --- PINES Y HARDWARE ---
#define PICO_TX_PIN   15
#define PICO_RX_PIN   16
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
#define HEADER_HEIGHT 50
#define MAIN_AREA_WIDTH 440
#define VU_METER_wIDTH 40

#define TRACK_WIDTH (SCREEN_WIDTH / 8)
#define BUTTON_HEIGHT 30
#define BUTTON_WIDTH (TRACK_WIDTH - 8)
#define BUTTON_SPACING 5

#define VU_METER_HEIGHT 150
// NO DEFINIR VU_METER_AREA_Y AQUÍ. Se define en Display.h o Display.cpp o main.cpp donde se ensambla.
// Mantendré la definición anterior si la necesitas, ajustándola.
#define VU_METER_AREA_Y (HEADER_HEIGHT + 140) // Este macro estaba antes, lo mantengo.

// --- COLORES ---
// Colores MCU para Neopixels o TFT, sin depender de 'tft'
#define TFT_MCU_BLUE     0x025D    // Azul oscuro MCU
#define TFT_MCU_GREEN    0x3F20   // Verde MCU 
#define TFT_MCU_YELLOW   0xFFC0  // Amarillo MCU
#define TFT_MCU_RED      0xF800     // Rojo MCU
#define TFT_MCU_GRAY     0x3186 // Gris azulado estándar TFT
#define TFT_MCU_DARKGRAY 0x0842 // Gris oscuro MCU

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

#define NUM_VU_CHANNELS 9   // 8 strips + MASTER


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
extern String trackNames[9];
extern bool recStates[9], soloStates[8], muteStates[8], selectStates[8];
extern float vuLevels[9];
extern bool vuClipState[9]; 
extern unsigned long vuLastUpdateTime[9];
extern float vuPeakLevels[9];             // <-- NUEVO: Nivel del pico
extern unsigned long vuPeakLastUpdateTime[9]; // <-- NUEVO: Timer para el pico
extern float faderPositions[9]; // <-- Para los faders

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
//extern char timeCodeChars_dirty[13];
//extern char beatsChars_dirty[13];
extern char timeCodeChars_clean[13];
extern char beatsChars_clean[13];

// --- Variable para saber en qué modo está el display de tiempo ---
extern DisplayMode currentTimecodeMode;

// --- Flags para sincronizar el dibujado del display (de tu config.h original) ---
extern unsigned long lastDisplayCC_Time;
extern bool displayDataUpdated;

extern float masterMeterLevel;
extern float masterPeakLevel;
extern bool masterClip;


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

// --- ESTADO GLOBAL DE LA INTERFAZ ---
// Usamos 'extern' para decir: "Busca estas variables en otro sitio, pero existen"

extern bool btnState[32];        // Estado (Encendido/Apagado) de los 32 botones visuales
extern bool globalShiftPressed;  // ¿Está el botón SHIFT físico pulsado?
extern int currentPage;          // Página actual de botones (1 o 2)

// Variables del Master Fader (para el vúmetro lateral)
extern int currentMasterFader;   // 0 - 16383
extern int currentMeterValue;    // 0 - 12 (Nivel Vúmetro)

// ============================================================
// PALETA DE COLORES (Definición RGB)
// ============================================================
// Formato: 0xRRGGBB (Hexadecimal)
// Brillo ajustado para uso en estudio (no blinding)

#define C_OFF     0x000000 // Apagado
#define C_RED     0x640000 // Rojo
#define C_GREEN   0x006400 // Verde
#define C_BLUE    0x000064 // Azul
#define C_YELLOW  0x555500 // Amarillo (R+G)
#define C_CYAN    0x005555 // Cyan (G+B)
#define C_MAGENTA 0x550055 // Magenta (R+B)
#define C_WHITE   0x444444 // Blanco (R+G+B)
#define C_ORANGE  0x662200 // Naranja

// Array de traducción (Índice -> Valor real)
// Usaremos esto para leer el array 'colors_PG1'
static const uint32_t PALETTE[8] = {
    C_OFF,      // 0
    C_RED,      // 1
    C_GREEN,    // 2
    C_BLUE,     // 3
    C_YELLOW,   // 4
    C_CYAN,     // 5
    C_MAGENTA,  // 6
    C_WHITE     // 7
};

// ============================================================
// MAPA DE COLORES ESTÁTICOS (Página 1)
// ============================================================
// Estos son los colores "Base" cuando el botón está en reposo.
// Si el DAW envía feedback (ej: Activar Mute), sobreescribirá esto.

static const byte LED_COLORS_PG1[32] = {
    // FILA 1 (Asignación -> Cyan/Verde)
    5, 5, 5, 5, 5, 5, 6, 6, 
    
    // FILA 2 (Automatización -> Rojo/Naranja)
    1, 1, 1, 1, 1, 0, 2, 2, 
    
    // FILA 3 (Navegación -> Azul)
    3, 3, 3, 3, 4, 4, 4, 4,
    
    // FILA 4 (Utilidades -> Blanco / Variado)
    6, 2, 7, 7, 7, 7, 2, 1  // El último (PG2) en Rojo para destacar
};

// Mapa para Página 2 (Todo Azul/Blanco excepto el botón de volver)
static const byte LED_COLORS_PG2[32] = {
    3, 3, 3, 3, 3, 3, 3, 3,  // F1-F8
    3, 3, 3, 3, 3, 3, 3, 3,  // F9-F16
    3, 3, 3, 3, 4, 4, 4, 4,  // Nav (Igual PG1)
    6, 2, 7, 7, 7, 7, 2, 2   // Mods (Igual PG1) + Botón Volver (Verde)
};

// ============================================================
// MAPEO DE BOTONES NEOTRELLIS (8x4 = 32 Botones)
// ============================================================

// NOMBRES PARA LA PANTALLA (Página 1: MIXER)
static const char* labels_PG1[32] = {
    // FILA 1 (Asignación Encoders + Modos)
    "TRACK", "PAN",   "EQ",    "SEND",  "PLUG",  "INST",  "FLIP",  "GLOB",
    
    // FILA 2 (Automatización + Vistas)
    "READ",  "WRIT",  "TCH",   "LTCH",  "TRIM",  "OFF",   "SOLO0", "MUTE0",
    
    // FILA 3 (Navegación + Modificadores) 
    "BANK<", "BANK>", "CHAN<", "CHAN>", "ZOOM",  "SCRUB", "NUDGE", "MARK",
    
    // FILA 4 (Utilidades + Transporte Global)
    "UNDO",  "SAVE",  "SHIFT", "CTRL",  "OPT",   "CMD",   "ENTER", ">>PG2"
};

// NOTAS MIDI PARA PÁGINA 1 (Protocolo Mackie MCU)
// 0x00 significa "Sin función MIDI directa" (ej: Cambio de página local)
static const byte MIDI_NOTES_PG1[32] = {
    // FILA 1
    0x28, 0x2A, 0x2C, 0x2B, 0x2D, 0x2E, 0x32, 0x33, 
    
    // FILA 2 (Automation: Read=0x4A, Write=0x4B...)
    0x4A, 0x4B, 0x4D, 0x4E, 0x4C, 0x4F, 0x57, 0x58, // Solo0 y Mute0 suelen ser Clear Solo/Mute
    
    // FILA 3 (Nav: Bank=46/47, Chan=48/49, Zoom=100)
    0x2E, 0x2F, 0x30, 0x31, 0x64, 0x65, 0x66, 0x54, 
    
    // FILA 4 (Mods: Shift=70, Ctrl=71... Save=80, Undo=76)
    0x4C, 0x50, 0x46, 0x47, 0x48, 0x49, 0x52, 0x00 
};

// ------------------------------------------------------------

// NOMBRES PARA LA PANTALLA (Página 2: FUNCTION KEYS)
static const char* labels_PG2[32] = {
    // FILA 1 (F1 - F8)
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8",
    
    // FILA 2 (F9 - F16)
    "F9", "F10", "F11", "F12", "F13", "F14", "F15", "F16",
    
    // FILA 3 (Misma navegación que Fila 3 PG1 para no perderse)
    "BANK<", "BANK>", "CHAN<", "CHAN>", "ZOOM", "SCRUB", "NUDGE", "MARK",
    
    // FILA 4 (Mismos modificadores que Fila 4 PG1)
    "UNDO", "SAVE", "SHIFT", "CTRL", "OPT", "CMD", "ENTER", ">>PG1"
};

// NOTAS MIDI PARA PÁGINA 2
static const byte MIDI_NOTES_PG2[32] = {
    // FILA 1 (F1=54 ... F8=61)
    0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D,
    
    // FILA 2 (F9 ... F16)
    0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 
    
    // FILA 3 (Copia exacta de PG1)
    0x2E, 0x2F, 0x30, 0x31, 0x64, 0x65, 0x66, 0x54, 
    
    // FILA 4 (Copia exacta de PG1 excepto último botón)
    0x4C, 0x50, 0x46, 0x47, 0x48, 0x49, 0x52, 0x00 
};

// Textos cuando SHIFT está MANTENIDO (Capa Shift)
static const char* labels_PG1_SHIFT[32] = {
    // Fila 1 (V-Pots) - Shift suele cambiar parámetros globales
    "GLOBAL", "FINE",  "LOW",   "MID",   "HI",    "FREQ",  "___",   "___", 
    
    // Fila 2 (Automatización) - A veces hace "Update" o "Punch"
    "OFF",    "TRIM",  "LATCH", "TOUCH", "WRITE", "READ",  "UNSOLO","UNMUTE", 
    
    // Fila 3 (Navegación) - AQUÍ ESTÁ LA MAGIA
    "SHIFT",  "ALT",   "OPT",   "CMD",   "CHAN <","CHAN >","ZOOM -","ZOOM +", 
    
    // Fila 4 (Utilidades)
    "REDO",   "SAVE AS","OK",   "CNCL",  "MARKER","NUDGE", "TAP",   ">> PG2"
};