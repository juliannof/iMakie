//src/main.cpp
#include <Arduino.h>

// --- INCLUDES GLOBALES DE LA APLICACIÓN ---
#include "config.h"          // Archivo de configuración global
#include "display/Display.h" // Funciones de display
#include "hardware/encoder/Encoder.h" // <<<<<<<<<<<<<<<<< Incluye la clase Encoder
#include "hardware/Hardware.h" // <<<<<<<<<<<<<<<<< ¡Incluye tu nuevo módulo de hardware!



// --- INSTANCIAS GLOBALES PARA LA PANTALLA ---
TFT_eSPI tft = TFT_eSPI(); // Objeto principal de la pantalla
TFT_eSprite header(&tft), mainArea(&tft), vuSprite(&tft), vPotSprite(&tft); // Sprites para dibujar

// --- DECLARACIONES EXTERNAS PARA FUNCIONES DE PANTALLA (desde display/Display.cpp) ---
// Estas se incluyen por "display/Display.h" si están declaradas allí.


extern void initDisplay();               // Inicialización de toda la pantalla
extern void updateDisplay();             // Función principal de refresco de pantalla
extern void setScreenBrightness(uint8_t brightness); // Control del brillo de la pantalla
extern void setVPotLevel(int8_t level);  // <<<<<<<<<<<<<<<<< Función para establecer el nivel del VPot en Display.cpp

extern int currentVPotLevel;
extern bool needsVPotRedraw;

// --- VARIABLES DE ESTADO DE CANAL ---
String assignmentString = "CH-01 ";
String trackName = "Track99";
bool recStates = false;
bool soloStates = false;
bool muteStates = false;
bool selectStates = true;
bool vuClipState = false;
float vuPeakLevels = 0.0f;
float faderPositions = 0.0f;
float vuLevels = 0.0f; 
unsigned long vuLastUpdateTime = 0;
unsigned long vuPeakLastUpdateTime = 0;


// ===================================
// --- FUNCIONES DE CALLBACK ---
// ===================================
// Este manejador ahora solo se preocupa por la lógica de *Negocio*,
// sin preocuparse por encender LEDs directamente, eso lo hace Hardware.cpp.
void myButtonEventHandler(ButtonId id) {
    Serial.print("Evento de boton: ");

    switch (id) {
        case ButtonId::REC:
            recStates = !recStates; // <<<<<<<<<<<<<<<< TOGGLE EL ESTADO
            Serial.print("REC (pulsado) - recStates ahora es: "); Serial.println(recStates ? "TRUE" : "FALSE");
            // Aquí iría tu lógica de negocio de REC
            needsMainAreaRedraw = true; // MARCAR PARA REDIBUJAR
            break;
        case ButtonId::SOLO:
            soloStates = !soloStates; // <<<<<<<<<<<<<<<< TOGGLE EL ESTADO
            Serial.print("SOLO (pulsado) - soloStates ahora es: "); Serial.println(soloStates ? "TRUE" : "FALSE");
            // Lógica de Solo
            needsMainAreaRedraw = true; // MARCAR PARA REDIBUJAR
            break;
        case ButtonId::MUTE:
            muteStates = !muteStates; // <<<<<<<<<<<<<<<< TOGGLE EL ESTADO
            Serial.print("MUTE (pulsado) - muteStates ahora es: "); Serial.println(muteStates ? "TRUE" : "FALSE");
            // Lógica de Mute
            needsMainAreaRedraw = true; // MARCAR PARA REDIBUJAR
            break;
        case ButtonId::SELECT:
            selectStates = !selectStates; // <<<<<<<<<<<<<<<< TOGGLE EL ESTADO
            Serial.print("SELECT (pulsado) - selectStates ahora es: "); Serial.println(selectStates ? "TRUE" : "FALSE");
            
            // Lógica de Select
            //needsMainAreaRedraw = true; // MARCAR PARA REDIBUJAR
            needsHeaderRedraw = true; // Si la cabecera muestra estado de selección
            break;
        case ButtonId::ENCODER_SELECT:
            Serial.println("ENCODER SELECT (pulsado) - Actuando sobre la selección actual.");
            Encoder::reset();
            needsVPotRedraw = true;  // Flag para redibujar
            // Lógica del pulsador del encoder (no controla Neopixel directamente en tu layout)
            //toggleLedBuiltin(); // Ejemplo: Usa el LED integrado
            //needsMainAreaRedraw = true; // MARCAR PARA REDIBUJAR (si la acción del encoder selector afecta el área principal)
            //currentVPotLevel = VPOT_DEFAULT_LEVEL; // Resetear el vPot al valor por defecto
            //setVPotLevel(VPOT_DEFAULT_LEVEL); // Actualizar la pantalla con el nuevo nivel
            break;
        //case ButtonId::FADER_TOUCH:
        //    Serial.println("FADER TOUCH (pulsado) - Fader activado.");
        //    needsMainAreaRedraw = true; // Si decides usarlo, también podrías querer que redibuje
        //    break;
        case ButtonId::UNKNOWN:
            Serial.println("Botón desconocido!");
            // No suele ser necesario redibujar para un botón desconocido,
            // pero si tu UI tiene un mensaje de error que se muestra, entonces sí.
            // needsMainAreaRedraw = true;
            break;
    }
}



// ===================================
// --- SIMULACIÓN DE SEÑAL VU ---
// ===================================
static unsigned long lastVuSimTime = 0;
static const unsigned long VU_SIM_INTERVAL_MS = 50; // Actualizar la simulación cada 50ms (20 FPS)
// Variables de estado para la simulación de señal
static float currentRmsLevel = 0.5f; // Nivel RMS base que variará lentamente
static float rmsTargetLevel = 0.5f;  // Nivel RMS hacia el que tendemos
static unsigned long lastRmsChangeTime = 0;
static const unsigned long RMS_CHANGE_INTERVAL_MS = 500; // Cambiar el target RMS cada 500ms
void simulateVuSignal() {
    unsigned long currentTime = millis();
    if (currentTime - lastVuSimTime >= VU_SIM_INTERVAL_MS) {
        // --- Nivel Instantáneo (vuLevels) ---
        // Genera un valor aleatorio entre 0.0 y 0.9 para el nivel del VU.
        // `random(0, 100)` para obtener un entero entre 0 y 99.
        // Luego lo dividimos por 100.0f para obtener un float entre 0.0 y 0.99.
        vuLevels = random(0, 99) / 100.0f;
        // --- Nivel de Pico (vuPeakLevels) ---
        // El pico suele ser el nivel instantáneo más alto en un cierto período de tiempo.
        // Lo simulamos como un valor aleatorio ligeramente superior o igual al nivel actual.
        // Y bajará lentamente con el tiempo.
        if (vuLevels > vuPeakLevels) { // Si el nivel actual supera al pico, el pico sube.
            vuPeakLevels = vuLevels;
        } else { // Si no, el pico baja lentamente.
            vuPeakLevels -= 0.02f; // Puedes ajustar la velocidad de decaída.
            if (vuPeakLevels < 0.0f) vuPeakLevels = 0.0f; // Asegura que no baje de 0.
        }
        // --- Estado de Clip (vuClipState) ---
        // Simulamos un clip si el nivel instantáneo es muy alto (ej. > 0.95)
        vuClipState = (vuLevels > 0.95f);
        
        // --- Marcamos que el VU necesita redibujarse ---
        needsVUMetersRedraw = true; // Variable "extern bool needsVUMetersRedraw" de Display.h
        lastVuSimTime = currentTime;
    }
}


void setup() {

    Serial.begin(115200);
    delay(1000); // Espera un momento para que el monitor serial se conecte
    // --- Inicialización y configuración de la Pantalla ---
    initDisplay(); 
    //log_v("Track 1 DEMO started."); // Descomentar si usas esp_log
    log_e("Conectado a DAW. Forzando redibujo completo.");
    Serial.println("Track 1 DEMO started.");

    // --- Inicialización del Hardware (incluido el Encoder) ---
    initHardware(); // <<<<<<<<<<<<<<<<< Llama a la inicialización del hardware
   
    
    // === Inicializa el vPot a su valor por defecto ===
    setVPotLevel(VPOT_DEFAULT_LEVEL); // Establece el nivel inicial del vPot en la pantalla
    // Registrar los callbacks - main.cpp YA NO pregunta 'if (buttonX.wasPressed())'
    // sino que DICE a Hardware.cpp QUÉ hacer cuando un botón es presionado.
    registerButtonEventCallback(myButtonEventHandler);

    Encoder::begin();
}

void loop() {

    // --- Lógica de actualización del encoder ---
    log_e("Conectado a DAW. Forzando redibujo completo.");
    Encoder::update();

    if (Encoder::hasChanged()) {
        //static long lastValue = 0;
        long value = Encoder::getCount();

        // --- Actualizar vPot DIRECTAMENTE en Encoder ---
        int newVPotLevel = constrain(value, -7, 7); // totalSegments = 7
        if (newVPotLevel != Encoder::currentVPotLevel) {
            Encoder::currentVPotLevel = newVPotLevel;  // <<<< importante
            needsVPotRedraw = true;  // Flag para redibujar
        }
}
    
    updateButtons(); // Esto activará los callbacks si hay eventos

    // --- Simulación de Señal VU ---
    //simulateVuSignal(); // <<<<<<<<<<<<<<<< ¡Llamada a la función de simulación!

    // --- Lógica de Actualización de la Pantalla ---
    // Delega toda la lógica de actualización y dibujo de la pantalla a updateDisplay().
    updateDisplay();
    
    // Aquí iría el resto de TU LÓGICA DE APLICACIÓN principal
    delay(1); // Pequeño delay no bloqueante.
}