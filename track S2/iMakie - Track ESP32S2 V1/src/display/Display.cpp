// src/display/Display.cpp
#include <Arduino.h>
#include "Display.h"
#include "hardware/encoder/Encoder.h"
#include "../hardware/Hardware.h"
#include "../config.h"

extern LGFX        tft;
extern LGFX_Sprite header;
extern LGFX_Sprite mainArea;
extern LGFX_Sprite vuSprite;
extern LGFX_Sprite vPotSprite;

namespace {
    uint8_t screenBrightness = 255;
}

void drawMainArea();
void drawVUMeters();
void drawHeaderSprite();
void drawOfflineScreen();
void drawInitializingScreen();
void drawVPotDisplay();
void drawButton(LGFX_Sprite &sprite, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char* label, bool active, uint16_t activeColor);
void drawMeter(LGFX_Sprite &sprite, uint16_t x, uint16_t y, uint16_t w, uint16_t h, float level, float peakLevel, bool isClipping);

bool needsTOTALRedraw    = true;
bool needsMainAreaRedraw = false;
bool needsHeaderRedraw   = false;
bool needsVUMetersRedraw = false;
bool needsVPotRedraw     = false;
volatile ConnectionState logicConnectionState = ConnectionState::DISCONNECTED;

static int8_t currentVPotLevel = VPOT_DEFAULT_LEVEL;
AutoMode currentAutoMode = AUTO_OFF;

// ════════════════════════════════════════════════════════════
//  initDisplay
// ════════════════════════════════════════════════════════════
void initDisplay() {
    esp_reset_reason_t reason = esp_reset_reason();
    Serial.printf("Reset reason: %d\n", (int)reason);

    
    tft.init();
    tft.setRotation(0);
    tft.setBrightness(screenBrightness);
    tft.fillScreen(TFT_BG_COLOR);

    mainArea.setColorDepth(16);
    mainArea.createSprite(MAINAREA_WIDTH, MAINAREA_HEIGHT);

    header.setColorDepth(16);
    header.createSprite(TFT_WIDTH, HEADER_HEIGHT);

    vuSprite.setColorDepth(16);
    vuSprite.createSprite(TFT_WIDTH - MAINAREA_WIDTH, MAINAREA_HEIGHT);

    vPotSprite.setColorDepth(16);
    vPotSprite.createSprite(TFT_WIDTH, VPOT_HEIGHT);

    Serial.printf("tft       : %d x %d\n", tft.width(), tft.height());
    Serial.printf("header    : %d x %d  → pushSprite(0, 0)\n",   header.width(),   header.height());
    Serial.printf("mainArea  : %d x %d  → pushSprite(0, %d)\n",  mainArea.width(),  mainArea.height(), HEADER_HEIGHT);
    Serial.printf("vuSprite  : %d x %d  → pushSprite(%d, %d)\n", vuSprite.width(),  vuSprite.height(), MAINAREA_WIDTH, HEADER_HEIGHT);
    Serial.printf("vPotSprite: %d x %d  → pushSprite(0, %d)\n",  vPotSprite.width(),vPotSprite.height(), MAINAREA_HEIGHT + HEADER_HEIGHT);
    Serial.printf("Layout total: %d px alto (pantalla: %d)\n", HEADER_HEIGHT + MAINAREA_HEIGHT + VPOT_HEIGHT, tft.height());

    setVPotLevel(VPOT_DEFAULT_LEVEL);

    Serial.printf("header     creado: %s\n", header.width()    > 0 ? "OK" : "FALLO");
    Serial.printf("mainArea   creado: %s\n", mainArea.width()  > 0 ? "OK" : "FALLO");
    Serial.printf("vuSprite   creado: %s\n", vuSprite.width()  > 0 ? "OK" : "FALLO");
    Serial.printf("vPotSprite creado: %s\n", vPotSprite.width()> 0 ? "OK" : "FALLO");
    Serial.printf("Heap libre:  %u bytes\n", ESP.getFreeHeap());
    Serial.printf("PSRAM libre: %u bytes\n", ESP.getFreePsram());
    Serial.println("[SETUP] Display iniciado - LovyanGFX");

    needsTOTALRedraw = true;
}

// ════════════════════════════════════════════════════════════
//  setScreenBrightness / setVPotLevel
// ════════════════════════════════════════════════════════════
void setScreenBrightness(uint8_t brightness) {
    screenBrightness = brightness;
    tft.setBrightness(brightness);
}



// ════════════════════════════════════════════════════════════
//  updateDisplay  —  redraw total + redraws incrementales
// ════════════════════════════════════════════════════════════
void updateDisplay() {
    // Sin conexión RS485: pantalla offline
    if (logicConnectionState != ConnectionState::CONNECTED) {
        static ConnectionState lastState = ConnectionState::CONNECTED;
        if (logicConnectionState != lastState) {
            tft.fillScreen(TFT_BG_COLOR);
            drawOfflineScreen();
            lastState = logicConnectionState;
        }
        return;
    }

    // Primera vez o redraw forzado
    if (needsTOTALRedraw) {
        tft.fillScreen(TFT_BG_COLOR);
        drawHeaderSprite();
        drawMainArea();
        drawVUMeters();
        drawVPotDisplay();
        needsTOTALRedraw    = false;
        needsMainAreaRedraw = false;
        needsHeaderRedraw   = false;
        needsVUMetersRedraw = false;
        needsVPotRedraw     = false;
        Serial.println("[Display] Redraw total OK");
        return;
    }

    // Redraws incrementales: solo lo que cambió
    if (needsHeaderRedraw) {
        drawHeaderSprite();
        needsHeaderRedraw = false;
    }
    if (needsMainAreaRedraw) {
        drawMainArea();
        needsMainAreaRedraw = false;
    }
    if (needsVUMetersRedraw) {
        drawVUMeters();
        needsVUMetersRedraw = false;
    }
    if (needsVPotRedraw) {
        drawVPotDisplay();
        needsVPotRedraw = false;
    }
}

// ════════════════════════════════════════════════════════════
//  Pantallas de estado
// ════════════════════════════════════════════════════════════
void drawOfflineScreen() {
    tft.setBrightness(0);
}

void drawInitializingScreen() {
    tft.fillScreen(TFT_BLUE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString("MIDI Handshake OK!", tft.width() / 2, tft.height() / 2 - 30);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("Esperando DAW...",   tft.width() / 2, tft.height() / 2 + 10);
}

// ════════════════════════════════════════════════════════════
//  drawHeaderSprite
// ════════════════════════════════════════════════════════════
void drawHeaderSprite() {
    static const uint16_t AUTO_COLORS[] = {
        TFT_AUTO_OFF,    // AUTO_OFF
        TFT_AUTO_READ,   // AUTO_READ
        TFT_AUTO_WRITE,  // AUTO_WRITE
        TFT_AUTO_OFF,    // AUTO_TRIM — reservado
        TFT_AUTO_TOUCH,  // AUTO_TOUCH
        TFT_AUTO_LATCH,  // AUTO_LATCH
    };

    uint16_t headerColor = selectStates
        ? AUTO_COLORS[currentAutoMode]
        : TFT_MCU_DARKGRAY;

    header.fillSprite(headerColor);

    if (!selectStates) {
        int rectX = (TFT_WIDTH - (TFT_WIDTH - 60)) / 2;
        int rectY = (HEADER_HEIGHT - 10)            / 2;
        header.fillRoundRect(rectX, rectY, TFT_WIDTH - 60, 10, 3, TFT_MCU_GRAY);
    }

    screenBrightness = selectStates ? 255 : 70;
    setScreenBrightness(screenBrightness);
    header.pushSprite(0, 0);
}

// ════════════════════════════════════════════════════════════
//  drawButton
// ════════════════════════════════════════════════════════════
void drawButton(LGFX_Sprite &sprite, uint16_t x, uint16_t y,
                uint16_t w, uint16_t h,
                const char* label, bool active, uint16_t activeColor) {
    uint16_t btnColor  = active ? activeColor : TFT_MCU_DARKGRAY;
    uint16_t textColor = active ? TFT_BLACK   : TFT_WHITE;
    sprite.fillRoundRect(x, y, w, h, 5, btnColor);
    sprite.setTextFont(2);
    sprite.setTextSize(1);
    sprite.setTextColor(textColor, btnColor);
    sprite.setTextDatum(MC_DATUM);
    sprite.drawString(label, x + w / 2, y + h / 2);
}

// ════════════════════════════════════════════════════════════
//  drawMainArea
// ════════════════════════════════════════════════════════════
void drawMainArea() {
    mainArea.fillSprite(TFT_BG_COLOR);

    drawButton(mainArea, 10,                        7, BUTTON_WIDTH, BUTTON_HEIGHT, "REC",  recStates,  TFT_REC_COLOR);
    drawButton(mainArea, 10 + BUTTON_WIDTH + 2,     7, BUTTON_WIDTH, BUTTON_HEIGHT, "SOLO", soloStates, TFT_SOLO_COLOR);
    drawButton(mainArea, 10 + BUTTON_WIDTH * 2 + 4, 7, BUTTON_WIDTH, BUTTON_HEIGHT, "MUTE", muteStates, TFT_MUTE_COLOR);


    // --- Track name ---
    mainArea.setTextDatum(TL_DATUM);
    mainArea.setTextColor(TFT_WHITE, TFT_BG_COLOR);
    mainArea.setFont(&fonts::FreeSans24pt7b);
    mainArea.setTextSize(1);
    mainArea.setCursor(7, 90);
    mainArea.print(trackName);

    // --- Fader dB ---
    char faderDbStr[12];
    if (faderPositions < 0.001f) {
        snprintf(faderDbStr, sizeof(faderDbStr), "-inf");
    } else if (faderPositions < 0.75f) {
        float db = (faderPositions / 0.75f) * 60.0f - 60.0f;
        snprintf(faderDbStr, sizeof(faderDbStr), "%.1f dB", db);
    } else {
        float db = ((faderPositions - 0.75f) / 0.25f) * 10.0f;
        snprintf(faderDbStr, sizeof(faderDbStr), "+%.1f dB", db);
    }
    mainArea.setFont(&fonts::FreeSans12pt7b);
    mainArea.setTextColor(TFT_DARKGREY, TFT_BG_COLOR);
    mainArea.setCursor(7, 145);
    mainArea.print(faderDbStr);

    mainArea.pushSprite(0, HEADER_HEIGHT);
    updateAllNeopixels();
    
}

// ════════════════════════════════════════════════════════════
//  drawMeter  — visual S2 (rounded) + cálculo robusto de S3
// ════════════════════════════════════════════════════════════
void drawMeter(LGFX_Sprite &sprite, uint16_t x, uint16_t y,
               uint16_t w, uint16_t h,
               float level, float peakLevel, bool isClipping) {

    const int numSegments     = 12;
    const int padding         = 2;
    const int cornerRadius    = 2;

    if (numSegments <= 1 || h <= (uint16_t)(padding * (numSegments - 1))) return;

    const int segmentHeight = (h - padding * (numSegments - 1)) / numSegments;

    // ── Cálculo de segmentos activos y peak (lógica S3) ──────────────────────
    size_t activeSegments  = (size_t)round(level     * numSegments);
    size_t peakSegment_idx = (size_t)round(peakLevel * numSegments);

    if (peakSegment_idx > 0) peakSegment_idx--;

    if (activeSegments > 0 && peakSegment_idx < activeSegments - 1) {
        peakSegment_idx = activeSegments - 1;
    } else if (activeSegments == 0 && peakSegment_idx != (size_t)-1) {
        peakSegment_idx = (size_t)-1;
    }

    // ── Dibujo con estilo S2 (rounded + doble borde peak) ────────────────────
    for (int i = 0; i < numSegments; i++) {
        int segY = y + h - (i + 1) * segmentHeight - i * padding;

        bool hasPeakBorder = (static_cast<size_t>(i) == peakSegment_idx
                              && peakLevel > level + 0.001f);   // ← condición S3

        uint16_t fillColor;
        if (hasPeakBorder) {
            fillColor = (i < 8) ? VU_GREEN_OFF : (i < 10) ? VU_YELLOW_OFF : VU_RED_OFF;
        } else if (static_cast<size_t>(i) < activeSegments) {
            fillColor = (i < 8) ? VU_GREEN_ON : (i < 10) ? VU_YELLOW_ON : VU_RED_ON;
        } else {
            fillColor = (i < 8) ? VU_GREEN_OFF : (i < 10) ? VU_YELLOW_OFF : VU_RED_OFF;
        }

        if (static_cast<size_t>(i) == (size_t)(numSegments - 1) && isClipping) {
            fillColor = VU_RED_ON;
        }

        sprite.fillRoundRect(x, segY, w, segmentHeight, cornerRadius, fillColor);

        if (hasPeakBorder) {
            sprite.drawRoundRect(x,   segY,   w,   segmentHeight,   cornerRadius,   VU_PEAK_COLOR);
            sprite.drawRoundRect(x+1, segY+1, w-2, segmentHeight-2, cornerRadius-1, VU_PEAK_COLOR);
        }
    }
}
// ════════════════════════════════════════════════════════════
//  drawVUMeters
// ════════════════════════════════════════════════════════════
void drawVUMeters() {
    vuSprite.fillSprite(TFT_MCU_DARKGRAY);
    drawMeter(vuSprite, 3, 4, 42, MAINAREA_HEIGHT - 10,
              vuLevels, vuPeakLevels, vuClipState);
    vuSprite.pushSprite(MAINAREA_WIDTH, HEADER_HEIGHT);
}

// ════════════════════════════════════════════════════════════
//  handleVUMeterDecay — lógica S3 + bug fix timer peak
// ════════════════════════════════════════════════════════════
void handleVUMeterDecay() {
    const unsigned long DECAY_INTERVAL_MS = 100;
    const unsigned long PEAK_HOLD_TIME_MS = 2000;
    const float DECAY_AMOUNT = 1.0f / 12.0f;

    unsigned long now = millis();
    bool changed = false;

    // 1. Decaimiento del nivel normal
    if (vuLevels > 0 && now - vuLastUpdateTime > DECAY_INTERVAL_MS) {
        float old = vuLevels;
        vuLevels -= DECAY_AMOUNT;
        if (vuLevels < 0.01f) vuLevels = 0.0f;
        vuLastUpdateTime = now;
        changed = true;
        log_v("VU Level decayed %.3f → %.3f", old, vuLevels);
    }

    // 2. Decaimiento del peak tras hold time
    if (vuPeakLevels > 0 && now - vuPeakLastUpdateTime > PEAK_HOLD_TIME_MS) {
        if (vuPeakLevels > vuLevels) {
            float old = vuPeakLevels;
            vuPeakLevels = vuLevels;
            vuPeakLastUpdateTime = now;     // ← BUG FIX: sin esto se dispara cada ciclo
            changed = true;
            log_v("VU Peak decayed %.3f → %.3f (jump to current level)", old, vuPeakLevels);
        }
    }

    // 3. Seguridad: peak nunca puede ser menor que el nivel actual
    if (vuPeakLevels < vuLevels) {
        log_w("VU Peak (%.3f) < VU Level (%.3f). Corrigiendo.", vuPeakLevels, vuLevels);
        vuPeakLevels = vuLevels;
        vuPeakLastUpdateTime = now;
        changed = true;
    }

    if (changed) needsVUMetersRedraw = true;
}

// ════════════════════════════════════════════════════════════
//  drawVPotDisplay
// ════════════════════════════════════════════════════════════
void drawVPotDisplay() {
    vPotSprite.fillSprite(TFT_MCU_DARKGRAY);
    int level = Encoder::currentVPotLevel;

    const int totalSegments = 7;
    const int centerWidth   = 30;
    const int gapVPot       = 2;
    const int cornerRadius  = 5;

    int segW   = (vPotSprite.width() - centerWidth - (2 * totalSegments * gapVPot)) / (2 * totalSegments);
    int totalW = (2 * totalSegments * segW) + (2 * totalSegments * gapVPot) + centerWidth;
    int startX = (vPotSprite.width() - totalW) / 2;
    int seg_y  = 3;
    int seg_h  = vPotSprite.height() - 6;

    for (int i = 0; i < totalSegments; i++) {
        int x    = startX + i * (segW + gapVPot);
        bool act = (level < 0 && i >= totalSegments + level);
        vPotSprite.fillRoundRect(x, seg_y, segW, seg_h, cornerRadius, act ? TFT_MCU_BLUE : TFT_MCU_GRAY);
    }
    int cx = startX + totalSegments * (segW + gapVPot);
    vPotSprite.fillRoundRect(cx, seg_y, centerWidth, seg_h, cornerRadius, TFT_MCU_BLUE);
    for (int i = 0; i < totalSegments; i++) {
        int x    = cx + centerWidth + gapVPot + i * (segW + gapVPot);
        bool act = (level > 0 && i < level);
        vPotSprite.fillRoundRect(x, seg_y, segW, seg_h, cornerRadius, act ? TFT_MCU_BLUE : TFT_MCU_GRAY);
    }

    
    
    vPotSprite.pushSprite(0, MAINAREA_HEIGHT + HEADER_HEIGHT);
}

void setVPotLevel(int8_t level) {
    if (level < VPOT_MIN_LEVEL) level = VPOT_MIN_LEVEL;
    if (level > VPOT_MAX_LEVEL) level = VPOT_MAX_LEVEL;
    if (level != currentVPotLevel) {
        currentVPotLevel = level;
        needsVPotRedraw = true;
    }
}