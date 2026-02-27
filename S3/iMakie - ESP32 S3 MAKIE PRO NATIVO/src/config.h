// src/config.h
#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>            // ← TFT_eSPI.h → LovyanGFX.hpp
#include "Adafruit_NeoTrellis.h"

// ====================================================================
// --- CLASE LGFX (antes en User_Setup.h, ahora aquí) ---
// ====================================================================

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7796  _panel_instance;
    lgfx::Bus_SPI       _bus_instance;

public:
    LGFX(void)
    {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host    = SPI2_HOST;
            cfg.spi_mode    = 0;
            cfg.freq_write  = 80000000;
            cfg.freq_read   = 20000000;
            cfg.spi_3wire   = false;
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk    = 10;
            cfg.pin_mosi    = 11;
            cfg.pin_miso    = 4;
            cfg.pin_dc      = 12;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = 14;
            cfg.pin_rst          = 13;
            cfg.pin_busy         = -1;
            cfg.panel_width      = 320;
            cfg.panel_height     = 480;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = true;
            cfg.invert           = false;
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false;
            _panel_instance.config(cfg);
        }
        // Sin Light_PWM — backlight via digitalWrite en initDisplay()
        setPanel(&_panel_instance);
    }
};

#define TFT_BL  18  // Backlight — digitalWrite puro


// ====================================================================
// --- 1. DEFINICIONES (Macros) ---
// ====================================================================

// --- HARDWARE NEOTRELLIS ---
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

// --- INTERFAZ GRÁFICA ---
#define SCREEN_WIDTH        480
#define SCREEN_HEIGHT       320
#define HEADER_HEIGHT        50

#define MAIN_AREA_WIDTH     480
#define MAIN_AREA_HEIGHT    ((SCREEN_HEIGHT - HEADER_HEIGHT) / 2)   // 135px
#define MAIN1_Y             HEADER_HEIGHT                            //  50px
#define MAIN2_Y             (HEADER_HEIGHT + MAIN_AREA_HEIGHT)       // 185px

#define TRACK_WIDTH         (SCREEN_WIDTH / 8)                       //  60px
#define BUTTON_HEIGHT        30
#define BUTTON_WIDTH        (TRACK_WIDTH - 8)                        //  52px
#define BUTTON_SPACING        5

#define VU_METER_HEIGHT     MAIN_AREA_HEIGHT                         // 135px
#define VU_METER_AREA_Y     MAIN2_Y                                  // 185px

#define NUM_VU_CHANNELS 9


// --- COLORES GENERALES ---
#define TFT_MCU_BLUE        0x025D
#define TFT_MCU_GREEN       0x3F20
#define TFT_MCU_YELLOW      0xFFC0
#define TFT_MCU_RED         0xF800
#define TFT_MCU_GRAY        0x3186
#define TFT_MCU_DARKGRAY    0x0842

// --- COLORES VU METERS ---
// LovyanGFX: color565() es método de instancia → usamos valores precalculados
#define VU_GREEN_OFF        0x0120   // color565(0,  36,  0) — oscuro
#define VU_GREEN_ON         0x05C0   // color565(0,  230,  0) — aprox
#define VU_YELLOW_OFF       0x30C0   // color565(70,  60,  0)
#define VU_YELLOW_ON        TFT_YELLOW
#define VU_RED_OFF          0x2800   // color565(80,   0,  0)
#define VU_RED_ON           TFT_RED
#define VU_PEAK_COLOR       0xB596   // color565(180,180,180)

// --- COLORES UI ---
#define TFT_BG_COLOR        TFT_BLACK
#define TFT_TEXT_COLOR      TFT_WHITE
#define TFT_BUTTON_COLOR    TFT_DARKGREY
#define TFT_REC_COLOR       TFT_RED
#define TFT_SOLO_COLOR      TFT_ORANGE
#define TFT_MUTE_COLOR      TFT_RED
#define TFT_BUTTON_TEXT     TFT_WHITE
#define TFT_HEADER_COLOR    0x0010   // color565(0, 0, 80)
#define TFT_SELECT_BG_COLOR 0x18C3   // color565(25, 25, 35)

// --- DISPLAY DE TIEMPO ---
#define TIME_DISPLAY_DIGIT_COUNT     10
#define FONT_CHAR_WIDTH_MONO         12
#define FONT_HEIGHT_MONO             24
#define FONT_CENTER_Y_OFFSET_HEADER   5
#define DOT_OFFSET_X                 -2
#define DOT_OFFSET_Y                  6
#define DOT_RADIUS                    2
#define DISPLAY_COPY_TIMEOUT         25  // ms


// ====================================================================
// --- 2. ENUMERACIONES ---
// ====================================================================

enum class ConnectionState {
    DISCONNECTED,
    AWAITING_SESSION,
    MIDI_HANDSHAKE_COMPLETE,
    CONNECTED
};

enum DisplayMode { MODE_BEATS, MODE_SMPTE };


// ====================================================================
// --- 3. DECLARACIONES GLOBALES (extern) ---
// ====================================================================

// --- OBJETOS DE HARDWARE ---
extern LGFX tft;                                    // ← TFT_eSPI → LGFX
extern LGFX_Sprite header, mainArea, vuSprite;      // ← TFT_eSprite → LGFX_Sprite
extern Adafruit_MultiTrellis trellis;

// --- VARIABLES DE ESTADO DE CANAL ---
extern String trackNames[9];
extern bool recStates[8], soloStates[8], muteStates[8], selectStates[8];
extern float vuLevels[9];
extern bool vuClipState[9];
extern unsigned long vuLastUpdateTime[9];
extern float vuPeakLevels[9];
extern unsigned long vuPeakLastUpdateTime[9];
extern float faderPositions[9];

// --- BANDERAS DE REDIBUJO ---
extern bool needsTOTALRedraw;
extern bool needsMainAreaRedraw;
extern bool needsHeaderRedraw;
extern bool needsVUMetersRedraw;

// --- VARIABLES DE ESTADO GENERAL ---
extern volatile ConnectionState logicConnectionState;
extern String assignmentString;
extern float projectTempo;
extern String timeCodeString;
extern String beatsString;
extern char tempoString[12];

// --- Display por CC ---
extern char timeCodeChars[13];
extern char beatsChars[13];
extern char timeCodeChars_clean[13];
extern char beatsChars_clean[13];
extern DisplayMode currentTimecodeMode;
extern unsigned long lastDisplayCC_Time;
extern bool displayDataUpdated;

// --- Master ---
extern float masterMeterLevel;
extern float masterPeakLevel;
extern bool masterClip;

// --- Mackie char map ---
const char MACKIE_CHAR_MAP[64] = {
    ' ', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
    ' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?'
};

// --- ESTADO GLOBAL DE LA INTERFAZ ---
extern bool btnStatePG1[32];
extern bool btnStatePG2[32];
extern bool globalShiftPressed;
extern int currentPage;

// --- Master Fader ---
extern int currentMasterFader;
extern int currentMeterValue;


// ====================================================================
// --- PALETA UNIFICADA ---
// ====================================================================

struct PaletteEntry {
    uint32_t rgb888;
    uint16_t rgb565;
};

static const PaletteEntry PALETTE[9] = {
    { 0x000000, 0x2104       },  // 0 — OFF
    { 0x640000, TFT_RED      },  // 1 — RED
    { 0x006400, TFT_GREEN    },  // 2 — GREEN
    { 0x000064, 0x03FF       },  // 3 — BLUE
    { 0x555500, TFT_YELLOW   },  // 4 — YELLOW
    { 0x005555, TFT_CYAN     },  // 5 — CYAN
    { 0x550055, TFT_MAGENTA  },  // 6 — MAGENTA
    { 0x444444, TFT_WHITE    },  // 7 — WHITE
    { 0x662200, TFT_ORANGE   },  // 8 — ORANGE
};

#define C_OFF     0x000000
#define C_WHITE   0x444444
#define C_YELLOW  0x555500
#define C_BLUE    0x000064
#define C_GREEN   0x006400
#define C_RED     0x640000


// ====================================================================
// --- 5. MAPAS DE COLORES LED ---
// ====================================================================

static const byte LED_COLORS_PG1[32] = {
    5, 5, 5, 5, 5, 5, 6, 6,
    2, 6, 4, 8, 8, 2, 2, 2,
    //3, 3, 3, 3, 4, 4, 4, 4,
    4, 4, 4, 4, 3, 3, 3, 3,
    6, 2, 7, 7, 7, 7, 2, 1
};

static const byte LED_COLORS_PG2[32] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    4, 4, 4, 4, 3, 3, 3, 3,
    6, 2, 7, 7, 7, 7, 2, 2
};

static const byte LED_COLORS_PG3[32] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 2
};


// ====================================================================
// --- 6. LABELS Y NOTAS MIDI ---
// ====================================================================

static const char* labels_PG1[32] = {
    "TRACK", "PAN",   "EQ",    "SEND",  "PLUG",  "INST",  "FLIP",  "GLOB",
    "READ",  "WRIT",  "TCH",   "LTCH",  "TRIM",  "OFF",   "SOLO0", "SMPT",
    //"BANK<", "BANK>", "CHAN<", "CHAN>",  "ZOOM",  "SCRUB", "NUDGE", "MARK",
    "ZOOM",  "SCRUB", "NUDGE", "MARK", "CHAN<", "CHAN>", "BANK<", "BANK>",
    "UNDO",  "SAVE",  "SHIFT", "CTRL",  "OPT",   "CMD",   "ENTER", ">>PG2"
};

static const byte MIDI_NOTES_PG1[32] = {
    0x28, 0x2A, 0x2C, 0x29, 0x2B, 0x2D, 0x32, 0x33,
    0x4A, 0x4B, 0x4D, 0x4E, 0x4C, 0x4F, 0x57, 0x35,
    //0x2E, 0x2F, 0x30, 0x31, 0x64, 0x65, 0x66, 0x54,
    0x64, 0x65, 0x66, 0x54, 0x30, 0x31, 0x2E, 0x2F, 
    0x51, 0x50, 0x46, 0x47, 0x48, 0x49, 0x53, 0x00
};

static const char* labels_PG2[32] = {
    "F1",    "F2",    "F3",    "F4",    "F5",    "F6",    "F7",    "F8",
    "F9",    "F10",   "F11",   "F12",   "F13",   "F14",   "F15",   "F16",
    "ZOOM",  "SCRUB", "NUDGE", "MARK", "CHAN<", "CHAN>", "BANK<", "BANK>",
    "UNDO",  "SAVE",  "SHIFT", "CTRL",  "OPT",   "CMD",   "ENTER", ">>VU"
};

static const byte MIDI_NOTES_PG2[32] = {
    0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D,
    0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45,
    0x64, 0x65, 0x66, 0x54, 0x30, 0x31, 0x2E, 0x2F,
    0x4C, 0x50, 0x46, 0x47, 0x48, 0x49, 0x52, 0x00
};

static const char* labels_PG1_SHIFT[32] = {
    "GLOBAL", "FINE",    "LOW",   "MID",    "HI",     "FREQ",  "___",    "___",
    "OFF",    "TRIM",    "LATCH", "TOUCH",  "WRITE",  "READ",  "UNSOLO", "UNMUTE",
    "SHIFT",  "ALT",     "OPT",   "CMD",    "CHAN <", "CHAN >","ZOOM -", "ZOOM +",
    "REDO",   "SAVE AS", "OK",    "CNCL",   "MARKER", "NUDGE", "TAP",    ">>PG2"
};