// src/hardware/Neopixels/Neopixel.cpp
#include "Neopixel.h"
#include "../../config.h"
#include "../Hardware.h"

Adafruit_NeoPixel neopixels(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

static uint8_t  neoBrightness  = NEOPIXEL_DEFAULT_BRIGHTNESS;
bool neoWaitingHandshake = true;

void initNeopixels() {
    neopixels.begin();
    neopixels.clear();
    // Todos en azul tenue (esperando handshake)
    for (int i = 0; i < NEOPIXEL_COUNT; i++) {
        neopixels.setPixelColor(i, 0, 0, NEOPIXEL_DIM_BRIGHTNESS);
    }
    neopixels.show();
    neoWaitingHandshake = true;
    log_i("[NEO] Adafruit NeoPixel OK — %d pixels GPIO%d", NEOPIXEL_COUNT, NEOPIXEL_PIN);
}

void setNeopixelGlobalBrightness(uint8_t brightness) {
    neoBrightness = brightness;
}

uint8_t getNeopixelBrightness() {
    return neoBrightness;
}

void setNeopixelState(int idx, uint8_t r, uint8_t g, uint8_t b) {
    if (idx < 0 || idx >= NEOPIXEL_COUNT) return;
    neopixels.setPixelColor(idx, r, g, b);
}

void clearNeopixel(int idx) {
    setNeopixelState(idx, 0, 0, 0);
}

void clearAllNeopixels() {
    neopixels.clear();
}

void showNeopixels() {
    neopixels.show();
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
    showNeopixels();
}

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
    if (id == ButtonId::SELECT) {
        uint8_t s = shouldBeOn ? NEOPIXEL_DEFAULT_BRIGHTNESS : 0;
        fr = (r * s) / 255;
        fg = (g * s) / 255;
        fb = (b * s) / 255;
    } else {
        if (selectStates) {
            uint8_t s = shouldBeOn ? NEOPIXEL_DEFAULT_BRIGHTNESS : 0;
            fr = (r * s) / 255;
            fg = (g * s) / 255;
            fb = (b * s) / 255;
        } else {
            uint8_t s = shouldBeOn ? NEOPIXEL_DIM_BRIGHTNESS : 0;
            fr = (r * s) / 255;
            fg = (g * s) / 255;
            fb = (b * s) / 255;
        }
    }

    setNeopixelState(neopixelIndex, fr, fg, fb);
}
