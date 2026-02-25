// src/hardware/Hardware.cpp
#include "Hardware.h" // Incluimos su propio .h
#include "../config.h"   // Para acceder a los pines y otras configuraciones
#include <Adafruit_NeoPixel.h> // Librería Neopixel

// >>>>> FALTABAN ESTAS DECLARACIONES EXTERN EN Hardware.cpp <<<<<
// Las necesitamos para poder modificarlas desde handleButtonLedState
extern bool recStates;      
extern bool soloStates;     
extern bool muteStates;     
extern bool selectStates;   
extern bool vuClipState;
extern float vuPeakLevels;
extern float faderPositions;
extern float vuLevels; 
extern unsigned long vuLastUpdateTime;
extern unsigned long vuPeakLastUpdateTime;



// ** Neopixels **
// Parámetros: NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800 (o NEO_RGB para RGB leds)
// NEO_GRB es el más común para WS2812B
Adafruit_NeoPixel neopixels(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// ** Sensor Táctil del Fader Variables **
volatile bool isFaderTouched = false;
static uint16_t faderTouchThreshold = 0; // Se calculará en initHardware
static uint16_t faderTouchBaseLine = 0;  // Valor de referencia sin tocar
static unsigned long faderTouchLastReadTime = 0;
static const unsigned long FADER_TOUCH_READ_INTERVAL_MS = 20;

// ** Instancias Button2 ** (Declaradas aquí, definidas extern en Hardware.h)
Button2 buttonRec(BUTTON_PIN_REC, BUTTON_USE_INTERNAL_PULLUP);
Button2 buttonSolo(BUTTON_PIN_SOLO, BUTTON_USE_INTERNAL_PULLUP);
Button2 buttonMute(BUTTON_PIN_MUTE, BUTTON_USE_INTERNAL_PULLUP);
Button2 buttonSelect(BUTTON_PIN_SELECT, BUTTON_USE_INTERNAL_PULLUP);
Button2 buttonEncoderSelect(ENCODER_SW_PIN, BUTTON_USE_INTERNAL_PULLUP);


// ===================================
// --- CALLBACKS GLOBALES ---
// ===================================
// Guardamos las funciones que main.cpp desea que llamemos
// El array de callbacks de ButtonPress para los Button2 primarios (REC=0 a ENCODER_SELECT=4)
static ButtonPressCallback onButtonPressCallbacks[5] = {nullptr, nullptr, nullptr, nullptr, nullptr}; 

static ButtonEventCallback globalButtonEventCallback = nullptr; 
static ButtonPressCallback onFaderTouchCallback = nullptr;
static ButtonPressCallback onFaderReleaseCallback = nullptr;


// ===================================
// --- ESTRUCTURA Y MAPEO DE BOTONES ---
// Esta tabla mapea instancias de Button2 a su ButtonId y propiedades de LED.
// ===================================
struct ButtonMapping {
    Button2& button;      // Referencia a la instancia de Button2
    ButtonId id;          // ID del botón
    int neopixelIndex;    // Índice del Neopixel asociado (-1 si no tiene)
    uint8_t r, g, b;      // Color por defecto del Neopixel
};

// La tabla de mapeo de tus botones (Button2 y sus LEDs asociados)
const ButtonMapping buttonMappings[] = {
    {buttonRec,           ButtonId::REC,          NEOPIXEL_FOR_REC,          BUTTON_REC_LED_COLOR_R,    BUTTON_REC_LED_COLOR_G,    BUTTON_REC_LED_COLOR_B},
    {buttonSolo,          ButtonId::SOLO,         NEOPIXEL_FOR_SOLO,         BUTTON_SOLO_LED_COLOR_R,   BUTTON_SOLO_LED_COLOR_G,   BUTTON_SOLO_LED_COLOR_B},
    {buttonMute,          ButtonId::MUTE,         NEOPIXEL_FOR_MUTE,         BUTTON_MUTE_LED_COLOR_R,   BUTTON_MUTE_LED_COLOR_G,   BUTTON_MUTE_LED_COLOR_B},
    {buttonSelect,        ButtonId::SELECT,       NEOPIXEL_FOR_SELECT,       BUTTON_SELECT_LED_COLOR_R, BUTTON_SELECT_LED_COLOR_G, BUTTON_SELECT_LED_COLOR_B},
    {buttonEncoderSelect, ButtonId::ENCODER_SELECT, -1,                        0, 0, 0} // Sin neopixel dedicado
    // El fader touch no es un Button2, se maneja aparte.
};
// Calcula el número de elementos en buttonMappings para poder iterar sobre ellos.
const size_t NUM_BUTTON2_BUTTONS = sizeof(buttonMappings) / sizeof(buttonMappings[0]);


// ===================================
// --- PROTOTIPOS DE FUNCIONES INTERNAS (FORWARD DECLARATIONS) ---
// ===================================
// Necesarias para que las funciones puedan llamarse entre sí sin problema de orden.

// Gestiona el estado de un Neopixel asociado a un botón.
void handleButtonLedState(ButtonId id); 

// Auxiliares para Button2
static ButtonId getButtonIdFromInstance(Button2& btn); // Para obtener el ID del botón a partir de la instancia Button2.
static void handleButtonEvent(Button2& btn, bool isPressed); // Maneja la lógica común de press/release para Button2.
static void handleButtonPress(Button2& btn); // Callback específico para Button2.setPressedHandler
static void handleButtonRelease(Button2& btn); // Callback específico para Button2.setReleasedHandler


// =========================================================================
// --- FUNCIONES PÚBLICAS DEL MÓDULO HARDWARE (Implementaciones) ---
// =========================================================================

// === initHardware: Inicializa todos los componentes de hardware ===
void initHardware() {
    // --- Inicialización del Encoder Rotatorio ---
    pinMode(ENCODER_PIN_A, INPUT);
    pinMode(ENCODER_PIN_B, INPUT);
    //lastKnownEncoderState = (digitalRead(ENCODER_PIN_A) << 1) | digitalRead(ENCODER_PIN_B);
    //lastEncoderDebounceTime = millis();
    Serial.printf("Encoder: A en GPIO%d, B en GPIO%d (desde config.h)\n", ENCODER_PIN_A, ENCODER_PIN_B);
    Serial.println("Asegurate que pull-ups (4.7k), series (470R) y condensadores (470nF) estan en cada via para el encoder.");
    Serial.printf("Debounce encoder inicial en %dms. Usa '+' o '-' para ajustar.\n", ENCODER_DEBOUNCE_DELAY_MS);
    Serial.println("El contador del encoder se actualiza cada 4 sub-pasos (un 'click' del encoder).");

    // --- Inicialización de Neopixels ---
    neopixels.begin();
    // --- NUEVO: Encender LEDs 1 al 4 en Blanco ---
    // Usamos un bucle para recorrer los índices 0, 1, 2 y 3
    for(int i = 0; i < 4; i++) {
        // setPixelColor(índice, R, G, B) -> 255, 255, 255 es blanco puro
        neopixels.setPixelColor(i, neopixels.Color(255, 255, 255));
    }
    
    neopixels.show(); // ¡Importante! Envía los datos actualizados a la tira de LEDs para que se refleje el cambio.
    delay(500); // Mantén los LEDs encendidos por un momento para verificar que funcionan (500ms)
    neopixels.setBrightness(NEOPIXEL_DEFAULT_BRIGHTNESS);
    neopixels.clear(); // Apaga todos los píxeles al inicio.
    neopixels.show();  // Confirma el apagado.
    
    // --- Configuración de Botones (Button2) ---
    // Enlazar los eventos de Button2 a nuestras funciones auxiliares
    for (size_t i = 0; i < NUM_BUTTON2_BUTTONS; ++i) { // Iterar sobre la tabla ButtonMappings
        buttonMappings[i].button.setPressedHandler(handleButtonPress);
        buttonMappings[i].button.setReleasedHandler(handleButtonRelease);
    }
    
    // === VERIFICACIÓN DE PULL-UPS (Debugging) ===
    // Configura el pinMode explícitamente y lo lee.
    Serial.println("Verificando Pull-ups:");
    const struct { int pin; const char* name; } pinChecks[] = {
        {BUTTON_PIN_REC, "REC"},
        {BUTTON_PIN_SOLO, "SOLO"},
        {BUTTON_PIN_MUTE, "MUTE"},
        {BUTTON_PIN_SELECT, "SELECT"},
        {ENCODER_SW_PIN, "Encoder SW"}
    };
    for (const auto& check : pinChecks) {
        pinMode(check.pin, BUTTON_USE_INTERNAL_PULLUP ? INPUT_PULLUP : INPUT); // Button2 ya lo hace, pero es buena práctica para la verificación
        delay(5); // Un pequeño retardo para estabilizar el pin si acaba de cambiar
        Serial.printf("  Pin %s (GPIO%d): Lectura %d (Esperado HIGH si no se presiona)\n", check.name, check.pin, digitalRead(check.pin));
    }

    // --- Inicialización del LED Integrado ---
    pinMode(LED_BUILTIN_PIN, OUTPUT);
    digitalWrite(LED_BUILTIN_PIN, LOW); // Asegurarse de que esté apagado al inicio (LOW = apagado para la mayoría de los LEDs incorporados)
   

    // --- Calibración del Sensor Táctil del Fader ---
    //Serial.print("Calibrando sensor táctil del fader...");
    //delay(5); // Pequeño retardo antes de leer el sensor
    //faderTouchBaseLine = touchRead(FADER_TOUCH_PIN);
    //faderTouchThreshold = faderTouchBaseLine * FADER_TOUCH_THRESHOLD_PERCENTAGE / 100;
    //Serial.printf("BaseLine: %u, Threshold: %u\n", faderTouchBaseLine, faderTouchThreshold);
}


// === updateButtons: Actualiza el estado de todos los botones y el sensor táctil ===
void updateButtons() {
    // Actualizar todos los botones Button2
    for (size_t i = 0; i < NUM_BUTTON2_BUTTONS; ++i) {
        buttonMappings[i].button.loop(); // Llama Button2::loop() para cada botón
    }
    
    // Lectura y gestión del sensor táctil del fader
    unsigned long currentTime = millis();
    if (currentTime - faderTouchLastReadTime >= FADER_TOUCH_READ_INTERVAL_MS) {
        uint16_t touchValue = touchRead(FADER_TOUCH_PIN);
        // Serial.printf("TouchRead: %u\n", touchValue); // Descomentar para depuración
        
        bool currentlyTouched = (touchValue < faderTouchThreshold);

        if (currentlyTouched != isFaderTouched) {
            isFaderTouched = currentlyTouched;
            // Disparar el control del LED asociado al fader
            handleButtonLedState(ButtonId::FADER_TOUCH);

            // Disparar callbacks específicos del fader
            if (isFaderTouched) {
                if (onFaderTouchCallback != nullptr) onFaderTouchCallback();
            } else {
                if (onFaderReleaseCallback != nullptr) onFaderReleaseCallback();
            }
            // Disparar callback global (si el ButtonId::FADER_TOUCH está mapeado)
            if (globalButtonEventCallback != nullptr) {
                globalButtonEventCallback(ButtonId::FADER_TOUCH);
            }
        }
        faderTouchLastReadTime = currentTime;
    }
}

// === setNeopixelState: Configura el color de un Neopixel individual ===
void setNeopixelState(int neopixelIndex, uint8_t r, uint8_t g, uint8_t b) {
    if (neopixelIndex >= 0 && neopixelIndex < NEOPIXEL_COUNT) {
       neopixels.setPixelColor(neopixelIndex, neopixels.Color(r, g, b)); // Usa neopixels.Color() para claridad
    }
}

// === showNeopixels: Envía los colores configurados a los LEDs físicos ===
void showNeopixels() {
    neopixels.show();
}

// === setNeopixelGlobalBrightness: Ajusta el brillo global de los Neopixels ===
void setNeopixelGlobalBrightness(uint8_t brightness) {
    neopixels.setBrightness(brightness);
    neopixels.show(); // Aplicar el brillo inmediatamente
}

// === Callbacks: Funciones para registrar y disparar callbacks ===
void registerButtonPressCallback(ButtonId id, ButtonPressCallback callback) {
    // Los Button2 primarios son los primeros N ButtonId (0 a ENCODER_SELECT)
    if (static_cast<int>(id) >= 0 && static_cast<int>(id) <= static_cast<int>(ButtonId::ENCODER_SELECT)) { 
         // onButtonPressCallbacks tiene un tamaño de 5 para los 5 botones Button2
         onButtonPressCallbacks[static_cast<int>(id)] = callback;
    }
}
void registerButtonEventCallback(ButtonEventCallback callback) { globalButtonEventCallback = callback; }
void registerFaderTouchCallback(ButtonPressCallback callback) { onFaderTouchCallback = callback; }
void registerFaderReleaseCallback(ButtonPressCallback callback) { onFaderReleaseCallback = callback; }


// ===================================
// --- FUNCIONES INTERNAS (Implementaciones) ---
// ===================================

// === getButtonIdFromInstance: Obtiene el ButtonId de una instancia Button2 ===
static ButtonId getButtonIdFromInstance(Button2& btn) {
    for (size_t i = 0; i < NUM_BUTTON2_BUTTONS; ++i) {
        if (&buttonMappings[i].button == &btn) { // Compara la dirección de memoria de la instancia
            return buttonMappings[i].id;
        }
    }
    return ButtonId::UNKNOWN; // Si no se encuentra
}

// === handleButtonEvent: Manejador unificado para los eventos de Button2 ===
static void handleButtonEvent(Button2& btn, bool isPressed) {
    ButtonId id = getButtonIdFromInstance(btn);
    if (id == ButtonId::UNKNOWN) return; // Si no conocemos el ID, ignorar

    handleButtonLedState(id); // Gestionar el LED asociado al botón

    if (isPressed) { // Si se acaba de presionar
        // Llamar al callback específico de pulsación si está registrado
        // Se comprueba el rango para evitar acceder fuera de onButtonPressCallbacks
        if (static_cast<int>(id) >= 0 && static_cast<int>(id) <= static_cast<int>(ButtonId::ENCODER_SELECT) && onButtonPressCallbacks[static_cast<int>(id)] != nullptr) {
            onButtonPressCallbacks[static_cast<int>(id)]();
        }
        // Llamar al callback global
        if (globalButtonEventCallback != nullptr) {
            globalButtonEventCallback(id);
        }
    } else { // Si se acaba de soltar
        // Por ahora, no hay callbacks específicos para "soltar" para los Button2 primarios.
        // Si se necesitan, se deberían añadir.
    }
}

// === handleButtonPress: Callback para Button2::setPressedHandler ===
static void handleButtonPress(Button2& btn) {
    handleButtonEvent(btn, true);
}

// === handleButtonRelease: Callback para Button2::setReleasedHandler ===
static void handleButtonEvent_release(Button2& btn, bool isPressed) {
    ButtonId id = getButtonIdFromInstance(btn);
    if (id == ButtonId::UNKNOWN) return; // Si no conocemos el ID, ignorar

    handleButtonLedState(id); // Gestionar el LED

    if (isPressed) { // Si se acaba de presionar
        // Llamar al callback específico de pulsación si está registrado
        if (static_cast<int>(id) >= 0 && static_cast<int>(id) <= static_cast<int>(ButtonId::ENCODER_SELECT) && onButtonPressCallbacks[static_cast<int>(id)] != nullptr) {
            onButtonPressCallbacks[static_cast<int>(id)]();
        }
        // Llamar al callback global
        if (globalButtonEventCallback != nullptr) {
            globalButtonEventCallback(id);
        }
    } else { // Si se acaba de soltar
        // Por ahora, no hay callbacks específicos para "soltar" para los Button2 primarios.
        // Si se necesitan, se deberían añadir.
    }
}
static void handleButtonRelease(Button2& btn) {
    handleButtonEvent(btn, false);
}


// =========================================================================
//  IMPLEMENTACIÓN DE handleButtonLedState - ¡AQUÍ ESTÁ EL CAMBIO PRINCIPAL!
//  Esta función AHORA NO TOMA `turnOn` como parámetro.
//  Decide el estado del LED leyendo directamente `recStates`, `soloStates`, etc.
// =========================================================================
void handleButtonLedState(ButtonId id) { // <<<< ELIMINADO EL PARÁMETRO `bool turnOn`
    // Obtener el estado lógico actual que el LED debe reflejar
    bool shouldBeOn = false;
    uint8_t r=0, g=0, b=0;
    int neopixelIndex = -1;

    switch (id) {
        case ButtonId::REC:
            shouldBeOn = recStates; // <<<< LEYENDO EL ESTADO LÓGICO GLOBAL (toggle)
            neopixelIndex = NEOPIXEL_FOR_REC;
            r = BUTTON_REC_LED_COLOR_R; g = BUTTON_REC_LED_COLOR_G; b = BUTTON_REC_LED_COLOR_B;
            break;
        case ButtonId::SOLO:
            shouldBeOn = soloStates; // <<<< LEYENDO EL ESTADO LÓGICO GLOBAL (toggle)
            neopixelIndex = NEOPIXEL_FOR_SOLO;
            r = BUTTON_SOLO_LED_COLOR_R; g = BUTTON_SOLO_LED_COLOR_G; b = BUTTON_SOLO_LED_COLOR_B;
            break;
        case ButtonId::MUTE:
            shouldBeOn = muteStates; // <<<< LEYENDO EL ESTADO LÓGICO GLOBAL (toggle)
            neopixelIndex = NEOPIXEL_FOR_MUTE;
            r = BUTTON_MUTE_LED_COLOR_R; g = BUTTON_MUTE_LED_COLOR_G; b = BUTTON_MUTE_LED_COLOR_B;
            break;
        case ButtonId::SELECT:
            shouldBeOn = selectStates; // <<<< LEYENDO EL ESTADO LÓGICO GLOBAL (toggle)
            neopixelIndex = NEOPIXEL_FOR_SELECT;
            // Para SELECT, el color se define como blanco al 15%
            uint8_t scaled_brightness_for_select_pixel = (uint16_t)255 * NEOPIXEL_COLOR_3_BRIGHTNESS_FACTOR / 50;
            r = g = b = scaled_brightness_for_select_pixel;
            break;
        
    }

    if (neopixelIndex != -1) { // Si el botón tiene un Neopixel asociado
        if (shouldBeOn) {
            setNeopixelState(neopixelIndex, r, g, b); // Encender con su color
        } else {
            setNeopixelState(neopixelIndex, 0, 0, 0); // Apagar
        }
        showNeopixels();
    }
}

// =========================================================================
//  IMPLEMENTACIÓN DE FUNCIONES PARA LED INTEGRADO
// =========================================================================

void setLedBuiltin(bool state) {
    // Para la mayoría de los ESP32, LOW = LED OFF, HIGH = LED ON (pero puede variar)
    digitalWrite(LED_BUILTIN_PIN, state ? HIGH : LOW);
}

void toggleLedBuiltin() {
    digitalWrite(LED_BUILTIN_PIN, !digitalRead(LED_BUILTIN_PIN));
}