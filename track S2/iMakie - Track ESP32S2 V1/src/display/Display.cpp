// src/display/Display.cpp
#include <Arduino.h>
#include "Display.h"
#include "hardware/encoder/Encoder.h"
#include "../config.h"

// ── extern objetos (definidos en main.cpp) ──────────────────
extern LGFX        tft;          // ← reemplaza TFT_eSPI
extern LGFX_Sprite header;       // ← reemplaza TFT_eSprite
extern LGFX_Sprite mainArea;
extern LGFX_Sprite vuSprite;
extern LGFX_Sprite vPotSprite;

// ── variables privadas ───────────────────────────────────────
namespace {
    uint8_t screenBrightness = 255;
}

// ── prototipos internos ──────────────────────────────────────
void drawMainArea();
void drawVUMeters();
void drawHeaderSprite();
void drawOfflineScreen();
void drawInitializingScreen();
void drawVPotDisplay();
void drawButton(LGFX_Sprite &sprite, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char* label, bool active, uint16_t activeColor);
void drawMeter(LGFX_Sprite &sprite, uint16_t x, uint16_t y, uint16_t w, uint16_t h, float level, float peakLevel, bool isClipping);

// ── banderas de redibujo ─────────────────────────────────────
bool needsTOTALRedraw    = true;
bool needsMainAreaRedraw = false;
bool needsHeaderRedraw   = false;
bool needsVUMetersRedraw = false;
bool needsVPotRedraw     = false;
volatile ConnectionState logicConnectionState = ConnectionState::CONNECTED;

static int8_t currentVPotLevel = VPOT_DEFAULT_LEVEL;

// ════════════════════════════════════════════════════════════
//  initDisplay
// ════════════════════════════════════════════════════════════
void initDisplay() {
    // Reset manual SOLO en power-on, no en RST software
    esp_reset_reason_t reason = esp_reset_reason();
    Serial.printf("Reset reason: %d\n", (int)reason);
    
    if (reason == ESP_RST_POWERON) {
        Serial.println("Power-on: reset manual del panel");
        pinMode(33, OUTPUT);
        digitalWrite(33, LOW);
        delay(100);
        digitalWrite(33, HIGH);
        delay(200);
    }

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BG_COLOR);
    tft.setBrightness(screenBrightness);
    tft.fillScreen(TFT_BLUE                                                                                                                                                          );
    delay(500);
    tft.fillScreen(TFT_BG_COLOR);

    // Especificar 16 bits explícitamente
    mainArea.setColorDepth(16);
    mainArea.createSprite(MAINAREA_WIDTH, MAINAREA_HEIGHT);

    header.setColorDepth(16);
    header.createSprite(TFT_WIDTH, HEADER_HEIGHT);
    
    vuSprite.setColorDepth(16);
    vuSprite.createSprite(TFT_WIDTH - MAINAREA_WIDTH, MAINAREA_HEIGHT);

    vPotSprite.setColorDepth(16);
    vPotSprite.createSprite(TFT_WIDTH, VPOT_HEIGHT);

    // ── DEBUG: verifica que los sprites se crearon ──
    Serial.printf("tft      : %d x %d\n", tft.width(), tft.height());
    Serial.printf("header   : %d x %d  → pushSprite(0, 0)\n",   header.width(),   header.height());
    Serial.printf("mainArea : %d x %d  → pushSprite(0, %d)\n",  mainArea.width(),  mainArea.height(), HEADER_HEIGHT);
    Serial.printf("vuSprite : %d x %d  → pushSprite(%d, %d)\n", vuSprite.width(),  vuSprite.height(), MAINAREA_WIDTH, HEADER_HEIGHT);
    Serial.printf("vPotSprite: %d x %d → pushSprite(0, %d)\n",  vPotSprite.width(),vPotSprite.height(), MAINAREA_HEIGHT + HEADER_HEIGHT);
    Serial.printf("Layout total: %d px alto (pantalla: %d)\n", HEADER_HEIGHT + MAINAREA_HEIGHT + VPOT_HEIGHT, tft.height());

    setVPotLevel(VPOT_DEFAULT_LEVEL);
    needsTOTALRedraw = true;

    // ── verificar allocación de sprites ──
    Serial.printf("header    creado: %s\n", header.width()    > 0 ? "OK" : "FALLO");
    Serial.printf("mainArea  creado: %s\n", mainArea.width()  > 0 ? "OK" : "FALLO");
    Serial.printf("vuSprite  creado: %s\n", vuSprite.width()  > 0 ? "OK" : "FALLO");
    Serial.printf("vPotSprite creado: %s\n",vPotSprite.width()> 0 ? "OK" : "FALLO");
    Serial.printf("Heap libre:  %u bytes\n", ESP.getFreeHeap());
    Serial.printf("PSRAM libre: %u bytes\n", ESP.getFreePsram());
    Serial.println("[SETUP] Display iniciado - LovyanGFX");

    needsTOTALRedraw = true;
    // reset connected_init forzando redibujo
    extern void resetDisplayState();
}

// ════════════════════════════════════════════════════════════
//  setScreenBrightness
// ════════════════════════════════════════════════════════════
void setScreenBrightness(uint8_t brightness) {
    screenBrightness = brightness;
    tft.setBrightness(brightness);   // ← reemplaza ledcWrite
}

// ════════════════════════════════════════════════════════════
//  setVPotLevel
// ════════════════════════════════════════════════════════════
void setVPotLevel(int8_t level) {
    if (level < VPOT_MIN_LEVEL) level = VPOT_MIN_LEVEL;
    if (level > VPOT_MAX_LEVEL) level = VPOT_MAX_LEVEL;
    if (level != currentVPotLevel) {
        currentVPotLevel = level;
        needsVPotRedraw = true;
    }
}

// ════════════════════════════════════════════════════════════
//  updateDisplay
// ════════════════════════════════════════════════════════════
void updateDisplay() {
    setScreenBrightness(screenBrightness);
    switch (logicConnectionState) {
        case ConnectionState::CONNECTED: {
            static bool connected_init = false;
            //Serial.printf("updateDisplay: connected_init=%d needsTOTALRedraw=%d\n", 
            //              connected_init, needsTOTALRedraw);  // ← añade
            if (!connected_init || needsTOTALRedraw) {
                // Primero verifica que el TFT responde directamente
                tft.fillScreen(TFT_RED);
                delay(500);
                tft.fillScreen(TFT_BG_COLOR);

                Serial.println("→ Dibujando todo...");         // ← añade
                drawHeaderSprite();
                drawMainArea();
                drawVUMeters();
                drawVPotDisplay();
                needsTOTALRedraw    = false;
                needsMainAreaRedraw = false;
                needsHeaderRedraw   = false;
                needsVUMetersRedraw = false;
                needsVPotRedraw     = false;
                connected_init      = true;
                Serial.println("→ Dibujo completo OK");       // ← añade

                Serial.printf("Después de push - test directo TFT\n");
                tft.fillRect(0, 0, 20, 20, TFT_GREEN);  // cuadrado verde esquina superior izquierda
            }
            break;
        }
        default:
            Serial.printf("updateDisplay: estado=%d (no CONNECTED)\n", 
                          (int)logicConnectionState);          // ← añade
            break;
    }
}

// ════════════════════════════════════════════════════════════
//  drawOfflineScreen
// ════════════════════════════════════════════════════════════
void drawOfflineScreen() {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("iMakie Control", tft.width() / 2, tft.height() / 2 - 20);
    tft.setTextColor(TFT_ORANGE);
    tft.drawString("Conectando...",  tft.width() / 2, tft.height() / 2 + 10);
}

// ════════════════════════════════════════════════════════════
//  drawInitializingScreen
// ════════════════════════════════════════════════════════════
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
    header.fillSprite(TFT_MCU_DARKGRAY);
    int rectWidth  = TFT_WIDTH - 60;
    int rectHeight = 10;
    int rectX = (TFT_WIDTH  - rectWidth)  / 2;
    int rectY = (HEADER_HEIGHT - rectHeight) / 2;

    if (selectStates) {
        header.fillRoundRect(rectX, rectY, rectWidth, rectHeight, 3, TFT_MCU_BLUE);
        screenBrightness = 255;
    } else {
        header.fillRoundRect(rectX, rectY, rectWidth, rectHeight, 3, TFT_MCU_GRAY);
        screenBrightness = 70;
    }
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

    drawButton(mainArea, 10, 7, BUTTON_WIDTH, BUTTON_HEIGHT, "REC",  recStates,  TFT_REC_COLOR);
    drawButton(mainArea, 10 + BUTTON_WIDTH + 2, 7, BUTTON_WIDTH, BUTTON_HEIGHT, "SOLO", soloStates, TFT_SOLO_COLOR);
    drawButton(mainArea, 10 + BUTTON_WIDTH*2 + 4, 7, BUTTON_WIDTH, BUTTON_HEIGHT, "MUTE", muteStates, TFT_MUTE_COLOR);

    // ── trackName con LovyanGFX (reemplaza LOAD_GFXFF) ──
    mainArea.setTextDatum(TL_DATUM);
    mainArea.setTextColor(TFT_WHITE, TFT_BG_COLOR);
    mainArea.setTextFont(4);       // fuente built-in ~26px
    mainArea.setCursor(7, 80);
    mainArea.print(trackName);

    mainArea.setTextFont(2);       // fuente built-in ~16px
    mainArea.setTextColor(TFT_DARKGREY, TFT_BG_COLOR);
    mainArea.setCursor(7, 120);
    mainArea.print("-60 dB");

    mainArea.pushSprite(0, HEADER_HEIGHT);
}

// ════════════════════════════════════════════════════════════
//  drawMeter
// ════════════════════════════════════════════════════════════
void drawMeter(LGFX_Sprite &sprite, uint16_t x, uint16_t y,
               uint16_t w, uint16_t h,
               float level, float peakLevel, bool isClipping) {
    const int numSegments    = 12;
    const int padding        = 2;
    const int peakBorderWidth = 2;

    if (numSegments <= 1 || h <= (uint16_t)(padding * (numSegments - 1))) return;

    int segmentHeight = (h - padding * (numSegments - 1)) / numSegments;
    int cornerRadius  = 2;
    size_t activeSegments = (size_t)(level * numSegments);
    size_t peakSegment    = (size_t)(peakLevel * numSegments);
    if (peakSegment >= (size_t)numSegments) peakSegment = numSegments - 1;

    const uint16_t green_on   = TFT_GREEN;
    const uint16_t green_off  = VU_GREEN_OFF;
    const uint16_t yellow_on  = TFT_YELLOW;
    const uint16_t yellow_off = VU_YELLOW_OFF;
    const uint16_t red_on     = TFT_RED;
    const uint16_t red_off    = VU_RED_OFF;

    for (int i = 0; i < numSegments; i++) {
        int segY = y + h - (i + 1) * segmentHeight - i * padding;
        bool hasPeakBorder = (i == (int)peakSegment && peakLevel > 0.0f);

        uint16_t fillColor, borderColor = VU_PEAK_COLOR;

        if (hasPeakBorder) {
            fillColor = (i < 8) ? green_off : (i < 10) ? yellow_off : red_off;
        } else if ((size_t)i < activeSegments) {
            fillColor = (i < 8) ? green_on : (i < 10) ? yellow_on : red_on;
        } else {
            fillColor = (i < 8) ? green_off : (i < 10) ? yellow_off : red_off;
        }

        if ((size_t)i == (size_t)(numSegments - 1) && isClipping) fillColor = red_on;

        if (hasPeakBorder) {
            sprite.fillRoundRect(x, segY, w, segmentHeight, cornerRadius, fillColor);
            sprite.drawRoundRect(x, segY, w, segmentHeight, cornerRadius, borderColor);
            sprite.drawRoundRect(x+1, segY+1, w-2, segmentHeight-2, cornerRadius-1, borderColor);
        } else {
            sprite.fillRoundRect(x, segY, w, segmentHeight, cornerRadius, fillColor);
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
//  handleVUMeterDecay
// ════════════════════════════════════════════════════════════
void handleVUMeterDecay() {
    const unsigned long DECAY_INTERVAL_MS = 150;
    const unsigned long PEAK_HOLD_TIME_MS = 1500;
    const float DECAY_AMOUNT = 1.0f / 12.0f;

    unsigned long currentTime = millis();
    bool changed = false;

    if (vuLevels > 0 && currentTime - vuLastUpdateTime > DECAY_INTERVAL_MS) {
        vuLevels -= DECAY_AMOUNT;
        if (vuLevels < 0.0f) vuLevels = 0.0f;
        vuLastUpdateTime = currentTime;
        changed = true;
    }
    if (vuPeakLevels > 0 && currentTime - vuPeakLastUpdateTime > PEAK_HOLD_TIME_MS) {
        if (vuPeakLevels > vuLevels) {
            vuPeakLevels = vuLevels;
            changed = true;
        }
    }
    if (vuPeakLevels < vuLevels) {
        vuPeakLevels = vuLevels;
        vuPeakLastUpdateTime = currentTime;
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

    const int totalSegments  = 7;
    const int centerWidth    = 30;
    const int gapVPot        = 2;
    const int cornerRadius   = 5;

    int segW     = (vPotSprite.width() - centerWidth - (2 * totalSegments * gapVPot)) / (2 * totalSegments);
    int totalW   = (2 * totalSegments * segW) + (2 * totalSegments * gapVPot) + centerWidth;
    int startX   = (vPotSprite.width() - totalW) / 2;
    int seg_y    = 3;
    int seg_h    = vPotSprite.height() - 6;

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