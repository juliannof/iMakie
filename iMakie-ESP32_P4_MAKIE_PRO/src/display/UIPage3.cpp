#include "UIPage3.h"
#include "../config.h"
#include "Display.h"
#include "lvgl.h"

// ── Dimensiones portrait 480×800 ─────────────────────────
#define P4_W            480
#define P4_H            800
#define SLIDER_W         30
#define CONTENT_W       (P4_W - SLIDER_W)  // 450px

// Franjas — ancho en portrait = alto para nosotros
#define VU_X         0
#define VU_W         220
#define TRACKNAME_X  (VU_X + VU_W)          // 110
#define TRACKNAME_W  35
#define SELECT_X     (TRACKNAME_X + TRACKNAME_W)  // 150
#define SELECT_W     50
#define MUTE_X       (SELECT_X + SELECT_W)   // 210
#define MUTE_W       50
#define PANORAMA_X   (MUTE_X + MUTE_W)       // 270
#define PANORAMA_W   55
#define HEADER_X     (PANORAMA_X + PANORAMA_W) // 370
#define HEADER_W     70



#define NUM_CH           8
#define CH_H             (P4_H / NUM_CH)             // 100px por canal

// ── Colores ───────────────────────────────────────────────
#define COLOR_BG        lv_color_hex(0x000000)
#define COLOR_HEADER    lv_color_hex(0x000050)
#define COLOR_MUTE_ON   lv_color_hex(0xFF0000)
#define COLOR_MUTE_OFF  lv_color_hex(0x400000)
#define COLOR_SEL_ON    lv_color_hex(0x888888)
 #define COLOR_SEL_OFF   lv_color_hex(0x333333)

#define COLOR_TRACK_BG       lv_color_hex(0x0F1218)
#define COLOR_TRACK_SEL      lv_color_hex(0x2A3040)
#define COLOR_TRACK_SEP      lv_color_hex(0x111111)

// ── Widgets ───────────────────────────────────────────────

static lv_obj_t* s_track_bg[NUM_CH] = {};
static lv_obj_t* s_screen        = NULL;
static lv_obj_t* s_timecode      = NULL;
static lv_obj_t* s_timecode_ghost = NULL;  // ← aquí
static lv_obj_t* s_mute[NUM_CH]      = {};
static lv_obj_t* s_select[NUM_CH]    = {};
static lv_obj_t* s_trackname[NUM_CH] = {};
static lv_obj_t* s_arc[NUM_CH]       = {};
static lv_obj_t* s_vu[NUM_CH]        = {};
// VU — 12 segmentos
static lv_obj_t* s_vu_seg[NUM_CH][12] = {};

static lv_obj_t* s_slider_panel      = NULL;
static lv_obj_t* s_slider            = NULL;
static bool      s_slider_visible    = false;

static bool s_page3_ready = false;


extern void sendMIDIBytes(const byte* data, size_t len);
extern String formatBeatString();
extern String formatTimecodeString();

// ── Helper: rotar label 90° ───────────────────────────────
static void set_rotated(lv_obj_t* obj) {
    lv_obj_set_style_transform_rotation(obj, 900, 0);
    lv_obj_set_style_transform_pivot_x(obj, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(obj, LV_PCT(50), 0);
}

static lv_obj_t* s_arc_lbl[NUM_CH] = {};

LV_FONT_DECLARE(lv_font_dseg7_44);


void uiPage3Create() {
    s_screen = lv_obj_create(NULL);
lv_obj_set_style_bg_color(s_screen, COLOR_BG, 0);
lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
lv_obj_set_style_pad_all(s_screen, 0, 0);
lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

// ── HEADER — franja X=330, W=120, H=800 ──────────────────
lv_obj_t* header = lv_obj_create(s_screen);
lv_obj_set_pos(header, HEADER_X, 0);
lv_obj_set_size(header, P4_W - HEADER_X, P4_H);
lv_obj_set_style_bg_color(header, COLOR_HEADER, 0);
lv_obj_set_style_border_width(header, 0, 0);
lv_obj_set_style_radius(header, 0, 0);
lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);


// ── TIMECODE — ghost primero (detrás) ────────────────────
//static lv_obj_t* s_timecode_ghost = NULL;
s_timecode_ghost = lv_label_create(s_screen);
//lv_label_set_text(s_timecode_ghost, "00:00:00: 00");
//lv_label_set_text(s_timecode_ghost, "0. 0. 0.  000");
lv_label_set_text(s_timecode_ghost,
    (currentTimecodeMode == MODE_BEATS) ? "0. 0. 0. 000" : "00:00:00: 00");
lv_obj_set_style_text_color(s_timecode_ghost, lv_color_hex(0x006666), 0);

lv_obj_set_style_text_font(s_timecode_ghost, &lv_font_dseg7_44, 0);
lv_obj_set_pos(s_timecode_ghost, 10, 10);

// ── TIMECODE — real (encima) ─────────────────────────────
s_timecode = lv_label_create(s_screen);
//lv_label_set_text(s_timecode, "00:00:00:00");
lv_label_set_text(s_timecode,
    (currentTimecodeMode == MODE_BEATS) ? "0. 0. 0. 000" : "00:00:00: 00");
lv_obj_set_style_text_color(s_timecode, lv_color_hex(0x00FFFF), 0);
lv_obj_set_style_text_font(s_timecode, &lv_font_dseg7_44, 0);
lv_obj_set_pos(s_timecode, 10, 10);

lv_obj_update_layout(s_screen);

int tw    = lv_obj_get_width(s_timecode);
int th    = lv_obj_get_height(s_timecode);
int pos_x = HEADER_X + HEADER_W/2 - tw/2;
int pos_y = P4_H/2 - th/2 + 15;

log_e("timecode tw=%d th=%d pos_x=%d pos_y=%d", tw, th, pos_x, pos_y);

// Posicionar y rotar ambos igual
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
        int y = i * CH_H;  // Y de cada canal

    // FONDO DE PISTA — primero para que quede detrás
    s_track_bg[i] = lv_obj_create(s_screen);
    lv_obj_set_pos(s_track_bg[i], 0, y);
    lv_obj_set_size(s_track_bg[i], HEADER_X, CH_H - 1);
    lv_obj_set_style_bg_color(s_track_bg[i], COLOR_TRACK_BG, 0);
    lv_obj_set_style_border_width(s_track_bg[i], 0, 0);
    lv_obj_set_style_radius(s_track_bg[i], 0, 0);
    lv_obj_clear_flag(s_track_bg[i], LV_OBJ_FLAG_SCROLLABLE);

    // PANORAMA — arco
    s_arc[i] = lv_arc_create(s_screen);
    lv_obj_set_pos(s_arc[i],
               PANORAMA_X + (PANORAMA_W - 40) / 2,
               y + (CH_H - 40) / 2);
    lv_obj_set_size(s_arc[i], 40, 40);
    lv_arc_set_range(s_arc[i], -100, 100);
    lv_arc_set_value(s_arc[i], 0);
    lv_arc_set_bg_angles(s_arc[i], 135, 405);  // 405 = 45+360 para cubrir el lado derecho
    lv_arc_set_mode(s_arc[i], LV_ARC_MODE_SYMMETRICAL);
    lv_obj_set_style_arc_color(s_arc[i], lv_color_hex(0x00FF00), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc[i], lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc[i], 4, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc[i], 4, LV_PART_INDICATOR);
    lv_obj_set_style_opa(s_arc[i], LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_remove_flag(s_arc[i], LV_OBJ_FLAG_CLICKABLE);
    set_rotated(s_arc[i]);

    // Label valor dentro del arco

    s_arc_lbl[i] = lv_label_create(s_arc[i]);
    lv_label_set_text(s_arc_lbl[i], "C");  // C = center
    lv_obj_set_style_text_color(s_arc_lbl[i], lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(s_arc_lbl[i], &lv_font_montserrat_12, 0);
    lv_obj_center(s_arc_lbl[i]);
    lv_obj_set_style_text_color(s_arc_lbl[i], lv_color_hex(0xFFFFFF), 0);


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
        uint8_t note = 0x10 + ch;
        byte msg[3] = { 0x90, note, 127 };
        sendMIDIBytes(msg, 3);
    }, LV_EVENT_CLICKED, (void*)(intptr_t)i);

    // SELECT
    s_select[i] = lv_obj_create(s_screen);
    lv_obj_set_pos(s_select[i], SELECT_X + 4, y + 4);
    lv_obj_set_size(s_select[i], SELECT_W - 8, CH_H - 8);
    lv_obj_set_style_bg_color(s_select[i], COLOR_SEL_OFF, 0);
    lv_obj_set_style_border_width(s_select[i], 0, 0);
    lv_obj_set_style_radius(s_select[i], 6, 0);
    lv_obj_clear_flag(s_select[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_select[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* sel_lbl = lv_label_create(s_select[i]);
    lv_label_set_text(sel_lbl, "S");
    lv_obj_set_style_text_color(sel_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(sel_lbl);
    set_rotated(sel_lbl);
    lv_obj_add_event_cb(s_select[i], [](lv_event_t* e) {
        int ch = (int)(intptr_t)lv_event_get_user_data(e);
        uint8_t note = 0x18 + ch;
        bool isOn = !selectStates[ch];
        byte msg[3] = { (byte)(isOn ? 0x90 : 0x80), note, (byte)(isOn ? 127 : 0) };
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

    // VU — 12 segmentos
    int seg_w   = (VU_W - 8 - 11) / 12;  // ancho de cada segmento horizontal
    int seg_pad = 1;
    for (int s = 0; s < 12; s++) {
        int seg_x = VU_X + 4 + s * (seg_w + seg_pad);
        s_vu_seg[i][s] = lv_obj_create(s_screen);
        lv_obj_set_pos(s_vu_seg[i][s], seg_x, y + 4);
        lv_obj_set_size(s_vu_seg[i][s], seg_w, CH_H - 8);
        lv_obj_set_style_border_width(s_vu_seg[i][s], 0, 0);
        lv_obj_set_style_radius(s_vu_seg[i][s], 1, 0);
        lv_obj_clear_flag(s_vu_seg[i][s], LV_OBJ_FLAG_SCROLLABLE);
        lv_color_t off_color;
        if      (s < 8)  off_color = lv_color_hex(0x003300);
        else if (s < 10) off_color = lv_color_hex(0x333300);
        else             off_color = lv_color_hex(0x330000);
        lv_obj_set_style_bg_color(s_vu_seg[i][s], off_color, 0);

        lv_obj_add_flag(s_vu_seg[i][s], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_vu_seg[i][s], [](lv_event_t* e) {
        int ch = (int)(intptr_t)lv_event_get_user_data(e);
        byte msg[3] = { 0x90, (uint8_t)(0x18 + ch), 127 };
        sendMIDIBytes(msg, 3);
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
    }


    needsTimecodeRedraw = true;
    needsButtonsRedraw  = true;

    s_page3_ready = true;

    lv_scr_load(s_screen);
}

void uiPage3Update() {

    if (!s_page3_ready) return;
    if (needsTimecodeRedraw) {
    String displayText = (currentTimecodeMode == MODE_BEATS)
                         ? formatBeatString()
                         : formatTimecodeString();
    lv_label_set_text(s_timecode, displayText.c_str());
    const char* ghost_text = (currentTimecodeMode == MODE_BEATS)
                                 ? "0. 0. 0.  000"
                                 : "00:00:00: 00";
    log_e("ghost mode=%d text=%s", currentTimecodeMode, ghost_text);
    lv_label_set_text(s_timecode_ghost, ghost_text);
    lv_obj_invalidate(s_screen);
    
    needsTimecodeRedraw = false;
} 

    if (needsButtonsRedraw) {
    for (int i = 0; i < NUM_CH; i++) {
        lv_obj_set_style_bg_color(s_track_bg[i],
            selectStates[i] ? COLOR_TRACK_SEL : COLOR_TRACK_BG, 0);
        lv_obj_set_style_bg_color(s_mute[i],
            muteStates[i] ? COLOR_MUTE_ON : COLOR_MUTE_OFF, 0);
        lv_obj_set_style_bg_color(s_select[i],
            selectStates[i] ? COLOR_SEL_ON : COLOR_SEL_OFF, 0);
        lv_label_set_text(s_trackname[i], trackNames[i].c_str());
        int pos = (int)(vpotValues[i] & 0x0F);
        int pan = ((pos - 6) * 100) / 6;
        lv_arc_set_value(s_arc[i], pan);
        char pan_txt[5];
        if (pos == 6)      snprintf(pan_txt, sizeof(pan_txt), "C");
        else if (pos > 6)  snprintf(pan_txt, sizeof(pan_txt), "R%d", pos - 6);
        else               snprintf(pan_txt, sizeof(pan_txt), "L%d", 6 - pos);
        lv_label_set_text(s_arc_lbl[i], pan_txt);
    }
    needsButtonsRedraw = false;
}

    if (needsVUMetersRedraw) {
        for (int i = 0; i < NUM_CH; i++) {
            int activeSegments = (int)round(vuLevels[i] * 12.0f);
            int peakSeg = (int)round(vuPeakLevels[i] * 12.0f) - 1;
            for (int s = 0; s < 12; s++) {
                lv_color_t color;
                if (s == peakSeg && vuPeakLevels[i] > vuLevels[i] + 0.001f)
                    color = lv_color_hex(0xB4B4B4);
                else if (s < activeSegments) {
                    if      (s < 8)  color = lv_color_hex(0x00E600);
                    else if (s < 10) color = lv_color_hex(0xFFFF00);
                    else             color = lv_color_hex(0xFF0000);
                } else {
                    if      (s < 8)  color = lv_color_hex(0x003300);
                    else if (s < 10) color = lv_color_hex(0x333300);
                    else             color = lv_color_hex(0x330000);
                }
                lv_obj_set_style_bg_color(s_vu_seg[i][s], color, 0);
            }
        }
        needsVUMetersRedraw = false;
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

void uiPage3Destroy() {
    if (s_screen) {
        lv_obj_del(s_screen);
        s_screen = NULL;
        s_page3_ready = false;
    }
}