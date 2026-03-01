#include <Arduino.h>
#include <LovyanGFX.hpp>

// ═══════════════════════════════════════════════════════════════
//  LGFX — ST7789V3  1.69"  240x280  ESP32-S2
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
            cfg.spi_host    = SPI3_HOST;
            cfg.spi_mode    = 0;
            cfg.freq_write  = 5000000;
            cfg.freq_read   = 8000000;
            cfg.spi_3wire   = false;
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk    = 7;
            cfg.pin_mosi    = 4;
            cfg.pin_miso    = -1;
            cfg.pin_dc      = 6;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        // --- PANEL ST7789V3 240x280 ---
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = 5;
            cfg.pin_rst          = 33;
            cfg.pin_busy         = -1;
            cfg.memory_width     = 240;
            cfg.memory_height    = 320;   // buffer interno real del ST7789V3
            cfg.panel_width      = 240;
            cfg.panel_height     = 280;
            cfg.offset_x         = 0;
            cfg.offset_y         = 20;    // offset típico 1.69" ST7789V3
            cfg.offset_rotation  = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = false;
            cfg.invert           = true;
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false;
            _panel_instance.config(cfg);
        }

        // --- BACKLIGHT ---
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl      = 3;
            cfg.invert      = false;
            cfg.freq        = 500;
            cfg.pwm_channel = 0;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        setPanel(&_panel_instance);
    }
};

// ═══════════════════════════════════════════════════════════════
//  OBJETOS
// ═══════════════════════════════════════════════════════════════
LGFX        tft;
LGFX_Sprite sprite(&tft);

// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== S2 ST7789V3 TEST ===");

    // Fuerza BL HIGH antes de init — descarta problema de PWM
    pinMode(3, OUTPUT);
    digitalWrite(3, HIGH);
    Serial.println("BL forzado HIGH");

    pinMode(33, OUTPUT);
    digitalWrite(33, LOW);
    delay(100);
    digitalWrite(33, HIGH);
    delay(200);

    tft.init();
    tft.setRotation(0);
    Serial.println("TFT init OK");

    // --- Test 1: colores básicos ---
    Serial.println("[1] RED");
    tft.fillScreen(TFT_RED);
    delay(800);

    Serial.println("[2] GREEN");
    tft.fillScreen(TFT_GREEN);
    delay(800);

    Serial.println("[3] BLUE");
    tft.fillScreen(TFT_BLUE);
    delay(800);

    Serial.println("[4] WHITE");
    tft.fillScreen(TFT_WHITE);
    delay(800);

    // --- Test 2: texto ---
    Serial.println("[5] Texto");
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);

    tft.setTextFont(4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("TRACK S2", 120, 100);

    tft.setTextFont(2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("ST7789V3  1.69\"", 120, 140);

    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("240 x 280", 120, 160);

    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.printf("Heap: %u", ESP.getFreeHeap());
    delay(800);

    // --- Test 3: rectángulos (botones) ---
    Serial.println("[6] Botones");
    tft.fillScreen(TFT_BLACK);
    tft.fillRoundRect(10,  10, 100, 35, 6, TFT_RED);
    tft.fillRoundRect(10,  55, 100, 35, 6, TFT_YELLOW);
    tft.fillRoundRect(10, 100, 100, 35, 6, 0x3F20);   // verde MCU
    tft.fillRoundRect(10, 145, 100, 35, 6, 0x025D);   // azul MCU

    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(TFT_BLACK, TFT_RED);
    tft.drawString("REC",    60, 27);
    tft.setTextColor(TFT_BLACK, TFT_YELLOW);
    tft.drawString("SOLO",   60, 72);
    tft.setTextColor(TFT_WHITE, 0x3F20);
    tft.drawString("MUTE",   60, 117);
    tft.setTextColor(TFT_WHITE, 0x025D);
    tft.drawString("SELECT", 60, 162);

    Serial.println("=== SETUP COMPLETO ===");
}

// ═══════════════════════════════════════════════════════════════
//  LOOP — barra vPot animada con sprite
// ═══════════════════════════════════════════════════════════════
void loop() {
    static int frame = 0;

    // vPot animado en la zona inferior
    sprite.createSprite(240, 44);

    const int totalSeg = 7;
    const int centerW  = 24;
    const int segW     = 12;
    const int gap      = 3;
    const int totalW   = totalSeg * 2 * (segW + gap) + centerW;
    const int startX   = (240 - totalW) / 2;

    int level = (int)(6.5f * sinf(frame * 0.12f));

    sprite.fillSprite(TFT_BLACK);

    for (int i = 0; i < totalSeg; i++) {
        int x    = startX + i * (segW + gap);
        bool act = (level < 0 && i >= totalSeg + level);
        sprite.fillRoundRect(x, 7, segW, 30, 4, act ? 0x025D : 0x3186);
    }
    int cx = startX + totalSeg * (segW + gap);
    sprite.fillRoundRect(cx, 7, centerW, 30, 4, 0x025D);
    for (int i = 0; i < totalSeg; i++) {
        int x    = cx + centerW + gap + i * (segW + gap);
        bool act = (level > 0 && i < level);
        sprite.fillRoundRect(x, 7, segW, 30, 4, act ? 0x025D : 0x3186);
    }

    sprite.pushSprite(0, 230);
    sprite.deleteSprite();

    frame++;
    if (frame % 20 == 0) Serial.printf("frame %d  level %+d\n", frame, level);
    delay(40);
}