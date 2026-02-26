// src/display/Display.cpp

#include "Display.h"
#include "../midi/MIDIProcessor.h"
#include "../config.h"

// --- Variables "Privadas" de este Módulo ---
namespace {
    const int TFT_BL_CHANNEL = 0;
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
    tft.initDMA();
    tft.setRotation(3);
    tft.fillScreen(TFT_BG_COLOR);

    ledcAttach(TFT_BL, 5000, 8);
    setScreenBrightness(screenBrightness);
    ledcWrite(TFT_BL, screenBrightness);

    // Un solo sprite reutilizable para MAIN1 y MAIN2
    mainArea.createSprite(MAIN_AREA_WIDTH, MAIN_AREA_HEIGHT); // 480 × 135
    header.createSprite(SCREEN_WIDTH, HEADER_HEIGHT);          // 480 × 50
    vuSprite.createSprite(TRACK_WIDTH, VU_METER_HEIGHT);       // 60  × 135

    log_i("[SETUP] Módulo de Display iniciado.");
}

void setScreenBrightness(uint8_t brightness) {
    screenBrightness = brightness;
    ledcWrite(TFT_BL_CHANNEL, screenBrightness);
}


// ─────────────────────────────────────────────────────────────────────────────
// PANTALLAS DE ESTADO
// ─────────────────────────────────────────────────────────────────────────────

void drawOfflineScreen() {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("iMakie Control", tft.width() / 2, tft.height() / 2);
}

void drawInitializingScreen() {
    log_v("drawInitializingScreen()");
    tft.fillScreen(TFT_DARKGREY);
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
    header.setFreeFont(&FreeMonoBold12pt7b);
    header.setTextSize(1);
    header.setTextColor(currentTimecodeMode == MODE_BEATS ? TFT_CYAN : TFT_ORANGE);

    log_d("drawHeaderSprite: %s: '%s'",
          (currentTimecodeMode == MODE_BEATS) ? "BEATS" : "TIMECODE",
          displayText.c_str());

    header.drawString(displayText.c_str(), SCREEN_WIDTH / 2, HEADER_HEIGHT / 2);

    header.setTextDatum(MR_DATUM);
    header.setFreeFont(&FreeMono9pt7b);
    header.setTextSize(1);

    if (currentTimecodeMode == MODE_BEATS) {
        header.setTextColor(TFT_GREEN);
        header.drawString("BEATS", SCREEN_WIDTH - 10, HEADER_HEIGHT / 2);
    } else {
        header.setTextColor(TFT_YELLOW);
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

void drawButton(TFT_eSprite &sprite,
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

void drawMeter(TFT_eSprite &sprite, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
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

    uint16_t green_off  = VU_GREEN_OFF;
    uint16_t green_on   = VU_GREEN_ON;
    uint16_t yellow_off = VU_YELLOW_OFF;
    uint16_t yellow_on  = VU_YELLOW_ON;
    uint16_t red_off    = VU_RED_OFF;
    uint16_t red_on     = VU_RED_ON;
    uint16_t peak_color = VU_PEAK_COLOR;

    for (int i = 0; i < numSegments; i++) {
        uint16_t segY = y + h - (static_cast<uint16_t>(i) + 1) * (segmentHeight + padding);
        uint16_t selectedColor;

        if (static_cast<size_t>(i) == peakSegment_idx && peakLevel > level + 0.001f) {
            selectedColor = peak_color;
        } else if (static_cast<size_t>(i) < activeSegments) {
            if      (i < 8)  selectedColor = green_on;
            else if (i < 10) selectedColor = yellow_on;
            else             selectedColor = red_on;
        } else {
            if      (i < 8)  selectedColor = green_off;
            else if (i < 10) selectedColor = yellow_off;
            else             selectedColor = red_off;
        }

        if (static_cast<size_t>(i) == (size_t)(numSegments - 1) && isClipping) {
            selectedColor = red_on;
        }

        sprite.fillRect(x, segY, w, segmentHeight, selectedColor);
    }
}

void drawVUMeters() {
    log_v("drawVUMeters(): 8 canales en Y=%d.", VU_METER_AREA_Y);

    const uint16_t meter_x = (TRACK_WIDTH - 20) / 2; // centrado en sprite de 60px

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
// HELPER: atenúa un color RGB565 a ~25%
// ─────────────────────────────────────────────────────────────────────────────

static uint16_t dimColor(uint16_t color) {
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5)  & 0x3F;
    uint8_t b =  color        & 0x1F;
    r = r / 4;
    g = g / 4;
    b = b / 4;
    return (r << 11) | (g << 5) | b;
}


// ─────────────────────────────────────────────────────────────────────────────
// PÁGINAS PG1 y PG2
// MAIN1 → pads 0-15  (filas 1 y 2) → pushSprite en MAIN1_Y
// MAIN2 → pads 16-31 (filas 3 y 4) → pushSprite en MAIN2_Y
// ─────────────────────────────────────────────────────────────────────────────

// 50% — color medio para pads inactivos (visible pero distinguible del activo)
static uint16_t midColor(uint16_t color) {
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5)  & 0x3F;
    uint8_t b =  color        & 0x1F;
    return ((r / 2) << 11) | ((g / 2) << 5) | (b / 2);
}

static void _drawPadsHalf(const char** labels, const byte* colors,
                           int startPad, int endPad) {
    int cellW   = MAIN_AREA_WIDTH / 8;
    int cellH   = MAIN_AREA_HEIGHT / 2;
    int btnSize = ((cellW < cellH) ? cellW : cellH) - 4;

    for (int i = startPad; i < endPad; i++) {
        int local = i - startPad;
        int fila  = local / 8;
        int col   = local % 8;

        int x = (col  * cellW) + (cellW  - btnSize) / 2;
        int y = (fila * cellH) + (cellH  - btnSize) / 2;

        // ← lookup directo, sin switch
        uint8_t colorIdx  = (colors[i] < 9) ? colors[i] : 0;
        uint16_t activeColor = PALETTE[colorIdx].rgb565;  // ← .rgb565

        uint16_t inactiveColor = dimColor(activeColor);

        uint16_t txtColor = globalShiftPressed ? TFT_YELLOW : TFT_WHITE;
        if (btnState[i] && (activeColor == TFT_WHITE  ||
                            activeColor == TFT_YELLOW ||
                            activeColor == TFT_CYAN)) {
            txtColor = TFT_BLACK;
        }

        drawButton(mainArea, x, y, btnSize, btnSize,
                   labels[i], btnState[i],
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
// MAIN1 → REC / SOLO / MUTE + nombre de pista  → pushSprite en MAIN1_Y
// MAIN2 → no se dibuja — VU meters ocupan MAIN2_Y via drawOverlay()
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

    mainArea.pushSprite(0, MAIN1_Y); // solo MAIN1 — MAIN2 queda libre para VU meters
}


// ─────────────────────────────────────────────────────────────────────────────
// OVERLAY — capa sobre mainArea, solo en PG3
// ─────────────────────────────────────────────────────────────────────────────

void drawOverlay() {
    if (currentPage == 3) drawVUMeters();
}


// ─────────────────────────────────────────────────────────────────────────────
// TABLA DE DESPACHO
// Añadir página: (1) escribir drawPageN()  (2) añadir línea aquí
// ─────────────────────────────────────────────────────────────────────────────

typedef void (*PageDrawFn)();

static const PageDrawFn pageDrawTable[] = {
    nullptr,    // índice 0 — no se usa
    drawPage1,  // currentPage == 1
    drawPage2,  // currentPage == 2
    drawPage3,  // currentPage == 3
};

static const int NUM_PAGES = (sizeof(pageDrawTable) / sizeof(pageDrawTable[0])) - 1;


// ─────────────────────────────────────────────────────────────────────────────
// DISPATCHER PRINCIPAL
// Cada página gestiona sus propios pushSprite internamente
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
// UPDATE DISPLAY — orquestador principal
// ─────────────────────────────────────────────────────────────────────────────

void updateDisplay() {
    log_v("updateDisplay(): conexión=%d totalRedraw=%d",
          (int)logicConnectionState, needsTOTALRedraw);

    // --- REDIBUJO TOTAL ---
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

    // --- REDIBUJO PARCIAL ---
    if (logicConnectionState == ConnectionState::CONNECTED) {
        log_v("updateDisplay(): Header=%d Main=%d VU=%d",
              needsHeaderRedraw, needsMainAreaRedraw, needsVUMetersRedraw);

        if (needsHeaderRedraw) {
            drawHeaderSprite();
            needsHeaderRedraw = false;
        }

        if (needsMainAreaRedraw) {
            drawMainArea();
            drawOverlay();              // ← siempre encima
            needsMainAreaRedraw = false;
            needsVUMetersRedraw = false;
        }

        if (needsVUMetersRedraw) {
            drawOverlay();
            needsVUMetersRedraw = false;
        }
    }
}