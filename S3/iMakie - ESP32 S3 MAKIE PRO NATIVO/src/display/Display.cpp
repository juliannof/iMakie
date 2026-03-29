// src/display/Display.cpp

#include "Display.h"
#include "../midi/MIDIProcessor.h"
#include "../config.h"

// --- Variables "Privadas" de este Módulo ---
namespace {
    uint8_t     screenBrightness   = 255;

    // Ondas
    LGFX_Sprite waveSprite(&tft);
    bool        offlineSpriteReady = false;
    float       wavePhase          = 0.0f;
    uint8_t     blinkCounter       = 0;

    constexpr int WAVE_TOP_Y = 15;
    constexpr int WAVE_BOT_Y = 195;
    constexpr int WAVE_H     = 90;

    // Logo reveal
    LGFX_Sprite logoSprite(&tft);
    bool        logoSpriteReady = false;
    int         logoReveal      = 0;

    constexpr int LOGO_W = 300;
    constexpr int LOGO_H = 57;
    constexpr int LOGO_X = (480 - LOGO_W) / 2;   // 90
    constexpr int LOGO_Y = (320 - LOGO_H) / 2;   // 131
}


volatile ConnectionState logicConnectionState = ConnectionState::DISCONNECTED;


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

    log_i("[DMA] PSRAM: %s", psramFound() ? "SI" : "NO");
    log_i("[DMA] DMA channel: %d", SPI_DMA_CH_AUTO);
    log_i("[DMA] Display: %dx%d", tft.width(), tft.height());
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
    setScreenBrightness(180);
    offlineSpriteReady = false;
    logoSpriteReady    = false;
    logoReveal         = 0;
    wavePhase          = 0.0f;

    tft.fillScreen(TFT_BLACK);

    if (!LittleFS.begin(false)) {
        log_e("[Offline] LittleFS no montado");
        goto create_sprite;
    }

    if (LittleFS.exists("/logo.jpg")) {
        File f = LittleFS.open("/logo.jpg", "r");
        if (f) {
            size_t sz = f.size();
            uint8_t* buf = (uint8_t*)ps_malloc(sz);
            if (buf) {
                f.read(buf, sz);
                f.close();
                LittleFS.end();

                logoSprite.setColorDepth(16);
                logoSprite.setPsram(true);
                logoSprite.createSprite(LOGO_W, LOGO_H);
                logoSprite.fillSprite(TFT_BLACK);
                logoSprite.drawJpg(buf, sz, 0, 0, LOGO_W, LOGO_H);
                free(buf);
                logoSpriteReady = (logoSprite.width() > 0);
                log_i("[Offline] logoSprite: %s", logoSpriteReady ? "OK" : "FALLO");
            } else {
                f.close();
                LittleFS.end();
                log_e("[Offline] ps_malloc falló");
            }
        } else {
            LittleFS.end();
        }
    } else {
        LittleFS.end();
        log_w("[Offline] logo.jpg no encontrado");
    }

create_sprite:
    tft.fillRect(0, WAVE_TOP_Y, 480, WAVE_H, TFT_BLACK);
    tft.fillRect(0, WAVE_BOT_Y, 480, WAVE_H, TFT_BLACK);
    waveSprite.setColorDepth(16);
    waveSprite.setPsram(true);
    waveSprite.createSprite(480, WAVE_H);
    offlineSpriteReady = (waveSprite.width() > 0);
    log_i("[Offline] waveSprite: %s", offlineSpriteReady ? "OK" : "FALLO");
}

void tickOfflineAnimation() {
    static uint32_t lastTick  = 0;
    static float    phase1    = 0.0f;
    static float    phase2    = 0.0f;
    static float    phase3    = 0.0f;
    static float    phase4    = 0.0f;
    static float    ampA      = 28.0f;
    static float    ampB      = 16.0f;
    static float    ampAdir   = 0.04f;
    static float    ampBdir   = 0.03f;

    uint32_t now = millis();
    if (now - lastTick < 33) return;
    lastTick = now;

    if (!offlineSpriteReady) return;

    // ── Reveal logo letra a letra ──
    if (logoSpriteReady && logoReveal < LOGO_W) {
        static uint32_t lastLetter = 0;
        constexpr int   LETTER_W   = 60;    // 300px / 5 letras
        constexpr uint32_t DELAY   = 100;   // ms entre letras

        if (now - lastLetter >= DELAY) {
            lastLetter = now;
            logoReveal = min(logoReveal + LETTER_W, LOGO_W);
            tft.fillRect(LOGO_X, LOGO_Y, LOGO_W, LOGO_H, TFT_BLACK);
            tft.setClipRect(LOGO_X, LOGO_Y, logoReveal, LOGO_H);
            logoSprite.pushSprite(LOGO_X, LOGO_Y);
            tft.clearClipRect();
        }
    }

    // Fases a velocidades distintas
    phase1 += 0.061f;
    phase2 += 0.037f;
    phase3 += 0.083f;
    phase4 += 0.019f;
    if (phase1 > 628.f) phase1 -= 628.f;
    if (phase2 > 628.f) phase2 -= 628.f;
    if (phase3 > 628.f) phase3 -= 628.f;
    if (phase4 > 628.f) phase4 -= 628.f;

    // Amplitud que respira
    ampA += ampAdir;
    if (ampA > 32.f || ampA < 14.f) ampAdir = -ampAdir;
    ampB += ampBdir;
    if (ampB > 20.f || ampB <  8.f) ampBdir = -ampBdir;

    // ── Dibujar sprite tenue ──
    waveSprite.fillSprite(TFT_BLACK);
    waveSprite.drawFastHLine(0, WAVE_H / 2, 480, waveSprite.color565(8, 8, 8));

    // Onda A
    for (int x = 0; x < 479; x++) {
        auto yA = [&](int px) {
            return (int)(WAVE_H/2
                + ampA * sinf(px * 0.034f + phase1)
                + (ampA * 0.3f) * sinf(px * 0.071f + phase2));
        };
        int y1 = constrain(yA(x),   1, WAVE_H - 2);
        int y2 = constrain(yA(x+1), 1, WAVE_H - 2);
        waveSprite.drawLine(x, y1, x+1, y2, waveSprite.color565(35, 35, 35));
        waveSprite.drawPixel(x, y1 - 1, waveSprite.color565(12, 12, 12));
        waveSprite.drawPixel(x, y1 + 1, waveSprite.color565(12, 12, 12));
    }

    // Onda B
    for (int x = 0; x < 479; x++) {
        auto yB = [&](int px) {
            return (int)(WAVE_H/2
                + ampB * sinf(px * 0.052f + phase3)
                + (ampB * 0.4f) * sinf(px * 0.021f + phase4));
        };
        int y1 = constrain(yB(x),   1, WAVE_H - 2);
        int y2 = constrain(yB(x+1), 1, WAVE_H - 2);
        waveSprite.drawLine(x, y1, x+1, y2, waveSprite.color565(22, 22, 22));
        waveSprite.drawPixel(x, y1 - 1, waveSprite.color565(8, 8, 8));
        waveSprite.drawPixel(x, y1 + 1, waveSprite.color565(8, 8, 8));
    }

    waveSprite.pushSprite(0, WAVE_TOP_Y);
    waveSprite.pushSprite(0, WAVE_BOT_Y);

    // ── Texto parpadeante — solo visible cuando logo ya está completo ──
    if (logoReveal >= LOGO_W) {
        blinkCounter++;
        if ((blinkCounter & 0x1F) == 0) {
            bool show = (blinkCounter & 0x20);
            tft.setTextDatum(MC_DATUM);
            tft.setTextSize(1);
            tft.setTextColor(show ? TFT_WHITE : TFT_BLACK, TFT_BLACK);
            tft.drawString("Esperando Logic Pro...", 240, WAVE_BOT_Y + WAVE_H + 8);
        }
    }
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

// ****************************************************************************
// Lógica de decaimiento de los vúmetros y retención de picos
// ****************************************************************************
void handleVUMeterDecay() {
    log_v("handleVUMeterDecay() llamado.");

    const unsigned long DECAY_INTERVAL_MS = 100;    // Frecuencia de decaimiento del nivel normal
    const unsigned long PEAK_HOLD_TIME_MS = 2000;   // Tiempo que el pico se mantiene visible
    const float DECAY_AMOUNT = 1.0f / 12.0f;        // Cantidad de decaimiento (aproximadamente 1 segmento)
    
    unsigned long currentTime = millis(); // Obtener el tiempo actual una sola vez
    bool anyVUMeterChanged = false; // Flag para saber si algún VU cambió por decaimiento

    for (int i = 0; i < 8; i++) {
        // --- 1. Decaimiento del Nivel Normal (RMS/Instantáneo) ---
        if (vuLevels[i] > 0) {
            // Si el tiempo desde la última actualización excede el intervalo de decaimiento
            if (currentTime - vuLastUpdateTime[i] > DECAY_INTERVAL_MS) {
                float oldLevel = vuLevels[i];
                vuLevels[i] -= DECAY_AMOUNT; // Reducir el nivel
                if (vuLevels[i] < 0.01f) { // Evitar números negativos y asegurar que llega a cero
                    vuLevels[i] = 0.0f;
                }
                vuLastUpdateTime[i] = currentTime; // Actualizar el tiempo de la última caída
                anyVUMeterChanged = true; // Indicar que hubo un cambio
                log_v("  Track %d: VU Level decayed from %.3f to %.3f.", i, oldLevel, vuLevels[i]);
            }
        }
        
        // --- 2. Decaimiento/Reinicia de Nivel de Pico (Peak Hold) ---
        if (vuPeakLevels[i] > 0) {
            // Si ha pasado el tiempo de retención del pico
            if (currentTime - vuPeakLastUpdateTime[i] > PEAK_HOLD_TIME_MS) {
                // El pico "salta" hacia abajo hasta el nivel normal actual.
                if (vuPeakLevels[i] > vuLevels[i]) { // Solo si el pico aún está por encima del nivel actual
                    float oldPeakLevel = vuPeakLevels[i];
                    vuPeakLevels[i] = vuLevels[i]; // Igualar el pico al nivel actual
                    anyVUMeterChanged = true; // Indicar que hubo un cambio
                    log_v("  Track %d: VU Peak decayed from %.3f to %.3f (jump to current level).", i, oldPeakLevel, vuPeakLevels[i]);
                }
            }
        }

        // --- 3. Lógica de Seguridad para el Pico ---
        // Asegurarse de que el pico nunca esté por debajo del nivel actual.
        // Si el nivel instantáneo sube por encima del pico, el pico debe igualarlo.
        if (vuPeakLevels[i] < vuLevels[i]) {
            log_w("  Track %d: VU Peak (%.3f) era menor que VU Level (%.3f). Corrigiendo.", i, vuPeakLevels[i], vuLevels[i]);
            vuPeakLevels[i] = vuLevels[i];
            vuPeakLastUpdateTime[i] = currentTime; // Reiniciar el temporizador de retención del pico
            anyVUMeterChanged = true; // Indicar que hubo un cambio
        }
    }

    // Si cualquier vúmetro cambió, activar el flag de redibujo específico para vúmetros
    if (anyVUMeterChanged) {
        needsVUMetersRedraw = true; // <--- Usamos el nuevo flag específico para VUMeters
        log_v("handleVUMeterDecay(): anyVUMeterChanged es TRUE. needsVUMetersRedraw = true.");
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

    if (logicConnectionState == ConnectionState::DISCONNECTED) {
        tickOfflineAnimation();
    }

    if (needsTOTALRedraw) {
        if (logicConnectionState == ConnectionState::CONNECTED) {
            if (offlineSpriteReady) {
                waveSprite.deleteSprite();
                offlineSpriteReady = false;
            }
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