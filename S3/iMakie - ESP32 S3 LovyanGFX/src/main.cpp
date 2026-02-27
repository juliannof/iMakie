#include <Arduino.h>
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7796  _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;

public:
    LGFX(void)
    {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host    = SPI2_HOST;
            cfg.spi_mode    = 0;
            cfg.freq_write  = 80000000;
            cfg.freq_read   = 20000000;
            cfg.spi_3wire   = false;
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk    = 10;
            cfg.pin_mosi    = 11;
            cfg.pin_miso    = 4;
            cfg.pin_dc      = 12;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = 14;
            cfg.pin_rst          = 13;
            cfg.pin_busy         = -1;
            cfg.panel_width      = 320;
            cfg.panel_height     = 480;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = true;
            cfg.invert           = false;
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false;
            _panel_instance.config(cfg);
        }
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl      = 18;
            cfg.invert      = false;
            cfg.freq        = 2000;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }
        setPanel(&_panel_instance);
    }
};

LGFX tft;

// ═══════════════════════════════════════════════════════════
//  NIVELES DE BRILLO A PROBAR
// ═══════════════════════════════════════════════════════════
const uint8_t  niveles[]  = { 255, 128, 60, 0 };
const char*    etiquetas[] = { "255 - MAXIMO", "128 - MEDIO", "60 - BAJO", "0 - APAGADO" };
const uint32_t colores[]   = { TFT_WHITE, TFT_CYAN, TFT_YELLOW, TFT_BLACK };

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("=== Test Brillo 200Hz ===");

    tft.init();
    tft.setRotation(0);
    tft.setBrightness(255);
    tft.fillScreen(TFT_BLACK);

    Serial.println("Init OK");
}

void loop()
{
    for (int i = 0; i < 4; i++)
    {
        Serial.printf("Brillo: %s\n", etiquetas[i]);

        tft.setBrightness(niveles[i]);
        tft.fillScreen(colores[i]);

        tft.setTextColor(TFT_BLACK ^ colores[i]);
        tft.setTextSize(3);
        tft.drawString(etiquetas[i], 10, tft.height() / 2 - 16);

        delay(2000);
    }
}