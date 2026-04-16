// src/display/UIMenu.cpp
#include "UIMenu.h"
#include "../config.h"
#include "Display.h"
#include "lvgl.h"
#include <Arduino.h>
#include <Preferences.h>

// ── Dimensiones ──────────────────────────────────────────
#define PANEL_W      300
#define PANEL_H      P4_H
#define PANEL_X      (HEADER_X - PANEL_W)   // 110px
#define PANEL_Y      0

#define HAM_X        (HEADER_X + (HEADER_W - 44) / 2)
#define HAM_Y        20
#define HAM_SIZE     44

#define SLIDER_W     24
#define SLIDER_H     180

// ── Estado ───────────────────────────────────────────────
static lv_obj_t*   s_ham_btn    = NULL;
static lv_obj_t*   s_ham_lbl    = NULL;   // ← label X / ≡
static lv_obj_t*   s_panel      = NULL;
static lv_obj_t*   s_list       = NULL;
static lv_obj_t*   s_slider     = NULL;
static lv_obj_t*   s_slider_lbl = NULL;
static bool        s_menu_open  = false;
static uint8_t     s_brightness = 80;
static lv_timer_t* s_save_timer = NULL;

static Preferences prefs;

extern void displaySetBrightness(uint8_t brightness);
extern volatile bool g_switchToPage1;
extern volatile bool g_switchToPage3A;
extern volatile bool g_switchToPage3B;
extern volatile uint8_t g_currentPage;

// ── Callbacks ────────────────────────────────────────────

static void ham_cb(lv_event_t* e) {
    if (s_menu_open) uiMenuClose();
    else             uiMenuOpen();
}

static void list_cb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    const char* txt = lv_list_get_button_text(s_list, btn);

    if (strcmp(txt, "Botones") == 0) {
        prefs.begin("uimenu", false);
        prefs.putUChar("lastPage", 1);
        prefs.end();
        g_currentPage   = 1;
        g_switchToPage1 = true;
        uiMenuClose();
    } else if (strcmp(txt, "VUMetros") == 0) {
        prefs.begin("uimenu", false);
        prefs.putUChar("lastPage", 0);
        prefs.end();
        g_currentPage    = 0;
        g_switchToPage3A = true;
        uiMenuClose();
    } else if (strcmp(txt, "Faders") == 0) {
        prefs.begin("uimenu", false);
        prefs.putUChar("lastPage", 2);
        prefs.end();
        g_currentPage    = 2;
        g_switchToPage3B = true;
        uiMenuClose();
    } else if (strcmp(txt, "Reiniciar") == 0) {
        ESP.restart();
    }
}

static void slider_cb(lv_event_t* e) {
    lv_obj_t* s = (lv_obj_t*)lv_event_get_target(e);
    s_brightness = (uint8_t)lv_slider_get_value(s);
    displaySetBrightness(s_brightness);
    lv_label_set_text_fmt(s_slider_lbl, "%d%%", s_brightness);

    if (s_save_timer) lv_timer_del(s_save_timer);
    s_save_timer = lv_timer_create([](lv_timer_t* t) {
        prefs.begin("uimenu", false);
        prefs.putUChar("brightness", s_brightness);
        prefs.end();
        lv_timer_del(t);
        s_save_timer = NULL;
    }, 500, NULL);
}

// ── Init ─────────────────────────────────────────────────

void uiMenuInit(lv_obj_t* parent) {
    prefs.begin("uimenu", false);
    s_brightness = prefs.getUChar("brightness", 80);
    prefs.end();
    displaySetBrightness(s_brightness);

    // ── Botón hamburguesa en el strip ────────────────────
    s_ham_btn = lv_obj_create(parent);
    lv_obj_set_pos(s_ham_btn, HAM_X, HAM_Y);
    lv_obj_set_size(s_ham_btn, HAM_SIZE, HAM_SIZE);
    lv_obj_set_style_bg_color(s_ham_btn, lv_color_hex(0x001080), 0);
    lv_obj_set_style_bg_opa(s_ham_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ham_btn, 0, 0);
    lv_obj_set_style_radius(s_ham_btn, 8, 0);
    lv_obj_set_style_pad_all(s_ham_btn, 0, 0);
    lv_obj_clear_flag(s_ham_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ham_btn, LV_OBJ_FLAG_CLICKABLE);

    s_ham_lbl = lv_label_create(s_ham_btn);
    lv_label_set_text(s_ham_lbl, LV_SYMBOL_LIST);
    lv_obj_set_style_text_color(s_ham_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_ham_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(s_ham_lbl);
    lv_obj_set_style_transform_rotation(s_ham_lbl, 900, 0);
    lv_obj_set_style_transform_pivot_x(s_ham_lbl, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(s_ham_lbl, LV_PCT(50), 0);

    lv_obj_add_event_cb(s_ham_btn, ham_cb, LV_EVENT_CLICKED, NULL);

    // ── Panel menú ───────────────────────────────────────
    s_panel = lv_obj_create(parent);
    lv_obj_set_pos(s_panel, PANEL_X, PANEL_Y);
    lv_obj_set_size(s_panel, PANEL_W, PANEL_H);
    lv_obj_set_style_bg_color(s_panel, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(s_panel, 241, 0);  // 241/255 ≈ 95%
    lv_obj_set_style_border_width(s_panel, 0, 0);
    lv_obj_set_style_radius(s_panel, 0, 0);
    lv_obj_set_style_pad_all(s_panel, 0, 0);
    lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_shadow_width(s_panel, 30, 0);
    lv_obj_set_style_shadow_color(s_panel, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(s_panel, LV_OPA_70, 0);
    lv_obj_set_style_shadow_offset_x(s_panel, 10, 0);
    lv_obj_set_style_transform_rotation(s_panel, 900, 0);
    lv_obj_set_style_transform_pivot_x(s_panel, PANEL_W / 2, 0);
    lv_obj_set_style_transform_pivot_y(s_panel, PANEL_H / 2, 0);
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);

    // ── Título ───────────────────────────────────────────
    lv_obj_t* title = lv_label_create(s_panel);
    lv_label_set_text(title, "General");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(title, 20, 16);

    // Separador bajo título
    lv_obj_t* sep = lv_obj_create(s_panel);
    lv_obj_set_pos(sep, 0, 44);
    lv_obj_set_size(sep, PANEL_W, 1);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x444466), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE);

    // ── Lista páginas ────────────────────────────────────
    s_list = lv_list_create(s_panel);
    lv_obj_set_pos(s_list, 0, 50);
    lv_obj_set_size(s_list, PANEL_W - SLIDER_W - 20, PANEL_H - 50);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_row(s_list, 4, 0);

    // Sección páginas
    lv_obj_t* sec1 = lv_list_add_text(s_list, "Páginas");
    lv_obj_set_style_text_color(sec1, lv_color_hex(0x8888AA), 0);
    lv_obj_set_style_text_font(sec1, &lv_font_montserrat_12, 0);

    lv_obj_t* btn;

    btn = lv_list_add_button(s_list, LV_SYMBOL_AUDIO,   "Botones");
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x252540), 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, list_cb, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_button(s_list, LV_SYMBOL_EYE_OPEN, "VUMetros");
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x252540), 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, list_cb, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_button(s_list, LV_SYMBOL_LIST,    "Faders");
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x252540), 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, list_cb, LV_EVENT_CLICKED, NULL);

    // Sección sistema
    lv_obj_t* sec2 = lv_list_add_text(s_list, "Sistema");
    lv_obj_set_style_text_color(sec2, lv_color_hex(0x8888AA), 0);
    lv_obj_set_style_text_font(sec2, &lv_font_montserrat_12, 0);

    btn = lv_list_add_button(s_list, LV_SYMBOL_POWER, "Reiniciar");
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x3A1010), 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xFF4444), 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, list_cb, LV_EVENT_CLICKED, NULL);

    // ── Slider brillo ────────────────────────────────────
    lv_obj_t* bright_lbl = lv_label_create(s_panel);
    lv_label_set_text(bright_lbl, "B");
    lv_obj_set_style_text_color(bright_lbl, lv_color_hex(0x8888AA), 0);
    lv_obj_set_pos(bright_lbl, PANEL_W - SLIDER_W - 14, PANEL_H - 20);

    s_slider = lv_slider_create(s_panel);
    lv_obj_set_pos(s_slider, PANEL_W - SLIDER_W - 10, 60);
    lv_obj_set_size(s_slider, SLIDER_W, SLIDER_H);
    lv_slider_set_range(s_slider, 2, 100);
    lv_slider_set_value(s_slider, s_brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(0x444466), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(0x6666AA), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(0xCCCCFF), LV_PART_KNOB);
    lv_obj_add_event_cb(s_slider, slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_slider_lbl = lv_label_create(s_panel);
    lv_label_set_text_fmt(s_slider_lbl, "%d%%", s_brightness);
    lv_obj_set_style_text_color(s_slider_lbl, lv_color_hex(0xCCCCFF), 0);
    lv_obj_set_style_text_font(s_slider_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(s_slider_lbl, PANEL_W - SLIDER_W - 14, 50);
}

// ── Open / Close / Destroy ───────────────────────────────

void uiMenuOpen() {
    if (!s_panel) return;
    s_menu_open = true;
    lv_obj_remove_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_panel);
    lv_obj_move_foreground(s_ham_btn);
    lv_label_set_text(s_ham_lbl, LV_SYMBOL_CLOSE);
}

void uiMenuClose() {
    if (!s_panel) return;
    s_menu_open = false;
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_ham_lbl, LV_SYMBOL_LIST);
}

void uiMenuDestroy() {
    s_menu_open = false;
    if (s_save_timer) {
        lv_timer_del(s_save_timer);
        s_save_timer = NULL;
    }
    if (s_panel) {
        lv_obj_del(s_panel);
        s_panel = NULL;
    }
    if (s_ham_btn) {
        lv_obj_del(s_ham_btn);
        s_ham_btn = NULL;
    }
    s_ham_lbl    = NULL;
    s_list       = NULL;
    s_slider     = NULL;
    s_slider_lbl = NULL;
}