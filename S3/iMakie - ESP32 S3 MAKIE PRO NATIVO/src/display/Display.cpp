// src/display/Display.cpp

#include "Display.h"
#include "../midi/MIDIProcessor.h"
#include "../config.h"

// --- Variables "Privadas" de este Módulo ---
namespace {
    uint8_t screenBrightness = 255;
}


// ─────────────────────────────────────────────────────────────────────────────
// INIT DISPLAY
// ─────────────────────────────────────────────────────────────────────────────

void initDisplay() {
    log_v("=== DIAGNÓSTICO DE MEMORIA ESP32-S3 ===");

    if (psramFound()) {
        log_v("PSRAM Detectada. Total: %u bytes (%.2f MB) | Libre: %u bytes",
              ESP.getPsramSize(),
              (float)ESP.getPsramSize() / 1024.0f / 1024.0f,
              ESP.getFreePsram());
    } else {
        log_v("ERROR CRÍTICO: PSRAM NO DETECTADA.");
    }

    tft.init();
    // tft.initDMA() — no existe en LovyanGFX, lo gestiona internamente
    tft.setRotation(3);
    tft.fillScreen(TFT_BG_COLOR);

    // Backlight — digitalWrite puro, sin PWM, sin ruido SPI
    pinMode(TFT_BL, OUTPUT);
    setScreenBrightness(screenBrightness);

    mainArea.createSprite(MAIN_AREA_WIDTH, MAIN_AREA_HEIGHT);
    header.createSprite(SCREEN_WIDTH, HEADER_HEIGHT);
    vuSprite.createSprite(TRACK_WIDTH, VU_METER_HEIGHT);

    log_i("[SETUP] Módulo de Display iniciado.");
}

void setScreenBrightness(uint8_t brightness) {
    screenBrightness = brightness;
    digitalWrite(TFT_BL, brightness > 0 ? HIGH : LOW);
}


// ─────────────────────────────────────────────────────────────────────────────
// PANTALLAS DE ESTADO
// ─────────────────────────────────────────────────────────────────────────────

void drawOfflineScreen() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("iMakie Control", tft.width() / 2, tft.height() / 2);
}

void drawInitializingScreen() {
    log_v("drawInitializingScreen()");
    tft.fillScreen(TFT_BLUE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("MIDI Handshake OK!", tft.width() / 2, tft.height() / 2 - 30);
    tft.setTextColor(TFT_SKYBLUE);
    tft.drawString("Esperando datos de proyecto del DAW...", tft.width() / 2, tft.height() / 2 + 10);
}


// ─────────────────────────────────────────────────────────────────────────────
// HEADER
// ─────────────────────────────────────────────────────────────────────────────

void drawHeaderSprite() {
    if (!needsHeaderRedraw) return;

    header.fillSprite(TFT_BLUE);

    String displayText = (currentTimecodeMode == MODE_BEATS)
                         ? formatBeatString()
                         : formatTimecodeString();

    header.setTextDatum(MC_DATUM);
    header.setFont(&fonts::FreeMonoBold12pt7b);   // ← setFreeFont → setFont
    // header.setFont(&fonts::Font7);   // 7 segmentos — 48px, solo 1234567890:-.
    header.setTextSize(1);
    header.setTextColor(currentTimecodeMode == MODE_BEATS ? TFT_WHITE : TFT_CYAN);

    log_d("drawHeaderSprite: %s: '%s'",
          (currentTimecodeMode == MODE_BEATS) ? "BEATS" : "TIMECODE",
          displayText.c_str());

    header.drawString(displayText.c_str(), SCREEN_WIDTH / 2, HEADER_HEIGHT / 2);

    header.setTextDatum(MR_DATUM);
    header.setFont(&fonts::FreeMono9pt7b);         // ← setFreeFont → setFont
    header.setTextSize(1);

    if (currentTimecodeMode == MODE_BEATS) {
        header.setTextColor(TFT_WHITE);
        header.drawString("BEATS", SCREEN_WIDTH - 10, HEADER_HEIGHT / 2);
    } else {
        header.setTextColor(TFT_CYAN);
        header.drawString("SMPTE", SCREEN_WIDTH - 10, HEADER_HEIGHT / 2);
    }

    static unsigned long lastMIDIActivity = 0;
    if (millis() - lastMIDIActivity < 200) {
        header.fillCircle(SCREEN_WIDTH - 30, 10, 3, TFT_GREEN);
    }

    header.pushSprite(0, 0);
    needsHeaderRedraw = false;
}


// ─────────────────────────────────────────────────────────────────────────────
// DRAWBUTTON
// ─────────────────────────────────────────────────────────────────────────────

void drawButton(LGFX_Sprite &sprite,              // ← TFT_eSprite → LGFX_Sprite
                int x, int y, int w, int h,
                const char* label, bool active,
                uint16_t activeColor,
                uint16_t textColor,
                uint16_t inactiveColor)
{
    uint16_t btnColor = active ? activeColor : inactiveColor;
    sprite.fillRoundRect(x, y, w, h, 8, btnColor);
    sprite.setTextColor(textColor, btnColor);
    sprite.setTextSize(1);
    sprite.setTextDatum(MC_DATUM);
    sprite.drawString(label, x + (w / 2), y + (h / 2));
}


// ─────────────────────────────────────────────────────────────────────────────
// VU METERS
// ─────────────────────────────────────────────────────────────────────────────

void drawMeter(LGFX_Sprite &sprite,               // ← TFT_eSprite → LGFX_Sprite
               uint16_t x, uint16_t y, uint16_t w, uint16_t h,
               float level, float peakLevel, bool isClipping) {
    log_v("  drawMeter(): x=%d y=%d w=%d h=%d level=%.3f peak=%.3f clip=%d",
          x, y, w, h, level, peakLevel, isClipping);

    const int numSegments = 12;
    const int padding     = 1;

    if (numSegments <= 1 || h <= (uint16_t)(padding * (numSegments - 1))) {
        log_e("  drawMeter(): Parámetros inválidos. h=%d", h);
        return;
    }

    const int segmentHeight = (h - padding * (numSegments - 1)) / numSegments;

    size_t activeSegments  = round(level     * numSegments);
    size_t peakSegment_idx = round(peakLevel * numSegments);

    if (peakSegment_idx > 0) peakSegment_idx--;

    if (activeSegments > 0 && peakSegment_idx < activeSegments - 1) {
        peakSegment_idx = activeSegments - 1;
    } else if (activeSegments == 0 && peakSegment_idx != (size_t)-1) {
        peakSegment_idx = (size_t)-1;
    }

    for (int i = 0; i < numSegments; i++) {
        uint16_t segY = y + h - (static_cast<uint16_t>(i) + 1) * (segmentHeight + padding);
        uint16_t selectedColor;

        if (static_cast<size_t>(i) == peakSegment_idx && peakLevel > level + 0.001f) {
            selectedColor = VU_PEAK_COLOR;
        } else if (static_cast<size_t>(i) < activeSegments) {
            if      (i < 8)  selectedColor = VU_GREEN_ON;
            else if (i < 10) selectedColor = VU_YELLOW_ON;
            else             selectedColor = VU_RED_ON;
        } else {
            if      (i < 8)  selectedColor = VU_GREEN_OFF;
            else if (i < 10) selectedColor = VU_YELLOW_OFF;
            else             selectedColor = VU_RED_OFF;
        }

        if (static_cast<size_t>(i) == (size_t)(numSegments - 1) && isClipping) {
            selectedColor = VU_RED_ON;
        }

        sprite.fillRect(x, segY, w, segmentHeight, selectedColor);
    }
}

void drawVUMeters() {
    log_v("drawVUMeters(): 8 canales en Y=%d.", VU_METER_AREA_Y);

    const uint16_t meter_x = (TRACK_WIDTH - 20) / 2;

    for (int track = 0; track < 8; track++) {
        vuSprite.fillSprite(TFT_BG_COLOR);

        drawMeter(vuSprite,
                  meter_x, 0,
                  20, VU_METER_HEIGHT,
                  vuLevels[track],
                  vuPeakLevels[track],
                  vuClipState[track]);

        vuSprite.pushSprite(track * TRACK_WIDTH, VU_METER_AREA_Y);
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// HELPERS DE COLOR
// ─────────────────────────────────────────────────────────────────────────────

static uint16_t dimColor(uint16_t color) {
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5)  & 0x3F;
    uint8_t b =  color        & 0x1F;
    return ((r / 4) << 11) | ((g / 4) << 5) | (b / 4);
}

static uint16_t midColor(uint16_t color) {
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5)  & 0x3F;
    uint8_t b =  color        & 0x1F;
    return ((r / 2) << 11) | ((g / 2) << 5) | (b / 2);
}


// ─────────────────────────────────────────────────────────────────────────────
// PÁGINAS PG1 y PG2
// ─────────────────────────────────────────────────────────────────────────────

static void _drawPadsHalf(const char** labels, const byte* colors,
                           int startPad, int endPad) {
    int cellW   = MAIN_AREA_WIDTH / 8;
    int cellH   = MAIN_AREA_HEIGHT / 2;
    int btnSize = ((cellW < cellH) ? cellW : cellH) - 4;

    bool* stateArray = (currentPage == 1) ? btnStatePG1 : btnStatePG2;

    for (int i = startPad; i < endPad; i++) {
        int local = i - startPad;
        int col   = local % 8;
        int fila  = local / 8;

        int x = (col  * cellW) + (cellW  - btnSize) / 2;
        int y = (fila * cellH) + (cellH  - btnSize) / 2;

        uint8_t  colorIdx      = (colors[i] < 9) ? colors[i] : 0;
        uint16_t activeColor   = PALETTE[colorIdx].rgb565;
        uint16_t inactiveColor = dimColor(activeColor);

        uint16_t txtColor = globalShiftPressed ? TFT_YELLOW : TFT_WHITE;
        if (stateArray[i] && (activeColor == TFT_WHITE  ||
                              activeColor == TFT_YELLOW ||
                              activeColor == TFT_CYAN)) {
            txtColor = TFT_BLACK;
        }

        drawButton(mainArea, x, y, btnSize, btnSize,
                   labels[i], stateArray[i],
                   activeColor, txtColor, inactiveColor);
    }
}

static void _pushMain1Pads(const char** labels, const byte* colors) {
    mainArea.fillSprite(TFT_BG_COLOR);
    _drawPadsHalf(labels, colors, 0, 16);
    mainArea.pushSprite(0, MAIN1_Y);
}

static void _pushMain2Pads(const char** labels, const byte* colors) {
    mainArea.fillSprite(TFT_BG_COLOR);
    _drawPadsHalf(labels, colors, 16, 32);
    mainArea.pushSprite(0, MAIN2_Y);
}

void drawPage1() {
    log_d("drawPage1(): PG1 shift=%d", globalShiftPressed);
    const char** labels = globalShiftPressed ? labels_PG1_SHIFT : labels_PG1;
    _pushMain1Pads(labels, LED_COLORS_PG1);
    _pushMain2Pads(labels, LED_COLORS_PG1);
}

void drawPage2() {
    log_d("drawPage2(): PG2");
    _pushMain1Pads(labels_PG2, LED_COLORS_PG2);
    _pushMain2Pads(labels_PG2, LED_COLORS_PG2);
}


// ─────────────────────────────────────────────────────────────────────────────
// PÁGINA 3 — Mixer
// ─────────────────────────────────────────────────────────────────────────────

void drawPage3() {
    log_v("drawPage3(): Mixer");

    mainArea.fillSprite(TFT_BG_COLOR);

    for (int track = 0; track < 8; ++track) {
        int x                = track * TRACK_WIDTH + BUTTON_SPACING / 2;
        uint16_t textBgColor = selectStates[track] ? TFT_SELECT_BG_COLOR : TFT_BG_COLOR;

        drawButton(mainArea, x, 5,
                   BUTTON_WIDTH, BUTTON_HEIGHT,
                   "REC", recStates[track], TFT_REC_COLOR);

        drawButton(mainArea, x, 5 + BUTTON_HEIGHT + BUTTON_SPACING,
                   BUTTON_WIDTH, BUTTON_HEIGHT,
                   "SOLO", soloStates[track], TFT_SOLO_COLOR);

        drawButton(mainArea, x, 5 + 2 * (BUTTON_HEIGHT + BUTTON_SPACING),
                   BUTTON_WIDTH, BUTTON_HEIGHT,
                   "MUTE", muteStates[track], TFT_MUTE_COLOR);

        int nameY = 5 + 3 * (BUTTON_HEIGHT + BUTTON_SPACING) + 10;
        mainArea.setTextColor(TFT_TEXT_COLOR, textBgColor);
        mainArea.setTextDatum(MC_DATUM);
        mainArea.drawString(trackNames[track].c_str(),
                            track * TRACK_WIDTH + TRACK_WIDTH / 2, nameY);
    }

    mainArea.pushSprite(0, MAIN1_Y);
}


// ─────────────────────────────────────────────────────────────────────────────
// OVERLAY
// ─────────────────────────────────────────────────────────────────────────────

void drawOverlay() {
    if (currentPage == 3) drawVUMeters();
}


// ─────────────────────────────────────────────────────────────────────────────
// TABLA DE DESPACHO
// ─────────────────────────────────────────────────────────────────────────────

typedef void (*PageDrawFn)();

static const PageDrawFn pageDrawTable[] = {
    nullptr,
    drawPage1,
    drawPage2,
    drawPage3,
};

static const int NUM_PAGES = (sizeof(pageDrawTable) / sizeof(pageDrawTable[0])) - 1;


// ─────────────────────────────────────────────────────────────────────────────
// DISPATCHER PRINCIPAL
// ─────────────────────────────────────────────────────────────────────────────

void drawMainArea() {
    log_v("drawMainArea(): currentPage=%d / NUM_PAGES=%d", currentPage, NUM_PAGES);

    if (currentPage < 1 || currentPage > NUM_PAGES) {
        log_e("drawMainArea(): currentPage=%d fuera de rango [1..%d]", currentPage, NUM_PAGES);
        return;
    }

    pageDrawTable[currentPage]();
}


// ─────────────────────────────────────────────────────────────────────────────
// NAVEGACIÓN
// ─────────────────────────────────────────────────────────────────────────────

void nextPage() {
    currentPage = (currentPage % NUM_PAGES) + 1;
    log_d("nextPage(): currentPage ahora = %d", currentPage);
    drawMainArea();
    drawOverlay();
}


// ─────────────────────────────────────────────────────────────────────────────
// UPDATE DISPLAY
// ─────────────────────────────────────────────────────────────────────────────

void updateDisplay() {
    log_v("updateDisplay(): conexión=%d totalRedraw=%d",
          (int)logicConnectionState, needsTOTALRedraw);

    if (needsTOTALRedraw) {
        if (logicConnectionState == ConnectionState::CONNECTED) {
            needsHeaderRedraw = true;
            drawHeaderSprite();
            drawMainArea();
            drawOverlay();
        } else if (logicConnectionState == ConnectionState::MIDI_HANDSHAKE_COMPLETE) {
            drawInitializingScreen();
        } else {
            drawOfflineScreen();
        }
        needsTOTALRedraw    = false;
        needsMainAreaRedraw = false;
        needsHeaderRedraw   = false;
        needsVUMetersRedraw = false;
        return;
    }

    if (logicConnectionState == ConnectionState::CONNECTED) {
        log_v("updateDisplay(): Header=%d Main=%d VU=%d",
              needsHeaderRedraw, needsMainAreaRedraw, needsVUMetersRedraw);

        if (needsHeaderRedraw) {
            drawHeaderSprite();
            needsHeaderRedraw = false;
        }

        if (needsMainAreaRedraw) {
            drawMainArea();
            drawOverlay();
            needsMainAreaRedraw = false;
            needsVUMetersRedraw = false;
        }

        if (needsVUMetersRedraw) {
            drawOverlay();
            needsVUMetersRedraw = false;
        }
    }
}