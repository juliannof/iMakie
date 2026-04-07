// src/display/Display.cpp
#include "Display.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "lcd/esp_lcd_st7701.h"
#include "lvgl.h"

#define LCD_H_RES           480
#define LCD_V_RES           800
#define LCD_RST_PIN         GPIO_NUM_5
#define LCD_BACKLIGHT_PIN   GPIO_NUM_23
#define LCD_LEDC_CHANNEL    LEDC_CHANNEL_0
#define MIPI_DSI_PHY_LDO_CHAN   3
#define MIPI_DSI_PHY_LDO_MV     2500

static esp_lcd_panel_handle_t s_panel = NULL;
static lv_display_t* s_disp = NULL;

static void backlight_init() {
    const ledc_timer_config_t timer_cfg = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num       = LEDC_TIMER_1,
        .freq_hz         = 5000,
        .clk_cfg         = LEDC_AUTO_CLK
    };
    const ledc_channel_config_t channel_cfg = {
        .gpio_num   = LCD_BACKLIGHT_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LCD_LEDC_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_1,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_timer_config(&timer_cfg);
    ledc_channel_config(&channel_cfg);
}

void displaySetBrightness(uint8_t percent) {
    if (percent > 100) percent = 100;
    uint32_t duty = (1023 * percent) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CHANNEL);
}

static void init_mipi_dsi_power() {
    static esp_ldo_channel_handle_t ldo_handle = NULL;
    esp_ldo_channel_config_t cfg = {
        .chan_id    = MIPI_DSI_PHY_LDO_CHAN,
        .voltage_mv = MIPI_DSI_PHY_LDO_MV,
    };
    esp_ldo_acquire_channel(&cfg, &ldo_handle);
}

lv_display_t* getDisplay() { return s_disp; }

void initDisplay() {
    backlight_init();
    init_mipi_dsi_power();

    // DSI bus
    esp_lcd_dsi_bus_handle_t dsi_bus = NULL;
    esp_lcd_dsi_bus_config_t bus_cfg = {
        .bus_id             = 0,
        .num_data_lanes     = 2,
        .phy_clk_src        = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = 1000,
    };
    esp_lcd_new_dsi_bus(&bus_cfg, &dsi_bus);

    // DBI IO
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_dbi_io_config_t dbi_cfg = ST7701_PANEL_IO_DBI_CONFIG();
    esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_cfg, &io);

    // DPI config
    esp_lcd_dpi_panel_config_t dpi_cfg =
        ST7701_480_360_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
    dpi_cfg.num_fbs = 2;

    // Panel
    st7701_vendor_config_t vendor_cfg = {
        .init_cmds      = NULL,
        .init_cmds_size = 0,
        .mipi_config = {
            .dsi_bus    = dsi_bus,
            .dpi_config = &dpi_cfg,
        },
        .flags = { .use_mipi_interface = 1 },
    };
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = LCD_RST_PIN,
        .rgb_ele_order  = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
        .vendor_config  = &vendor_cfg,
    };
    esp_lcd_new_panel_st7701(io, &panel_cfg, &s_panel);
    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);

    // LVGL init
    lv_init();

    // Buffers
    static uint8_t* lvgl_buf1 = (uint8_t*)heap_caps_malloc(
        LCD_H_RES * 100 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    static uint8_t* lvgl_buf2 = (uint8_t*)heap_caps_malloc(
        LCD_H_RES * 100 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);

    s_disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_buffers(s_disp, lvgl_buf1, lvgl_buf2,
                           LCD_H_RES * 100 * sizeof(lv_color_t),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, [](lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
        esp_lcd_panel_draw_bitmap(s_panel,
                                  area->x1, area->y1,
                                  area->x2 + 1, area->y2 + 1,
                                  px_map);
        lv_display_flush_ready(disp);
    });

    displaySetBrightness(80);
    log_i("[Display] Init OK");
}