// src/display/UIOffline.cpp
#include "UIOffline.h"
#include "../config.h"
#include "Display.h"
#include "lvgl.h"
#include <LittleFS.h>

#define P4_W        480
#define P4_H        800
#define LOGO_W      300
#define LOGO_H       57
#define LOGO_X      ((P4_W - LOGO_W) / 2)
#define LOGO_Y      ((P4_H - LOGO_H) / 2)

static lv_obj_t*    s_screen      = NULL;
static lv_obj_t*    s_logo        = NULL;
static lv_obj_t*    s_blink_label = NULL;
static uint8_t*     s_logo_buf    = NULL;
static bool         s_logo_ready  = false;
static int          s_logo_reveal = 0;
static uint8_t      s_blink_cnt   = 0;
static uint32_t     s_lastTick    = 0;
static uint32_t     s_lastLetter  = 0;

static bool s_offline_active = false;

static lv_image_dsc_t s_img_dsc;

void uiOfflineCreate() {
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    // ── Logo desde LittleFS ───────────────────────────────
    if (LittleFS.exists("/logo.jpg")) {
        File f = LittleFS.open("/logo.jpg", "r");
        if (f) {
            size_t sz = f.size();
            s_logo_buf = (uint8_t*)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
            if (s_logo_buf) {
                f.read(s_logo_buf, sz);
                f.close();

                memset(&s_img_dsc, 0, sizeof(s_img_dsc));
                s_img_dsc.header.magic  = LV_IMAGE_HEADER_MAGIC;
                s_img_dsc.header.cf     = LV_COLOR_FORMAT_RAW;
                s_img_dsc.header.w      = LOGO_W;
                s_img_dsc.header.h      = LOGO_H;
                s_img_dsc.data_size     = sz;
                s_img_dsc.data          = s_logo_buf;

                s_logo = lv_image_create(s_screen);
                lv_obj_set_style_transform_rotation(s_logo, 900, 0);
                lv_obj_set_style_transform_pivot_x(s_logo, LV_PCT(50), 0);
                lv_obj_set_style_transform_pivot_y(s_logo, LV_PCT(50), 0);
                lv_image_set_src(s_logo, &s_img_dsc);
                lv_obj_set_pos(s_logo, LOGO_X +40, LOGO_Y+ 10) ;  // sube un poco

                lv_obj_set_size(s_logo, 0, LOGO_H);
                s_logo_ready = true;
                log_i("[Offline] logo OK");
            } else {
                f.close();
                log_e("[Offline] malloc falló");
            }
        }
    } else {
        log_w("[Offline] logo.jpg no encontrado");
    }

   // ── Texto parpadeante ─────────────────────────────────
s_blink_label = lv_label_create(s_screen);
lv_label_set_text(s_blink_label, "Esperando Logic Pro...");
lv_obj_set_style_text_color(s_blink_label, lv_color_hex(0xFFFFFF), 0);
lv_obj_set_style_text_font(s_blink_label, &lv_font_montserrat_16, 0);
lv_obj_set_pos(s_blink_label, 30   , LOGO_Y + LOGO_H - 30);
lv_obj_set_style_transform_rotation(s_blink_label, 900, 0);
lv_obj_set_style_transform_pivot_x(s_blink_label, LV_PCT(50), 0);
lv_obj_set_style_transform_pivot_y(s_blink_label, LV_PCT(50), 0);
lv_obj_add_flag(s_blink_label, LV_OBJ_FLAG_HIDDEN);

    s_logo_reveal = 0;
    s_blink_cnt   = 0;
    s_lastTick    = 0;
    s_lastLetter  = 0;

    s_offline_active = true;

    lv_scr_load(s_screen);
    log_i("[Offline] uiOfflineCreate OK");
}

void uiOfflineTick() {
    if (!s_offline_active) return;
    uint32_t now = millis();
    if (now - s_lastTick < 33) return;
    s_lastTick = now;

    // ── Reveal logo ───────────────────────────────────────
    if (s_logo_ready && s_logo_reveal < LOGO_W) {
        if (now - s_lastLetter >= 100) {
            s_lastLetter  = now;
            s_logo_reveal = min(s_logo_reveal + 60, LOGO_W);
            lv_obj_set_width(s_logo, s_logo_reveal);
            lv_obj_invalidate(s_logo);
        }
    }

    // ── Texto parpadeante ─────────────────────────────────
    if (s_logo_reveal >= LOGO_W) {
        s_blink_cnt++;
        if ((s_blink_cnt & 0x1F) == 0) {
            bool show = (s_blink_cnt & 0x20);
            if (show) lv_obj_remove_flag(s_blink_label, LV_OBJ_FLAG_HIDDEN);
            else      lv_obj_add_flag(s_blink_label, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void uiOfflineDestroy() {
    s_offline_active = false;
    if (s_screen)   { lv_obj_del(s_screen); s_screen = NULL; }
    if (s_logo_buf) { heap_caps_free(s_logo_buf); s_logo_buf = NULL; }
    s_logo_ready  = false;
    s_logo_reveal = 0;
    log_i("[Offline] destruido");
}