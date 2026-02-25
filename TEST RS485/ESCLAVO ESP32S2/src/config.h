// src/config.h
#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h> // Se mantiene.

// ===================================
// --- ENUMERACIONES (Tipos de Datos) ---
// ===================================
// ** MUY IMPORTANTE: ConnectionState debe definirse solo aquí **
enum class ConnectionState {
    DISCONNECTED,
    INITIALIZING,
    AWAITING_SESSION,
    MIDI_HANDSHAKE_COMPLETE,
    CONNECTED
};

// ===================================
// --- 1. DEFINICIONES (Macros y Constantes) ---
// ===================================

// --- PINES Y HARDWARE ---
// --- LED INTEGRADO ---
#define LED_BUILTIN_PIN 15 // Pin del LED integrado en la Lolin D1 ESP32 S2 (GPIO15)
                          // Verifica el diagrama de pines de tu placa si tienes dudas.
// --- CODIGO DEL ENCODER ROTATIVO ---
#define ENCODER_PIN_A       13 // Pin A del encoder
#define ENCODER_PIN_B       12 // Pin B del encoder
#define ENCODER_SW_PIN      11 // GPIO para el Switch (botón) del encoder
#define ENCODER_DEBOUNCE_DELAY_MS 5


// <<<<<<<<<<<<<<<<<<<<<<<< DEFINICIONES DEL ENCODER ROTATIVO >>>>>>>>>>>>>>>>>>>>>>>>>>
// Retardo de rebote para la rotación del encoder en ms. Se usará como valor inicial para la variable global.
#define ENCODER_ROTARY_DEBOUNCE_DELAY_MS 5
// Retardo inicial de rebote para el encoder en ms (si fuera diferente del valor principal).
#define ENCODER_INITIAL_DEBOUNCE_DELAY_MS 50
// Umbral en ms para considerar un "click" del encoder (usado para efectos o lógica avanzada).
#define ENCODER_CLICK_THRESHOLD 500
// Rango de valores (min/max) que el encoder puede controlar, para mapeos o límites.
#define ENCODER_MAX_VALUE 127
#define ENCODER_MIN_VALUE 0


// --- BOTONES FÍSICOS ---
#define BUTTON_PIN_REC      37
#define BUTTON_PIN_SOLO     38
#define BUTTON_PIN_MUTE     39
#define BUTTON_PIN_SELECT   40
#define BUTTON_USE_INTERNAL_PULLUP true // Usar pull-up internos para los botones


// --- SENSOR TÁCTIL DEL FADER ---
#define FADER_TOUCH_PIN     T1 // Pin táctil para el fader (GPIO1 en ESP32-S2)
// Porcentaje del valor base táctil para el umbral de detección (80% significa que detecta si el valor cae por debajo del 80% del valor base).
#define FADER_TOUCH_THRESHOLD_PERCENTAGE 80


// --- NEOPixel ---
#define NEOPIXEL_PIN        36  // Pin GPIO donde están conectados los Neopixels
#define NEOPIXEL_COUNT      4   // Número total de Neopixels
#define NEOPIXEL_DEFAULT_BRIGHTNESS 5 // Brillo inicial por defecto (0-255)


// --- COLORES PREDETERMINADOS DE LOS BOTONES (PARA CADA NEOPixel) ---
// Se definen los componentes RGB para el estado "encendido" de cada Neopixel asociado a un botón.

// Neopixel 0 (asociado a REC)
#define BUTTON_REC_LED_COLOR_R 255
#define BUTTON_REC_LED_COLOR_G 0
#define BUTTON_REC_LED_COLOR_B 0 // -> Rojo

// Neopixel 1 (asociado a SOLO)
#define BUTTON_SOLO_LED_COLOR_R 255
#define BUTTON_SOLO_LED_COLOR_G 255
#define BUTTON_SOLO_LED_COLOR_B 0 // -> Amarillo

// Neopixel 2 (asociado a MUTE)
#define BUTTON_MUTE_LED_COLOR_R 255
#define BUTTON_MUTE_LED_COLOR_G 0
#define BUTTON_MUTE_LED_COLOR_B 0 // -> Rojo

// Neopixel 3 (asociado a SELECT)
#define BUTTON_SELECT_LED_COLOR_R 100
#define BUTTON_SELECT_LED_COLOR_G 100
#define BUTTON_SELECT_LED_COLOR_B 100 // -> Blanco tenue

// --- COLORES PARA OTROS BOTONES (QUE NO TIENEN NEOPIXEL DEDICADO) ---
// Estos colores se usarán si ENCODER_SELECT o FADER_TOUCH activan un Neopixel
// (ej. si controlan un píxel compartido o si se les asigna uno después).
#define BUTTON_ENCODER_LED_COLOR_R 50
#define BUTTON_ENCODER_LED_COLOR_G 50
#define BUTTON_ENCODER_LED_COLOR_B 50 // Un blanco más tenue

#define BUTTON_FADER_TOUCH_LED_COLOR_R 0
#define BUTTON_FADER_TOUCH_LED_COLOR_G 100
#define BUTTON_FADER_TOUCH_LED_COLOR_B 100 // Cian tenue para el fader (ejemplo)


// --- PROPIEDADES DE BRILLO PARA PIXELES ESPECIALES ---
// Se define el factor de brillo para el píxel 3 (asociado a ButtonId::SELECT).
#define NEOPIXEL_COLOR_3_BRIGHTNESS_FACTOR 15 // 15% para el brillo del píxel 3


// --- MAPEO DE BOTONES A ÍNDICES DE NEOPIXEL ---
// Cada uno de los 4 Neopixels se asocia a uno de los 4 botones primarios.
#define NEOPIXEL_FOR_REC        0
#define NEOPIXEL_FOR_SOLO       1
#define NEOPIXEL_FOR_MUTE       2
#define NEOPIXEL_FOR_SELECT     3


// --- Rango del VPot ---
#define VPOT_MIN_LEVEL -7
#define VPOT_MAX_LEVEL  7
#define VPOT_DEFAULT_LEVEL 0

#define BUTTON_HEIGHT 20
#define BUTTON_WIDTH 56
//#define BUTTON_SPACING 5



// --- INTERFAZ GRÁFICA (TFT) ---
#define MAINAREA_WIDTH 190          // Reducido
#define MAINAREA_HEIGHT 220          // Reducido

#define HEADER_HEIGHT 20
#define VPOT_HEIGHT 30              // Altura del área de dibujo del VPot. Usado en Display.cpp

// Los botones REC, SOLO, MUTE, SELECT individuales se dibujan en una 'fila' dentro de MainArea.
#define BUTTON_GROUP_WIDTH MAINAREA_WIDTH // Ancho para el grupo de 4 botones.
// La altura de cada botón individual dentro de la fila.
#define BUTTON_LINE_HEIGHT 25 // Altura de un botón individual (un poco menos que BUTTONS_SECTION_HEIGHT)
#define BUTTON_SPACING 2 // Espacio entre botones

// Dimensiones para cada botón individual (REC, SOLO, MUTE, SELECT)
// (MAINAREA_WIDTH - (3 * BUTTON_SPACING)) / 4 para dividirlo en 4 botones
#define BUTTON_INDIVIDUAL_WIDTH ((BUTTON_GROUP_WIDTH - (3 * BUTTON_SPING)) / 4)

#define VU_METER_WIDTH 50
#define VU_METER_AREA_Y (HEADER_HEIGHT + 140) // Posición Y del área de los Vúmetros.

// <<<<<<<<<<<<<<<< MACRO COLOR_16_BITS - NECESARIA PARA LAS DEFINICIONES DE ABAJO >>>>>>>>>>>>>>>>>>>>
// Macro para definir colores de 16 bits RGB565.
// Usa los 5 bits más significativos para R, los 6 para G, y los 5 para B.
// Permite definir colores antes de que la instancia TFT_eSPI esté disponible.
#ifndef COLOR_16_BITS
#define COLOR_16_BITS(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
#endif

// <<<<<<<<<<<<<<<< COLORES PARA EL VU METER - AHORA COMO MACROS >>>>>>>>>>>>>>>>>>>>
#define VU_GREEN_OFF  COLOR_16_BITS(0, 20, 0)       // Verde oscuro para segmentos inactivos
#define VU_GREEN_ON   TFT_GREEN                    // Verde brillante para segmentos activos
#define VU_YELLOW_OFF COLOR_16_BITS(20, 20, 0)     // Amarillo oscuro
#define VU_YELLOW_ON  TFT_YELLOW                   // Amarillo brillante
#define VU_RED_OFF    COLOR_16_BITS(20, 0, 0)       // Rojo oscuro
#define VU_RED_ON     TFT_RED                      // Rojo brillante
#define VU_PEAK_COLOR COLOR_16_BITS(150, 150, 150) // Color para el indicador de pico


// --- COLORES TFT ---
#define TFT_BG_COLOR TFT_BLACK
#define TFT_TEXT_COLOR TFT_WHITE
#define TFT_BUTTON_COLOR TFT_DARKGREY
#define TFT_REC_COLOR TFT_RED
#define TFT_SOLO_COLOR TFT_ORANGE
#define TFT_MUTE_COLOR TFT_RED
#define TFT_BUTTON_TEXT TFT_WHITE

// Colores MCU para Neopixels o TFT, sin depender de 'tft'
#define TFT_MCU_BLUE     0x025D    // Azul oscuro MCU
#define TFT_MCU_GREEN    0x3F20   // Verde MCU 
#define TFT_MCU_YELLOW   0xFFC0  // Amarillo MCU
#define TFT_MCU_RED      0xF800     // Rojo MCU
#define TFT_MCU_GRAY     0x3186 // Gris azulado estándar TFT
#define TFT_MCU_DARKGRAY 0x0842 // Gris oscuro MCU


// --- CONSTANTES PARA EL DISPLAY DE TIEMPO ---
#define FONT_CHAR_WIDTH_MONO 12     // Ancho estimado de un carácter en FreeMonoBold12pt7b (ajustar si es necesario)
#define FONT_HEIGHT_MONO 24         // Altura estimada de un carácter en FreeMonoBold12pt7b (ajustar si es necesario)
#define FONT_CENTER_Y_OFFSET_HEADER 5 // Ajuste fino para centrar la fuente en HEADER_HEIGHT/2
#define DOT_OFFSET_X  -2 // Offset X desde el borde derecho del carácter.
#define DOT_OFFSET_Y   6 // Offset Y desde la base del carácter (hacia abajo).
#define DOT_RADIUS     2 // Radio del círculo del punto.
#define DISPLAY_COPY_TIMEOUT 25 // 25ms para sincronización de display


// --- PROTOCOLO UART (Si lo usas) ---
#define USB_TO_UART_START 0xAA
#define USB_TO_UART_END   0xBB
#define ESP_TO_USB_START  0xCC
#define ESP_TO_USB_END    0xDD
#define CMD_RAW_SYSEX     0x50
#define CMD_REC_ARM       0x10
#define CMD_SOLO          0x11


// ===================================
// --- 2. DECLARACIONES GLOBALES (con 'extern') ---

// --- OBJETOS TFT (Declarados en main.cpp, usados en Display.cpp) ---
extern TFT_eSPI tft;
extern TFT_eSprite header, mainArea, vuSprite, vPotSprite; 

// --- VARIABLES DE ESTADO DE CANAL (Declaradas en Display.cpp, usadas en main.cpp) ---
extern String trackName; // Corregido a singular
extern bool recStates, soloStates, muteStates, selectStates;
extern bool vuClipState; 
extern float vuLevels;
extern unsigned long vuLastUpdateTime;
extern float vuPeakLevels;             
extern unsigned long vuPeakLastUpdateTime;
extern float faderPositions; 
extern String assignmentString; 

// --- BANDERAS DE REDIBUJO (Declaradas en Display.cpp) ---
extern bool needsTOTALRedraw;      
extern bool needsMainAreaRedraw;   
extern bool needsHeaderRedraw;     
extern bool needsVUMetersRedraw;   
extern bool needsVPotRedraw;       // La bandera para el vPot.

// --- VARIABLES DE ESTADO GENERAL ---
extern volatile ConnectionState logicConnectionState; // Usa el enum de este mismo config.h


// --- Flags para sincronizar el dibujado del display ---
extern unsigned long lastDisplayCC_Time;
extern bool displayDataUpdated;

// --- Mapeo de caracteres Mackie (si es global) ---
const char MACKIE_CHAR_MAP[64] = {
    ' ', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', // 0-15
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^', '_', // 16-31
    ' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/', // 32-47
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?'  // 48-63
};