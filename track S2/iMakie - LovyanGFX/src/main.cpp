#include <Arduino.h>
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789  _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;
public:
    LGFX(void) {
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
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = 5;
            cfg.pin_rst          = 33;
            cfg.pin_busy         = -1;
            cfg.memory_width     = 240;
            cfg.memory_height    = 320;
            cfg.panel_width      = 240;
            cfg.panel_height     = 280;
            cfg.offset_x         = 0;
            cfg.offset_y         = 20;
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

LGFX tft;

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== TEST BRILLO ===");

    // Reset manual
    //pinMode(33, OUTPUT);
    //digitalWrite(33, LOW);
    //delay(100);
    //digitalWrite(33, HIGH);
    //delay(200);

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_WHITE);

    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.drawString("TEST BRILLO", 120, 130);

    delay(1000);

    // --- Fade OUT ---
    Serial.println("Fade OUT...");
    for (int b = 255; b >= 0; b -= 3) {
        tft.setBrightness(b);
        Serial.printf("BL: %d\n", b);
        delay(15);
    }
    delay(500);

    // --- Fade IN ---
    Serial.println("Fade IN...");
    for (int b = 0; b <= 255; b += 3) {
        tft.setBrightness(b);
        Serial.printf("BL: %d\n", b);
        delay(15);
    }
    delay(500);

    // --- Niveles fijos ---
    Serial.println("25%");
    tft.setBrightness(64);
    tft.fillScreen(TFT_WHITE);
    tft.drawString("25%", 120, 130);
    delay(1500);

    Serial.println("50%");
    tft.setBrightness(128);
    tft.fillScreen(TFT_WHITE);
    tft.drawString("50%", 120, 130);
    delay(1500);

    Serial.println("100%");
    tft.setBrightness(255);
    tft.fillScreen(TFT_WHITE);
    tft.drawString("100%", 120, 130);
    delay(1500);

    Serial.println("=== FIN TEST BRILLO ===");
}

void loop() {
    Serial.println("OK");
    delay(2000);
}