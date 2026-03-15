// src/hardware/Neopixels/Neopixel.cpp
#include "Neopixel.h"

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

// ─────────────────────────────────────────────
//  initNeopixels
//  Llamar SIEMPRE después de initDisplay()
// ─────────────────────────────────────────────
void initNeopixels() {
    neopixels.Begin();

    // Test visual: todos blanco → apagar
    neopixels.ClearTo(RgbColor(255, 255, 255));
    neopixels.Show();
    delay(400);

    neopixels.ClearTo(RgbColor(0));
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