// src/main.cpp  —  TEST LovyanGFX Track S2 — TODO EN UN ARCHIVO
#include <Arduino.h>
#include <LovyanGFX.hpp>

// ═══════════════════════════════════════════════════════════════
//  CONFIGURACIÓN LOVYANGFX  (inline, sin header externo)
// ═══════════════════════════════════════════════════════════════
class LGFX : public lgfx::LGFX_Device {

    lgfx::Panel_ST7789  _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;

public:
    LGFX(void) {

        // --- BUS SPI ---
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host    = SPI2_HOST;      // HSPI en ESP32-S2
            cfg.spi_mode    = 0;
            cfg.freq_write  = 20000000;
            cfg.freq_read   = 20000000;
            cfg.spi_3wire   = false;
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk    = 7;              // TFT_SCLK
            cfg.pin_mosi    = 4;              // TFT_MOSI
            cfg.pin_miso    = -1;
            cfg.pin_dc      = 6;              // TFT_DC
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        // --- PANEL ST7789 240x280 ---
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = 5;         // TFT_CS
            cfg.pin_rst          = 33;        // TFT_RST
            cfg.pin_busy         = -1;
            cfg.memory_width     = 240;
            cfg.memory_height    = 280;
            cfg.panel_width      = 240;
            cfg.panel_height     = 280;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = false;
            cfg.invert           = true;      // ← false si colores invertidos
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false;
            _panel_instance.config(cfg);
        }

        // --- BACKLIGHT PWM ---
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl      = 3;              // TFT_BL
            cfg.invert      = false;
            cfg.freq        = 500;            // Hz bajos → sin crosstalk SPI
            cfg.pwm_channel = 0;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        setPanel(&_panel_instance);
    }
};

// ═══════════════════════════════════════════════════════════════
//  OBJETOS GLOBALES
// ═══════════════════════════════════════════════════════════════
LGFX        tft;
LGFX_Sprite sprite(&tft);

// ═══════════════════════════════════════════════════════════════
//  COLORES  (RGB565 — los mismos que en tu proyecto)
// ═══════════════════════════════════════════════════════════════
#define COL_BG       0x0842
#define COL_WHITE    0xFFFF
#define COL_RED      0xF800
#define COL_GREEN    0x3F20
#define COL_BLUE     0x025D
#define COL_YELLOW   0xFFC0
#define COL_GRAY     0x3186
#define COL_DARKGRAY 0x18C3

// ═══════════════════════════════════════════════════════════════
//  TEST 1 — Fill básico  (verifica SPI + colores)
// ═══════════════════════════════════════════════════════════════
void testBasicFill() {
    Serial.println("[1] Fill screen");
    tft.fillScreen(COL_RED);   delay(400);
    tft.fillScreen(COL_GREEN); delay(400);
    tft.fillScreen(COL_BLUE);  delay(400);
    tft.fillScreen(COL_WHITE); delay(400);
    tft.fillScreen(COL_BG);    delay(200);
}

// ═══════════════════════════════════════════════════════════════
//  TEST 2 — Texto y fuentes  (las que usa Display.cpp)
// ═══════════════════════════════════════════════════════════════
void testText() {
    Serial.println("[2] Texto");
    tft.fillScreen(COL_BG);
    tft.setTextDatum(TL_DATUM);

    tft.setTextFont(2);
    tft.setTextColor(COL_WHITE, COL_BG);
    tft.setCursor(10, 10);
    tft.print("LovyanGFX OK - Font2");

    tft.setTextFont(4);
    tft.setTextColor(COL_GREEN, COL_BG);
    tft.setCursor(10, 40);
    tft.print("Track S2 - Font4");

    tft.setTextFont(7);                          // 7-segment (tiempo/tempo)
    tft.setTextColor(COL_YELLOW, COL_BG);
    tft.setCursor(10, 80);
    tft.print("12:34");

    tft.setTextFont(2);
    tft.setTextColor(COL_GRAY, COL_BG);
    tft.setCursor(10, 160);
    tft.printf("Panel: %d x %d px", tft.width(), tft.height());
    tft.setCursor(10, 180);
    tft.printf("Heap libre: %u bytes", ESP.getFreeHeap());

    delay(2500);
}

// ═══════════════════════════════════════════════════════════════
//  TEST 3 — Botones + VU meter estático
// ═══════════════════════════════════════════════════════════════
void testShapes() {
    Serial.println("[3] Botones + VU");
    tft.fillScreen(COL_BG);

    // — Botones REC / SOLO / MUTE / SELECT —
    struct { uint16_t col; const char* label; } btns[4] = {
        { COL_RED,    "REC"    },
        { COL_YELLOW, "SOLO"   },
        { COL_RED,    "MUTE"   },
        { COL_BLUE,   "SELECT" },
    };
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(2);
    for (int i = 0; i < 4; i++) {
        int y = 10 + i * 38;
        tft.fillRoundRect(10, y, 110, 30, 5, btns[i].col);
        tft.setTextColor(COL_BG, btns[i].col);
        tft.drawString(btns[i].label, 65, y + 15);
    }

    // — VU meter vertical simulado —
    const int vuX = 160, vuY = 10, vuW = 20, vuH = 200;
    const int level = 140;    // píxeles activos desde abajo
    const int segH  = 4, gap = 2;

    for (int i = 0; i * (segH+gap) < vuH; i++) {
        int pxFromBottom = i * (segH + gap);
        int y = vuY + vuH - pxFromBottom - segH;
        uint16_t onCol  = (pxFromBottom > vuH*0.85) ? COL_RED
                        : (pxFromBottom > vuH*0.65) ? COL_YELLOW
                        :                             COL_GREEN;
        uint16_t drawCol = (pxFromBottom < level) ? onCol : COL_DARKGRAY;
        tft.fillRoundRect(vuX, y, vuW, segH, 1, drawCol);
    }

    // Peak indicator
    int peakY = vuY + vuH - 170 - segH;
    tft.fillRect(vuX, peakY, vuW, 2, COL_WHITE);

    delay(2500);
}

// ═══════════════════════════════════════════════════════════════
//  TEST 4 — Sprite con barra vPot animada  (verifica DMA)
// ═══════════════════════════════════════════════════════════════
void testSprite() {
    Serial.println("[4] Sprite / DMA - vPot animado");
    tft.fillScreen(COL_BG);

    // Header estático
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(COL_WHITE, COL_BG);
    tft.drawString("SPRITE + DMA TEST", 120, 15);

    // Track name area
    tft.fillRoundRect(10, 35, 220, 25, 4, COL_GRAY);
    tft.setTextColor(COL_WHITE, COL_GRAY);
    tft.drawString("Track 01", 120, 47);

    // Sprite para vPot (ancho completo, altura fija)
    sprite.createSprite(240, 44);

    const int totalSeg = 7;
    const int centerW  = 26;
    const int segW     = 13;
    const int gap      = 3;
    const int totalW   = totalSeg*2*(segW+gap) + centerW;
    const int startX   = (240 - totalW) / 2;

    for (int frame = 0; frame < 90; frame++) {
        // nivel oscila -7..+7
        float t    = frame * 0.15f;
        int   level = (int)(6.5f * sinf(t));

        sprite.fillSprite(COL_BG);

        // segmentos izquierda
        for (int i = 0; i < totalSeg; i++) {
            int x    = startX + i*(segW+gap);
            bool act = (level < 0 && i >= totalSeg + level);
            sprite.fillRoundRect(x, 7, segW, 30, 4, act ? COL_BLUE : COL_GRAY);
        }
        // centro
        int cx = startX + totalSeg*(segW+gap);
        sprite.fillRoundRect(cx, 7, centerW, 30, 4, COL_BLUE);
        // segmentos derecha
        for (int i = 0; i < totalSeg; i++) {
            int x    = cx + centerW + gap + i*(segW+gap);
            bool act = (level > 0 && i < level);
            sprite.fillRoundRect(x, 7, segW, 30, 4, act ? COL_BLUE : COL_GRAY);
        }

        // Nivel numérico
        sprite.setTextDatum(MC_DATUM);
        sprite.setTextFont(2);
        sprite.setTextColor(COL_WHITE, COL_BG);
        char buf[8];
        snprintf(buf, sizeof(buf), "%+d", level);
        sprite.drawString(buf, 120, 40);    // debajo de la barra (fuera del sprite... ajusta Y)

        sprite.pushSprite(0, 80);
        delay(40);
    }

    sprite.deleteSprite();
    delay(500);
}

// ═══════════════════════════════════════════════════════════════
//  TEST 5 — Brillo  (verifica Light_PWM sin ledcAttach)
// ═══════════════════════════════════════════════════════════════
void testBrightness() {
    Serial.println("[5] Brillo PWM");
    tft.fillScreen(COL_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    tft.setTextColor(COL_BG, COL_WHITE);
    tft.drawString("Fade test", 120, 130);

    for (int b = 255; b >= 10; b -= 4) { tft.setBrightness(b); delay(12); }
    for (int b = 10; b <= 255; b += 4) { tft.setBrightness(b); delay(12); }

    delay(500);
}

// ═══════════════════════════════════════════════════════════════
//  SETUP / LOOP
// ═══════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== TEST LovyanGFX — Track ESP32-S2 ===");

    tft.init();
    tft.setRotation(0);
    tft.setBrightness(255);
    tft.fillScreen(COL_BG);

    Serial.printf("Panel inicializado: %d x %d\n", tft.width(), tft.height());
    Serial.printf("Heap libre: %u bytes\n", ESP.getFreeHeap());

    // Pantalla de bienvenida
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    tft.setTextColor(COL_WHITE, COL_BG);
    tft.drawString("INIT OK", 120, 110);
    tft.setTextFont(2);
    tft.setTextColor(COL_GREEN, COL_BG);
    tft.drawString("LovyanGFX  ESP32-S2", 120, 148);
    tft.setTextColor(COL_GRAY, COL_BG);
    tft.drawString("ST7789  240x280", 120, 168);
    delay(2000);
}

void loop() {
    testBasicFill();
    testText();
    testShapes();
    testSprite();
    testBrightness();
    Serial.println("--- Ciclo completo OK ---\n");
    delay(1000);
}