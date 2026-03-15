// src/hardware/Hardware.cpp
#include "Hardware.h"
#include "../config.h"
#include "Neopixels/Neopixel.h"   // ← NeoPixelBus, sustituye Adafruit_NeoPixel.h

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
static ButtonPressCallback onButtonPressCallbacks[5] = {nullptr, nullptr, nullptr, nullptr, nullptr}; 
static ButtonEventCallback globalButtonEventCallback  = nullptr; 
static ButtonPressCallback onFaderTouchCallback       = nullptr;
static ButtonPressCallback onFaderReleaseCallback     = nullptr;

// ===================================
// --- ESTRUCTURA Y MAPEO DE BOTONES ---
// ===================================
struct ButtonMapping {
    Button2& button;
    ButtonId id;
    int      neopixelIndex;
    uint8_t  r, g, b;
};

const ButtonMapping buttonMappings[] = {
    {buttonRec,           ButtonId::REC,            NEOPIXEL_FOR_REC,    BUTTON_REC_LED_COLOR_R,    BUTTON_REC_LED_COLOR_G,    BUTTON_REC_LED_COLOR_B},
    {buttonSolo,          ButtonId::SOLO,            NEOPIXEL_FOR_SOLO,   BUTTON_SOLO_LED_COLOR_R,   BUTTON_SOLO_LED_COLOR_G,   BUTTON_SOLO_LED_COLOR_B},
    {buttonMute,          ButtonId::MUTE,            NEOPIXEL_FOR_MUTE,   BUTTON_MUTE_LED_COLOR_R,   BUTTON_MUTE_LED_COLOR_G,   BUTTON_MUTE_LED_COLOR_B},
    {buttonSelect,        ButtonId::SELECT,          NEOPIXEL_FOR_SELECT, BUTTON_SELECT_LED_COLOR_R, BUTTON_SELECT_LED_COLOR_G, BUTTON_SELECT_LED_COLOR_B},
    {buttonEncoderSelect, ButtonId::ENCODER_SELECT,  -1,                  0, 0, 0}
};
const size_t NUM_BUTTON2_BUTTONS = sizeof(buttonMappings) / sizeof(buttonMappings[0]);

// ===================================
// --- FORWARD DECLARATIONS ---
// ===================================
void handleButtonLedState(ButtonId id);
static ButtonId getButtonIdFromInstance(Button2& btn);
static void handleButtonEvent(Button2& btn, bool isPressed);
static void handleButtonPress(Button2& btn);
static void handleButtonRelease(Button2& btn);

// =========================================================================
// --- FUNCIONES PÚBLICAS ---
// =========================================================================

void initHardware() {
    // --- Encoder ---
    pinMode(ENCODER_PIN_A, INPUT);
    pinMode(ENCODER_PIN_B, INPUT);

    // --- NeoPixels ---
    // initNeopixels() se llama desde main.cpp DESPUÉS de initDisplay()
    // NO llamar aquí para evitar conflicto RMT/I2S con LovyanGFX

    // --- Botones (Button2) ---
    for (size_t i = 0; i < NUM_BUTTON2_BUTTONS; ++i) {
        buttonMappings[i].button.setPressedHandler(handleButtonPress);
        buttonMappings[i].button.setReleasedHandler(handleButtonRelease);
    }

    // --- LED integrado ---
    pinMode(LED_BUILTIN_PIN, OUTPUT);
    digitalWrite(LED_BUILTIN_PIN, LOW);

    // --- Calibración táctil fader ---
    uint32_t sum = 0;
    for (int i = 0; i < 20; i++) {
        sum += touchRead(FADER_TOUCH_PIN);
        delay(10);
    }
    faderTouchBaseLine  = sum / 20;
    faderTouchThreshold = faderTouchBaseLine * FADER_TOUCH_THRESHOLD_PERCENTAGE / 100;
}

bool readFaderTouch() {
    return false;  // AT42QT1011 eliminado en rev2
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
        uint32_t touchValue    = touchRead(FADER_TOUCH_PIN);
        bool currentlyTouched  = (touchValue < faderTouchThreshold);

        if (currentlyTouched != isFaderTouched) {
            isFaderTouched = currentlyTouched;
            handleButtonLedState(ButtonId::FADER_TOUCH);

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
// --- handleButtonLedState ---
// ===================================
void handleButtonLedState(ButtonId id) {
    bool    shouldBeOn    = false;
    uint8_t r=0, g=0, b=0;
    int     neopixelIndex = -1;

    switch (id) {
        case ButtonId::REC:
            shouldBeOn    = recStates;
            neopixelIndex = NEOPIXEL_FOR_REC;
            r = BUTTON_REC_LED_COLOR_R;
            g = BUTTON_REC_LED_COLOR_G;
            b = BUTTON_REC_LED_COLOR_B;
            break;
        case ButtonId::SOLO:
            shouldBeOn    = soloStates;
            neopixelIndex = NEOPIXEL_FOR_SOLO;
            r = BUTTON_SOLO_LED_COLOR_R;
            g = BUTTON_SOLO_LED_COLOR_G;
            b = BUTTON_SOLO_LED_COLOR_B;
            break;
        case ButtonId::MUTE:
            shouldBeOn    = muteStates;
            neopixelIndex = NEOPIXEL_FOR_MUTE;
            r = BUTTON_MUTE_LED_COLOR_R;
            g = BUTTON_MUTE_LED_COLOR_G;
            b = BUTTON_MUTE_LED_COLOR_B;
            break;
        case ButtonId::SELECT: {
            shouldBeOn    = selectStates;
            neopixelIndex = NEOPIXEL_FOR_SELECT;
            r = BUTTON_SELECT_LED_COLOR_R;
            g = BUTTON_SELECT_LED_COLOR_G;
            b = BUTTON_SELECT_LED_COLOR_B;
            break;
        }
        default: return;
    }

    if (neopixelIndex == -1) return;

    uint8_t fr, fg, fb;
    // Escala plena = NEOPIXEL_DEFAULT_BRIGHTNESS
    // Escala tenue = NEOPIXEL_DIM_BRIGHTNESS
    // Los colores R/G/B son solo el tono — el brillo lo mandan los defines

    if (id == ButtonId::SELECT) {
        uint8_t s = shouldBeOn ? NEOPIXEL_DEFAULT_BRIGHTNESS : 0;
        fr = (r * s) / 255;
        fg = (g * s) / 255;
        fb = (b * s) / 255;
    } else {
        // REC, SOLO, MUTE
        if (selectStates) {
            // Canal seleccionado: ON=pleno, OFF=apagado
            uint8_t s = shouldBeOn ? NEOPIXEL_DEFAULT_BRIGHTNESS : 0;
            fr = (r * s) / 255;
            fg = (g * s) / 255;
            fb = (b * s) / 255;
        } else {
            // Canal no seleccionado: ON=atenuado, OFF=apagado
            uint8_t s = shouldBeOn ? NEOPIXEL_DIM_BRIGHTNESS : 0;
            fr = (r * s) / 255;
            fg = (g * s) / 255;
            fb = (b * s) / 255;
        }
    }

    setNeopixelState(neopixelIndex, fr, fg, fb);
}

void updateAllNeopixels() {
    static bool lastRec    = false;
    static bool lastSolo   = false;
    static bool lastMute   = false;
    static bool lastSelect = false;

    if (recStates    == lastRec    &&
        soloStates   == lastSolo   &&
        muteStates   == lastMute   &&
        selectStates == lastSelect) return;

    lastRec    = recStates;
    lastSolo   = soloStates;
    lastMute   = muteStates;
    lastSelect = selectStates;

    handleButtonLedState(ButtonId::REC);
    handleButtonLedState(ButtonId::SOLO);
    handleButtonLedState(ButtonId::MUTE);
    handleButtonLedState(ButtonId::SELECT);
    showNeopixels();   // único Show() del ciclo
}

// ===================================
// --- Callbacks ---
// ===================================
void registerButtonPressCallback(ButtonId id, ButtonPressCallback callback) {
    if (static_cast<int>(id) >= 0 && static_cast<int>(id) <= static_cast<int>(ButtonId::ENCODER_SELECT))
        onButtonPressCallbacks[static_cast<int>(id)] = callback;
}
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

static void handleButtonEvent(Button2& btn, bool isPressed) {
    ButtonId id = getButtonIdFromInstance(btn);
    
    if (isPressed) {
        switch (id) {
            case ButtonId::REC:    recStates    = !recStates;    break;
            case ButtonId::SOLO:   soloStates   = !soloStates;   break;
            case ButtonId::MUTE:   muteStates   = !muteStates;   break;
            case ButtonId::SELECT: selectStates = !selectStates; break;
            default: break;
        }
    }
    handleButtonLedState(id);
    if (globalButtonEventCallback) globalButtonEventCallback(id);
}

static void handleButtonPress(Button2& btn)   { handleButtonEvent(btn, true);  }
static void handleButtonRelease(Button2& btn) { handleButtonEvent(btn, false); }

// ===================================
// --- LED integrado ---
// ===================================
void setLedBuiltin(bool state)  { digitalWrite(LED_BUILTIN_PIN, state ? HIGH : LOW); }
void toggleLedBuiltin()         { digitalWrite(LED_BUILTIN_PIN, !digitalRead(LED_BUILTIN_PIN)); }