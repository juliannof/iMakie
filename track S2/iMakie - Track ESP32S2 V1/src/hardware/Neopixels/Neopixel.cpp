// src/hardware/Neopixels/Neopixel.cpp
#include "Neopixel.h"
#include "../../config.h"
#include "../Hardware.h"   // ButtonId

// ─────────────────────────────────────────────
//  Instancia global
//  NeoEsp32I2s0800KbpsMethod: usa periférico I2S,
//  NO usa RMT → sin conflicto con LovyanGFX
// ─────────────────────────────────────────────
NeoStrip neopixels(NEOPIXEL_COUNT, NEOPIXEL_PIN);

// ─────────────────────────────────────────────
//  Estado interno
// ─────────────────────────────────────────────
static uint8_t  neoBrightness  = NEOPIXEL_DEFAULT_BRIGHTNESS;
static bool     neoNeedsUpdate = false;
bool neoWaitingHandshake = true;  // ← definición global

// ─────────────────────────────────────────────
//  initNeopixels
//  Llamar SIEMPRE después de initDisplay()
// ─────────────────────────────────────────────
void initNeopixels() {
    neopixels.Begin();

    // Test visual: todos blanco → apagar
    neopixels.ClearTo(RgbColor(0, 0, NEOPIXEL_DIM_BRIGHTNESS));  // azul tenue — esperando handshake
    neopixels.Show();

    neoNeedsUpdate = false;

    log_i("[NEO] NeoPixelBus I2S OK — %d pixels GPIO%d  brillo=%d",
          NEOPIXEL_COUNT, NEOPIXEL_PIN, neoBrightness);
}

// ─────────────────────────────────────────────
//  Brillo global
// ─────────────────────────────────────────────
void setNeopixelGlobalBrightness(uint8_t brightness) {
    neoBrightness = brightness;
}

uint8_t getNeopixelBrightness() {
    return neoBrightness;
}

// ─────────────────────────────────────────────
//  setNeopixelState
//  Aplica escala de brillo y marca dirty.
//  NO llama Show() — el loop lo hace una vez.
// ─────────────────────────────────────────────
void setNeopixelState(int idx, uint8_t r, uint8_t g, uint8_t b) {
    if (idx < 0 || idx >= NEOPIXEL_COUNT) return;

    // Sin escalar — el brillo se controla desde los valores que se pasan
    RgbColor color(r, g, b);

    if (neopixels.GetPixelColor(idx) != color) {
        neopixels.SetPixelColor(idx, color);
        neoNeedsUpdate = true;
    }
}

// ─────────────────────────────────────────────
//  clearNeopixel / clearAllNeopixels
// ─────────────────────────────────────────────
void clearNeopixel(int idx) {
    setNeopixelState(idx, 0, 0, 0);
}

void clearAllNeopixels() {
    for (int i = 0; i < NEOPIXEL_COUNT; i++) {
        neopixels.SetPixelColor(i, RgbColor(0));
    }
    neoNeedsUpdate = true;
}

// ─────────────────────────────────────────────
//  showNeopixels
//  Llamar UNA VEZ por ciclo en updateHardware()
// ─────────────────────────────────────────────
void showNeopixels() {
    if (!neoNeedsUpdate) return;
    neopixels.Show();
    neoNeedsUpdate = false;
}


void updateAllNeopixels() {
    if (neoWaitingHandshake) return;
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
