// src/hardware/Hardware.cpp
#include "Hardware.h"
#include "../config.h"
#include "Neopixels/Neopixel.h"

// Hardware.cpp no gestiona estados de botones ni LEDs.
// Solo detecta pulsaciones y notifica via callbacks.
// RS485Handler es la fuente de verdad para recStates/soloStates/etc.

// ** Sensor Táctil del Fader Variables **
volatile bool     isFaderTouched            = false;
static uint32_t   faderTouchThreshold       = 0;
static uint32_t   faderTouchBaseLine        = 0;
static unsigned long faderTouchLastReadTime = 0;
static const unsigned long FADER_TOUCH_READ_INTERVAL_MS = 20;

// ** Instancias Button2 **
Button2 buttonRec(BUTTON_PIN_REC, BUTTON_USE_INTERNAL_PULLUP);
Button2 buttonSolo(BUTTON_PIN_SOLO, BUTTON_USE_INTERNAL_PULLUP);
Button2 buttonMute(BUTTON_PIN_MUTE, BUTTON_USE_INTERNAL_PULLUP);
Button2 buttonSelect(BUTTON_PIN_SELECT, BUTTON_USE_INTERNAL_PULLUP);
Button2 buttonEncoderSelect(ENCODER_SW_PIN, BUTTON_USE_INTERNAL_PULLUP);

// ===================================
// --- CALLBACKS GLOBALES ---
// ===================================
static ButtonEventCallback globalButtonEventCallback  = nullptr;
static ButtonPressCallback onFaderTouchCallback       = nullptr;
static ButtonPressCallback onFaderReleaseCallback     = nullptr;

// ===================================
// --- MAPEO DE BOTONES ---
// ===================================
struct ButtonMapping {
    Button2& button;
    ButtonId id;
};

const ButtonMapping buttonMappings[] = {
    {buttonRec,           ButtonId::REC           },
    {buttonSolo,          ButtonId::SOLO          },
    {buttonMute,          ButtonId::MUTE          },
    {buttonSelect,        ButtonId::SELECT        },
    {buttonEncoderSelect, ButtonId::ENCODER_SELECT},
};
const size_t NUM_BUTTON2_BUTTONS = sizeof(buttonMappings) / sizeof(buttonMappings[0]);

// ===================================
// --- FORWARD DECLARATIONS ---
// ===================================
static ButtonId getButtonIdFromInstance(Button2& btn);
static void handleButtonPress(Button2& btn);
static void handleButtonRelease(Button2& btn);

// =========================================================================
// --- FUNCIONES PÚBLICAS ---
// =========================================================================
void initHardware() {
    pinMode(ENCODER_PIN_A, INPUT);
    pinMode(ENCODER_PIN_B, INPUT);

    for (size_t i = 0; i < NUM_BUTTON2_BUTTONS; ++i) {
        buttonMappings[i].button.setPressedHandler(handleButtonPress);
        buttonMappings[i].button.setReleasedHandler(handleButtonRelease);
    }

    pinMode(LED_BUILTIN_PIN, OUTPUT);
    digitalWrite(LED_BUILTIN_PIN, LOW);

    // Calibración táctil fader
    uint32_t sum = 0;
    for (int i = 0; i < 20; i++) {
        sum += touchRead(FADER_TOUCH_PIN);
        delay(10);
    }
    faderTouchBaseLine  = sum / 20;
    faderTouchThreshold = faderTouchBaseLine * FADER_TOUCH_THRESHOLD_PERCENTAGE / 100;
}

// ===================================
// --- updateButtons ---
// ===================================
void updateButtons() {
    for (size_t i = 0; i < NUM_BUTTON2_BUTTONS; ++i) {
        buttonMappings[i].button.loop();
    }

    unsigned long currentTime = millis();
    if (currentTime - faderTouchLastReadTime >= FADER_TOUCH_READ_INTERVAL_MS) {
        uint32_t touchValue   = touchRead(FADER_TOUCH_PIN);
        bool currentlyTouched = (touchValue < faderTouchThreshold);

        if (currentlyTouched != isFaderTouched) {
            isFaderTouched = currentlyTouched;
            if (isFaderTouched) {
                if (onFaderTouchCallback)   onFaderTouchCallback();
            } else {
                if (onFaderReleaseCallback) onFaderReleaseCallback();
            }
            if (globalButtonEventCallback) globalButtonEventCallback(ButtonId::FADER_TOUCH);
        }
        faderTouchLastReadTime = currentTime;
    }
}

// ===================================
// --- Callbacks ---
// ===================================
void registerButtonEventCallback(ButtonEventCallback callback)  { globalButtonEventCallback = callback; }
void registerFaderTouchCallback(ButtonPressCallback callback)   { onFaderTouchCallback      = callback; }
void registerFaderReleaseCallback(ButtonPressCallback callback) { onFaderReleaseCallback    = callback; }

// ===================================
// --- Funciones internas ---
// ===================================
static ButtonId getButtonIdFromInstance(Button2& btn) {
    for (size_t i = 0; i < NUM_BUTTON2_BUTTONS; ++i)
        if (&buttonMappings[i].button == &btn) return buttonMappings[i].id;
    return ButtonId::UNKNOWN;
}

static void handleButtonPress(Button2& btn) {
    ButtonId id = getButtonIdFromInstance(btn);
    // Solo notificar — Logic decide el estado via RS485
    if (globalButtonEventCallback) globalButtonEventCallback(id);
}

static void handleButtonRelease(Button2& btn) {
    // Release no se notifica — RS485 gestiona los flags
    (void)btn;
}

// ===================================
// --- LED integrado ---
// ===================================
void setLedBuiltin(bool state)  { digitalWrite(LED_BUILTIN_PIN, state ? HIGH : LOW); }
void toggleLedBuiltin()         { digitalWrite(LED_BUILTIN_PIN, !digitalRead(LED_BUILTIN_PIN)); }