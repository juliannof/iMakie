// src/main.cpp
#include "config.h"             // Lo primero, siempre
#include "comUA/UARTHandler.h"
#include "display/Display.h"
#include "hardware/Hardware.h"

// --- DEFINICIÓN DE OBJETOS Y VARIABLES GLOBALES ---
TFT_eSPI tft;
TFT_eSprite header(&tft), mainArea(&tft), vuSprite(&tft);

// Inicialización del Trellis. Adafruit_NeoTrellis es un array de objetos.
Adafruit_NeoTrellis t_array[Y_DIM / 4][X_DIM / 4] = {{ Adafruit_NeoTrellis(0x2F), Adafruit_NeoTrellis(0x2E) }};
Adafruit_MultiTrellis trellis((Adafruit_NeoTrellis *)t_array, Y_DIM / 4, X_DIM / 4);

// Arrays de estado para los botones de las pistas (REC, SOLO, MUTE, SELECT)
String trackNames[9];
bool recStates[9]={false}, soloStates[8]={false}, muteStates[8]={false}, selectStates[8]={false};

// Variables para los vúmetros
float vuLevels[9] = {0.0f};
bool vuClipState[9] = {false};
unsigned long vuLastUpdateTime[9] = {0}; // Timestamp de la última vez que el nivel de VU fue actualizado
float vuPeakLevels[9] = {0.0f};             // Nivel de pico
unsigned long vuPeakLastUpdateTime[9] = {0}; // Timestamp de la última vez que el pico de VU fue actualizado

// String para TimeCode / Beats (usado para display, actualizado por MIDI)0
String timeCodeString = "00:00:00:00"; 
String beatsString = "  1. 1. 1.  "; // Valor por defecto

// String para el display de asignación
String assignmentString = "--";

// Posiciones de los faders (normalizadas de 0.0 a 1.0)
float faderPositions[9] = {0.0f};

// --- BANDERAS DE REDIBUJO GLOBALES ---
bool needsTOTALRedraw = true;     // Para forzar el redibujo inicial al arrancar y en cambios de estado mayores
bool needsMainAreaRedraw = true;  // Para el redibujo inicial de MainArea (botones y nombres)
bool needsHeaderRedraw = true;    // Para el redibujo inicial de Header (tiempo, tempo, asignación)
bool needsVUMetersRedraw = true;  // Para el redibujo inicial de VUMeters

// Variables de estado de conexión
//volatile ConnectionState logicConnectionState = ConnectionState::DISCONNECTED;
volatile ConnectionState logicConnectionState = ConnectionState::CONNECTED;


// Variables para BPM y Time Display
float projectTempo = 120.0f; // Valor por defecto
char tempoString[12]; // Buffer para el string del tempo (ej: "120.00 BPM")
DisplayMode currentTimecodeMode = MODE_BEATS; // Modo inicial del display de tiempo

// Variables para la sincronización del display de tiempo (usado por MIDIProcessor y Display)
unsigned long lastDisplayCC_Time = 0; // Timestamp de la última vez que se recibió un CC de display
bool displayDataUpdated = false;      // Flag que indica que hay nuevos datos 'dirty' listos para ser copiados

// Buffers de caracteres para el display de tiempo
char timeCodeChars[13]; // This can probably be removed if you only use clean/dirty
char beatsChars[13];    // This can probably be removed if you only use clean/dirty
char timeCodeChars_dirty[13];
char beatsChars_dirty[13];
char timeCodeChars_clean[13];
char beatsChars_clean[13];

// --- DEFINICIÓN DE VARIABLES GLOBALES ---
// Aquí sí se reserva la memoria. Inicializamos a 0.

bool btnState[32] = {false};
bool globalShiftPressed = false;
int currentPage = 1;

int currentMasterFader = 0;
int currentMeterValue = 0;


// Variables externas necesarias (deben estar definidas en main.cpp o config.cpp)
extern int currentPage;
extern bool globalShiftPressed;
extern bool needsMainAreaRedraw;



void setup() {
  Serial.begin(115200); // Inicializar comunicación serial para depuración
  log_i("Iniciando setup...");

  // Inicialización de buffers de tiempo con espacios y null terminators
  memset(timeCodeChars_dirty, ' ', 12); timeCodeChars_dirty[12] = '\0';
  memset(beatsChars_dirty,   ' ', 12); beatsChars_dirty[12]   = '\0';
  memset(timeCodeChars_clean, ' ', 12); timeCodeChars_clean[12] = '\0';
  memset(beatsChars_clean,   ' ', 12); beatsChars_clean[12]   = '\0';
  
  // Inicializa el string del tempo
  snprintf(tempoString, sizeof(tempoString), "%.2f BPM", projectTempo);
  
  // Inicialización de módulos de hardware y comunicación
  initUART();
  log_v("initUART() completado.");
  initDisplay();
  log_v("initDisplay() completado.");
  initHardware();
  log_v("initHardware() completado.");
  
  log_i("--- Sistema Modularizado - LISTO ---");
}

void loop() {
    static bool wasConnected = false; // Flag para detectar cambios en el estado de conexión
    
    handleUART(); // Gestiona la comunicación UART con la Pico y procesamiento MIDI
    
    //handleDisplaySynchronization(); // Sincroniza los buffers de display de tiempo

    // --- LÓGICA DE GESTIÓN DE ESTADO DE CONEXIÓN Y ACCIONES ASOCIADAS ---
    if (logicConnectionState == ConnectionState::CONNECTED) {
        if (!wasConnected) { // Transición de no conectado a conectado
            wasConnected = true;
            needsTOTALRedraw = true;     // Forzar un redibujo completo al conectar
            log_e("Conectado a DAW. Forzando redibujo completo.");
        }
        
       
        handleVUMeterDecay();   // Gestiona el decaimiento visual de los VUMeters
        updateLeds();           // Actualiza los LEDs del Trellis
        handleHardwareInputs(); // Procesa inputs de la controladora
       
    } else { // Estados NO conectados (DISCONNECTED, AWAITING_SESSION, MIDI_HANDSHAKE_COMPLETE)
        if (wasConnected) { // Transición de conectado a desconectado
            resetToStandbyState(); // Limpiará los estados y activará needsTOTALRedraw
            wasConnected = false;
            log_i("Desconectado de DAW. Transicionando a modo standby.");
            needsTOTALRedraw = true; // Forzar redibujo de pantalla offline
        }
    }
    
    // updateDisplay() se llama siempre, pero su lógica interna decide qué dibujar
    // basándose en el estado de conexión y las banderas de redibujo.
    updateDisplay(); 
}