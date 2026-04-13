// src/display/UIPage3B.cpp
#include "UIPage3B.h"
#include "UIMenu.h"
#include "../config.h"
#include "Display.h"
#include "lvgl.h"

// ── Dimensiones portrait 480×800 ─────────────────────────
#define P4_W        480
#define P4_H        800

#define FADER_X      0
#define FADER_W      220
#define TRACKNAME_X  (FADER_X + FADER_W)
#define TRACKNAME_W  35
#define AUTOMODE_X   (TRACKNAME_X + TRACKNAME_W)
#define AUTOMODE_W   50
#define MUTE_X       (AUTOMODE_X + AUTOMODE_W)
#define MUTE_W       50
#define PANORAMA_X   (MUTE_X + MUTE_W)
#define PANORAMA_W   55
#define HEADER_X     (PANORAMA_X + PANORAMA_W)

#define NUM_CH       8
#define CH_H         (P4_H / NUM_CH)

// ── Colores ───────────────────────────────────────────────
#define COLOR_BG         lv_color_hex(0x000000)
#define COLOR_HEADER     lv_color_hex(0x000050)
#define COLOR_MUTE_ON    lv_color_hex(0xFF0000)
#define COLOR_MUTE_OFF   lv_color_hex(0x400000)
#define COLOR_TRACK_BG   lv_color_hex(0x0F1218)
#define COLOR_TRACK_SEL  lv_color_hex(0x2A3040)

// Automode colors
#define COLOR_AUTO_READ  lv_color_hex(0x006600)
#define COLOR_AUTO_TOUCH lv_color_hex(0x0000AA)
#define COLOR_AUTO_LATCH lv_color_hex(0xAA6600)
#define COLOR_AUTO_WRITE lv_color_hex(0xAA0000)
#define COLOR_AUTO_OFF   lv_color_hex(0x333333)

// ── Widgets ───────────────────────────────────────────────
static lv_obj_t* s_screen           = NULL;
static lv_obj_t* s_timecode         = NULL;
static lv_obj_t* s_timecode_ghost   = NULL;
static lv_obj_t* s_track_bg[NUM_CH] = {};
static lv_obj_t* s_fader[NUM_CH]    = {};
static lv_obj_t* s_automode[NUM_CH] = {};
static lv_obj_t* s_automode_lbl[NUM_CH] = {};
static lv_obj_t* s_mute[NUM_CH]     = {};
static lv_obj_t* s_trackname[NUM_CH]= {};
static lv_obj_t* s_arc[NUM_CH]      = {};
static lv_obj_t* s_arc_lbl[NUM_CH]  = {};

static bool s_page3b_ready = false;

// Automode state per channel — 0=READ 1=TOUCH 2=LATCH 3=WRITE
static uint8_t s_automode_state[NUM_CH] = {};

// Mackie automode values:
// 0=READ 1=WRITE 2=TRIM 3=TOUCH 4=LATCH 5=OFF
static const char* AUTOMODE_LABELS[] = { "R", "W", "T", "TCH", "L", "OFF" };
static const lv_color_t AUTOMODE_COLORS[] = {
    lv_color_hex(0x006600),  // READ — verde
    lv_color_hex(0xAA0000),  // WRITE — rojo
    lv_color_hex(0x006600),  // TRIM — verde claro
    lv_color_hex(0x0000AA),  // TOUCH — azul
    lv_color_hex(0xAA6600),  // LATCH — naranja
    lv_color_hex(0x333333),  // OFF — gris
};
static const uint8_t AUTOMODE_NOTES[] = { 0x4A, 0x4D, 0x4E, 0x4B }; // READ TOUCH LATCH WRITE

extern void sendMIDIBytes(const byte* data, size_t len);
extern String formatBeatString();
extern String formatTimecodeString();
extern uint8_t g_channelAutoMode[8];

LV_FONT_DECLARE(lv_font_dseg7_44);

static void set_rotated(lv_obj_t* obj) {
    lv_obj_set_style_transform_rotation(obj, 900, 0);
    lv_obj_set_style_transform_pivot_x(obj, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(obj, LV_PCT(50), 0);
}

static lv_color_t automode_color(uint8_t state) {
    switch (state) {
        case 0: return COLOR_AUTO_READ;
        case 1: return COLOR_AUTO_TOUCH;
        case 2: return COLOR_AUTO_LATCH;
        case 3: return COLOR_AUTO_WRITE;
        default: return COLOR_AUTO_OFF;
    }
}

void uiPage3BCreate() {
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    // ── HEADER ───────────────────────────────────────────
    lv_obj_t* header = lv_obj_create(s_screen);
    lv_obj_set_pos(header, HEADER_X, 0);
    lv_obj_set_size(header, P4_W - HEADER_X, P4_H);
    lv_obj_set_style_bg_color(header, COLOR_HEADER, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_shadow_width(header, 15, 0);
    lv_obj_set_style_shadow_color(header, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(header, LV_OPA_70, 0);
    lv_obj_set_style_shadow_offset_x(header, -10, 0);
    lv_obj_set_style_shadow_offset_y(header, 0, 0);

    // ── TIMECODE ghost ────────────────────────────────────
    s_timecode_ghost = lv_label_create(s_screen);
    lv_label_set_text(s_timecode_ghost,
        (currentTimecodeMode == MODE_BEATS) ? "0. 0. 0. 000" : "00:00:00: 00");
    lv_obj_set_style_text_color(s_timecode_ghost, lv_color_hex(0x006666), 0);
    lv_obj_set_style_text_font(s_timecode_ghost, &lv_font_dseg7_44, 0);
    lv_obj_set_pos(s_timecode_ghost, 10, 10);

    // ── TIMECODE real ─────────────────────────────────────
    s_timecode = lv_label_create(s_screen);
    lv_label_set_text(s_timecode,
        (currentTimecodeMode == MODE_BEATS) ? "0. 0. 0. 000" : "00:00:00: 00");
    lv_obj_set_style_text_color(s_timecode, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_text_font(s_timecode, &lv_font_dseg7_44, 0);
    lv_obj_set_pos(s_timecode, 10, 10);

    lv_obj_update_layout(s_screen);
    int tw    = lv_obj_get_width(s_timecode);
    int th    = lv_obj_get_height(s_timecode);
    int pos_x = HEADER_X + (P4_W - HEADER_X) / 2 - tw / 2;
    int pos_y = P4_H / 2 - th / 2 + 15;

    lv_obj_set_pos(s_timecode_ghost, pos_x, pos_y);
    lv_obj_set_style_transform_rotation(s_timecode_ghost, 900, 0);
    lv_obj_set_style_transform_pivot_x(s_timecode_ghost, tw/2, 0);
    lv_obj_set_style_transform_pivot_y(s_timecode_ghost, th/2, 0);

    lv_obj_set_pos(s_timecode, pos_x, pos_y);
    lv_obj_set_style_transform_rotation(s_timecode, 900, 0);
    lv_obj_set_style_transform_pivot_x(s_timecode, tw/2, 0);
    lv_obj_set_style_transform_pivot_y(s_timecode, th/2, 0);

    // ── 8 CANALES ─────────────────────────────────────────
    for (int i = 0; i < NUM_CH; i++) {
        int y = i * CH_H;

        // FONDO
        s_track_bg[i] = lv_obj_create(s_screen);
        lv_obj_set_pos(s_track_bg[i], 0, y);
        lv_obj_set_size(s_track_bg[i], HEADER_X, CH_H - 1);
        lv_obj_set_style_bg_color(s_track_bg[i], COLOR_TRACK_BG, 0);
        lv_obj_set_style_border_width(s_track_bg[i], 0, 0);
        lv_obj_set_style_radius(s_track_bg[i], 0, 0);
        lv_obj_clear_flag(s_track_bg[i], LV_OBJ_FLAG_SCROLLABLE);

        // FADER — slider vertical
s_fader[i] = lv_slider_create(s_screen);
lv_obj_set_pos(s_fader[i], FADER_X + 20, y + 50);
lv_obj_set_size(s_fader[i], FADER_W - 20, CH_H - 8);
lv_slider_set_range(s_fader[i], 0, 16383);
lv_slider_set_value(s_fader[i], 0, LV_ANIM_OFF);
lv_obj_set_style_bg_color(s_fader[i], lv_color_hex(0x000000), LV_PART_MAIN);
lv_obj_set_style_bg_color(s_fader[i], lv_color_hex(0x888888), LV_PART_KNOB);
lv_obj_set_style_height(s_fader[i], 4, LV_PART_MAIN);
lv_obj_set_style_pad_top(s_fader[i], 15, LV_PART_KNOB);
lv_obj_set_style_pad_bottom(s_fader[i], 15, LV_PART_KNOB);
lv_obj_set_style_pad_left(s_fader[i], 15, LV_PART_KNOB);
lv_obj_set_style_pad_right(s_fader[i], 15, LV_PART_KNOB);
lv_obj_add_event_cb(s_fader[i], [](lv_event_t* e) {
    int ch = (int)(intptr_t)lv_event_get_user_data(e);
    byte sel[3] = { 0x90, (uint8_t)(0x18 + ch), 127 };
    sendMIDIBytes(sel, 3);
    lv_obj_t* sl = (lv_obj_t*)lv_event_get_target(e);
    int val = lv_slider_get_value(sl);
    byte lsb = val & 0x7F;
    byte msb = (val >> 7) & 0x7F;
    byte msg[3] = { (byte)(0xE0 + ch), lsb, msb };
    sendMIDIBytes(msg, 3);
}, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)i);

        // PANORAMA — arco
        s_arc[i] = lv_arc_create(s_screen);
        lv_obj_set_pos(s_arc[i],
                       PANORAMA_X + (PANORAMA_W - 40) / 2,
                       y + (CH_H - 40) / 2);
        lv_obj_set_size(s_arc[i], 40, 40);
        lv_arc_set_range(s_arc[i], -100, 100);
        lv_arc_set_value(s_arc[i], 0);
        lv_arc_set_bg_angles(s_arc[i], 135, 405);
        lv_arc_set_mode(s_arc[i], LV_ARC_MODE_SYMMETRICAL);
        lv_obj_set_style_arc_color(s_arc[i], lv_color_hex(0x00FF00), LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(s_arc[i], lv_color_hex(0x333333), LV_PART_MAIN);
        lv_obj_set_style_arc_width(s_arc[i], 4, LV_PART_MAIN);
        lv_obj_set_style_arc_width(s_arc[i], 4, LV_PART_INDICATOR);
        lv_obj_set_style_opa(s_arc[i], LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_remove_flag(s_arc[i], LV_OBJ_FLAG_CLICKABLE);
        set_rotated(s_arc[i]);

        s_arc_lbl[i] = lv_label_create(s_arc[i]);
        lv_label_set_text(s_arc_lbl[i], "C");
        lv_obj_set_style_text_color(s_arc_lbl[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(s_arc_lbl[i], &lv_font_montserrat_12, 0);
        lv_obj_center(s_arc_lbl[i]);

        // MUTE
        s_mute[i] = lv_obj_create(s_screen);
        lv_obj_set_pos(s_mute[i], MUTE_X + 4, y + 4);
        lv_obj_set_size(s_mute[i], MUTE_W - 8, CH_H - 8);
        lv_obj_set_style_bg_color(s_mute[i], COLOR_MUTE_OFF, 0);
        lv_obj_set_style_border_width(s_mute[i], 0, 0);
        lv_obj_set_style_radius(s_mute[i], 6, 0);
        lv_obj_clear_flag(s_mute[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_mute[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* mute_lbl = lv_label_create(s_mute[i]);
        lv_label_set_text(mute_lbl, "M");
        lv_obj_set_style_text_color(mute_lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(mute_lbl);
        set_rotated(mute_lbl);
        lv_obj_add_event_cb(s_mute[i], [](lv_event_t* e) {
            int ch = (int)(intptr_t)lv_event_get_user_data(e);
            byte msg[3] = { 0x90, (uint8_t)(0x10 + ch), 127 };
            sendMIDIBytes(msg, 3);
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        // AUTOMODE — botón cíclico READ/TOUCH/LATCH/WRITE
        s_automode[i] = lv_obj_create(s_screen);
        lv_obj_set_pos(s_automode[i], AUTOMODE_X + 4, y + 4);
        lv_obj_set_size(s_automode[i], AUTOMODE_W - 8, CH_H - 8);
        lv_obj_set_style_bg_color(s_automode[i], COLOR_AUTO_READ, 0);
        lv_obj_set_style_border_width(s_automode[i], 0, 0);
        lv_obj_set_style_radius(s_automode[i], 6, 0);
        lv_obj_clear_flag(s_automode[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_automode[i], LV_OBJ_FLAG_CLICKABLE);
        s_automode_lbl[i] = lv_label_create(s_automode[i]);
        lv_label_set_text(s_automode_lbl[i], "R");
        lv_obj_set_style_text_color(s_automode_lbl[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(s_automode_lbl[i]);
        set_rotated(s_automode_lbl[i]);
        lv_obj_add_event_cb(s_automode[i], [](lv_event_t* e) {
            int ch = (int)(intptr_t)lv_event_get_user_data(e);
            s_automode_state[ch] = (s_automode_state[ch] + 1) % 4;
            byte msg[3] = { 0x90, AUTOMODE_NOTES[s_automode_state[ch]], 127 };
            sendMIDIBytes(msg, 3);
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        // TRACK NAME
        lv_obj_t* tn_cont = lv_obj_create(s_screen);
        lv_obj_set_pos(tn_cont, TRACKNAME_X, y);
        lv_obj_set_size(tn_cont, TRACKNAME_W, CH_H);
        lv_obj_set_style_bg_opa(tn_cont, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(tn_cont, 0, 0);
        lv_obj_clear_flag(tn_cont, LV_OBJ_FLAG_SCROLLABLE);
        s_trackname[i] = lv_label_create(tn_cont);
        lv_label_set_text(s_trackname[i], "---");
        lv_obj_set_style_text_color(s_trackname[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(s_trackname[i], &lv_font_montserrat_14, 0);
        lv_obj_center(s_trackname[i]);
        set_rotated(s_trackname[i]);
    }

    needsTimecodeRedraw = true;
    needsButtonsRedraw  = true;

    uiMenuInit(s_screen);

    s_page3b_ready = true;
    lv_scr_load(s_screen);
}

void uiPage3BUpdate() {
    if (!s_page3b_ready) return;

    if (needsTimecodeRedraw) {
        String displayText = (currentTimecodeMode == MODE_BEATS)
                             ? formatBeatString()
                             : formatTimecodeString();
        lv_label_set_text(s_timecode, displayText.c_str());
        const char* ghost_text = (currentTimecodeMode == MODE_BEATS)
                                 ? "0. 0. 0.  000"
                                 : "00:00:00: 00";
        lv_label_set_text(s_timecode_ghost, ghost_text);
        needsTimecodeRedraw = false;
    }

    if (needsButtonsRedraw) {
    for (int i = 0; i < NUM_CH; i++) {
        lv_obj_set_style_bg_color(s_track_bg[i],
            selectStates[i] ? COLOR_TRACK_SEL : COLOR_TRACK_BG, 0);
        lv_obj_set_style_bg_color(s_mute[i],
            muteStates[i] ? COLOR_MUTE_ON : COLOR_MUTE_OFF, 0);
        uint8_t am = g_channelAutoMode[i];
        lv_obj_set_style_bg_color(s_automode[i], AUTOMODE_COLORS[am < 6 ? am : 0], 0);
        lv_label_set_text(s_automode_lbl[i], AUTOMODE_LABELS[am < 6 ? am : 0]);
        lv_label_set_text(s_trackname[i], trackNames[i].c_str());
        int pos = (int)(vpotValues[i] & 0x0F);
        int pan = ((pos - 6) * 100) / 6;
        lv_arc_set_value(s_arc[i], pan);
        char pan_txt[5];
        if (pos == 6)      snprintf(pan_txt, sizeof(pan_txt), "C");
        else if (pos > 6)  snprintf(pan_txt, sizeof(pan_txt), "R%d", pos - 6);
        else               snprintf(pan_txt, sizeof(pan_txt), "L%d", 6 - pos);
        lv_label_set_text(s_arc_lbl[i], pan_txt);
        int fval = (int)(faderPositions[i] * 16383.0f);
        lv_slider_set_value(s_fader[i], fval, LV_ANIM_OFF);
    }
    needsButtonsRedraw = false;
}
}

void uiPage3BDestroy() {
    if (s_screen) {
        lv_obj_del(s_screen);
        s_screen       = NULL;
        s_page3b_ready = false;
    }
} 