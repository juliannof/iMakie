// src/display/UIMenu.cpp
#include "UIMenu.h"
#include "../config.h"
#include "Display.h"
#include "lvgl.h"
#include <Arduino.h>
#include <Preferences.h>

// ── Dimensiones portrait ─────────────────────────────────
#define PANEL_X      0
#define PANEL_Y      500
#define PANEL_W      300
#define PANEL_H      300
#define HAM_X        427
#define HAM_Y        740
#define HAM_SIZE     50

#define SLIDER_W     20
#define SLIDER_H     (PANEL_H - 150)

// ── Estado ────────────────────────────────────────────────
static lv_obj_t* s_ham_btn    = NULL;
static lv_obj_t* s_menu_panel = NULL;
static lv_obj_t* s_list       = NULL;
static lv_obj_t* s_slider     = NULL;
static lv_obj_t* s_slider_lbl = NULL;
static bool      s_menu_open  = false;
static uint8_t   s_brightness = 80;
static uint8_t   s_last_page  = 0;

static Preferences prefs;

extern void displaySetBrightness(uint8_t brightness);
extern volatile bool g_switchToPage3A;
extern volatile bool g_switchToPage3B;
extern volatile uint8_t g_currentPage;

static void ham_cb(lv_event_t* e) {
    if (s_menu_open) uiMenuClose();
    else             uiMenuOpen();
}

static void list_cb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    const char* txt = lv_list_get_button_text(s_list, btn);

    if (strcmp(txt, "P1") == 0) {
        prefs.begin("uimenu", false);
        prefs.putUChar("lastPage", 1);
        prefs.end();
        uiMenuClose();
    } else if (strcmp(txt, "VUMetros") == 0) {
    prefs.begin("uimenu", false);
    prefs.putUChar("lastPage", 0);
    prefs.end();
    g_currentPage    = 0;
    g_switchToPage3A = true;
    uiMenuClose();
    } else if (strcmp(txt, "Fader") == 0) {
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

static lv_timer_t* s_save_timer = NULL;

static void slider_cb(lv_event_t* e) {
    lv_obj_t* s = (lv_obj_t*)lv_event_get_target(e);
    s_brightness = (uint8_t)lv_slider_get_value(s);
    displaySetBrightness(s_brightness);
    lv_label_set_text_fmt(s_slider_lbl, "%d", s_brightness);
    lv_obj_align_to(s_slider_lbl, s_slider, LV_ALIGN_OUT_TOP_MID, 0, -10);

    if (s_save_timer) lv_timer_del(s_save_timer);
    s_save_timer = lv_timer_create([](lv_timer_t* t) {
        prefs.begin("uimenu", false);
        prefs.putUChar("brightness", s_brightness);
        prefs.end();
        lv_timer_del(t);
        s_save_timer = NULL;
    }, 500, NULL);
}
void uiMenuInit(lv_obj_t* parent_screen) {
    // ── Leer NVS ─────────────────────────────────────────
    prefs.begin("uimenu", false);
    s_brightness = prefs.getUChar("brightness", 80);
    s_last_page  = prefs.getUChar("lastPage", 0);
    prefs.end();
    displaySetBrightness(s_brightness);

    // ── Botón hamburger ───────────────────────────────────
    s_ham_btn = lv_obj_create(parent_screen);
    lv_obj_set_pos(s_ham_btn, HAM_X, HAM_Y);
    lv_obj_set_size(s_ham_btn, HAM_SIZE, HAM_SIZE);
    lv_obj_set_style_bg_color(s_ham_btn, lv_color_hex(0x000050), 0);
    lv_obj_set_style_border_width(s_ham_btn, 0, 0);
    lv_obj_set_style_radius(s_ham_btn, 4, 0);
    lv_obj_clear_flag(s_ham_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ham_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(s_ham_btn, 0, 0);

    for (int i = 0; i < 3; i++) {
    lv_obj_t* line = lv_obj_create(s_ham_btn);
    lv_obj_set_pos(line, 8 + i * 11, 10);
    lv_obj_set_size(line, 3, 30);
    lv_obj_set_style_bg_color(line, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_radius(line, 1, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_add_event_cb(s_ham_btn, ham_cb, LV_EVENT_CLICKED, NULL);

    // ── Panel menú ────────────────────────────────────────
    s_menu_panel = lv_obj_create(parent_screen);
    lv_obj_set_pos(s_menu_panel, PANEL_X, PANEL_Y);
    lv_obj_set_size(s_menu_panel, PANEL_W, PANEL_H);
    lv_obj_set_style_bg_opa(s_menu_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_menu_panel, 0, 0);
    lv_obj_set_style_radius(s_menu_panel, 0, 0);
    lv_obj_clear_flag(s_menu_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_shadow_width(s_menu_panel, 20, 0);
    lv_obj_set_style_shadow_color(s_menu_panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(s_menu_panel, LV_OPA_50, 0);
    lv_obj_set_style_shadow_offset_y(s_menu_panel, -10, 0);
    lv_obj_set_style_transform_rotation(s_menu_panel, 900, 0);
    lv_obj_set_style_transform_pivot_x(s_menu_panel, PANEL_W/2, 0);
    lv_obj_set_style_transform_pivot_y(s_menu_panel, PANEL_H/2, 0);
    lv_obj_add_flag(s_menu_panel, LV_OBJ_FLAG_HIDDEN);

    // ── Lista ─────────────────────────────────────────────
    s_list = lv_list_create(s_menu_panel);
    lv_obj_set_pos(s_list, 0, 0);
    lv_obj_set_size(s_list, PANEL_W - SLIDER_W - 10, PANEL_H);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);

    

    lv_obj_t* header_txt = lv_list_add_text(s_list, "Paginas");
    lv_obj_set_style_pad_all(header_txt, 10, 0);
    
    lv_obj_set_style_pad_left(header_txt, 10, 0);

    lv_obj_t* btn;
    btn = lv_list_add_button(s_list, LV_SYMBOL_AUDIO, "P1");
    lv_obj_add_event_cb(btn, list_cb, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_button(s_list, LV_SYMBOL_SHUFFLE, "VUMetros");
    lv_obj_add_event_cb(btn, list_cb, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_button(s_list, LV_SYMBOL_LIST, "Fader");
    lv_obj_add_event_cb(btn, list_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* sys_txt = lv_list_add_text(s_list, "Sistema");
    lv_obj_set_style_pad_all(sys_txt, 10, 0);

    lv_obj_set_style_pad_left(sys_txt, 10, 0);

    btn = lv_list_add_button(s_list, LV_SYMBOL_POWER, "Reiniciar");
    lv_obj_set_style_text_color(btn, lv_color_hex(0xFF0000), 0);
    lv_obj_add_event_cb(btn, list_cb, LV_EVENT_CLICKED, NULL);

    // ── Slider brillo — derecha del panel ────────────────
    
    s_slider = lv_slider_create(s_menu_panel);
    lv_obj_set_pos(s_slider, PANEL_W - SLIDER_W - 5, 20);
    lv_obj_set_size(s_slider, SLIDER_W, SLIDER_H);
    lv_slider_set_range(s_slider, 2, 100);
    lv_slider_set_value(s_slider, s_brightness, LV_ANIM_ON);
    lv_obj_add_event_cb(s_slider, slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Label valor slider
    s_slider_lbl = lv_label_create(s_menu_panel);
    lv_label_set_text_fmt(s_slider_lbl, "%d", s_brightness);
    lv_obj_align_to(s_slider_lbl, s_slider, LV_ALIGN_OUT_TOP_MID, 0, -10);
}

void uiMenuOpen() {
    if (!s_menu_panel) return;
    s_menu_open = true;
    lv_obj_remove_flag(s_menu_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_menu_panel);
    lv_obj_move_foreground(s_ham_btn);
}

void uiMenuClose() {
    if (!s_menu_panel) return;
    s_menu_open = false;
    lv_obj_add_flag(s_menu_panel, LV_OBJ_FLAG_HIDDEN);
}

void uiMenuDestroy() {
    s_menu_open = false;
    if (s_menu_panel) {
        lv_obj_del(s_menu_panel);
        s_menu_panel = NULL;
    }
    if (s_ham_btn) {
        lv_obj_del(s_ham_btn);
        s_ham_btn = NULL;
    }
    s_list       = NULL;
    s_slider     = NULL;
    s_slider_lbl = NULL;
    s_save_timer = NULL;
}