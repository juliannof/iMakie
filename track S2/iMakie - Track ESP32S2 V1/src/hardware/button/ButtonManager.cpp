// ============================================================
//  ButtonManager.cpp  —  iMakie PTxx Track S2
// ============================================================
#include "ButtonManager.h"
#include "SAT/SatMenu.h"
#include "display/Display.h"
#include "protocol.h"

extern bool recStates, soloStates, muteStates, selectStates;
extern bool needsMainAreaRedraw, needsHeaderRedraw;
extern void handleButtonLedState(ButtonId id);
extern Button2 buttonRec;

namespace ButtonManager {

static LovyanGFX* _tft   = nullptr;
static SatMenu*   _sat   = nullptr;
static uint8_t    _flags = 0;
static uint8_t    _encoderBtnCount = 0;

static bool          _holding   = false;
static unsigned long _holdStart = 0;
static int           _lastBarW  = -1;
static bool          _fired     = false;

static constexpr unsigned long HOLD_MS     = 1000;
static constexpr unsigned long BAR_SHOW_MS = 300;    // esperar antes de mostrar barra
static constexpr uint16_t      COL_ACCENT = 0xF800;   // rojo puro
static constexpr uint16_t      COL_TRACK  = 0x2104;   // gris oscuro
static constexpr uint16_t      COL_BG     = 0x0000;
static constexpr uint16_t      COL_TEXT   = 0xFFFF;

// Barra centrada en pantalla — no interfiere con ningún sprite
static constexpr int BAR_W  = 180;
static constexpr int BAR_H  = 8;
static constexpr int BAR_CX = 120;   // centro 240px
static constexpr int BAR_CY = 140;   // centro 280px
static constexpr int LABEL_Y = BAR_CY - 16;

// ─────────────────────────────────────────────────────────────
static void _drawBar(float pct) {
    if (!_tft) return;
    if (_sat && _sat->isOpen()) return;   // SAT gestiona su propia pantalla

    int bx   = BAR_CX - BAR_W / 2;
    int fill = (int)(BAR_W * pct);
    if (fill == _lastBarW) return;
    _lastBarW = fill;

    if (pct == 0.0f) {
        _tft->setTextColor(COL_TEXT, COL_BG);
        _tft->setTextSize(1);
        _tft->setTextDatum(textdatum_t::middle_center);
        _tft->drawString("Mantener para SAT...", BAR_CX, LABEL_Y);
    }
    _tft->fillRect(bx, BAR_CY, BAR_W, BAR_H, COL_TRACK);
    if (fill > 0)
        _tft->fillRect(bx, BAR_CY, fill, BAR_H, COL_ACCENT);
}

static void _clearBar() {
    if (!_tft) return;
    _tft->fillRect(BAR_CX - BAR_W/2 - 2, LABEL_Y - 8,
                   BAR_W + 4, BAR_H + 28, COL_BG);
    _lastBarW = -1;
}

// ─────────────────────────────────────────────────────────────
static void _onRecPressed(Button2& btn) {
    (void)btn;
    if (_sat && _sat->isOpen()) return;
    _holding   = true;
    _fired     = false;
    _holdStart = millis();
    _lastBarW  = -1;
    // No dibujar nada aún — update() esperará BAR_SHOW_MS
}

static void _onRecReleased(Button2& btn) {
    (void)btn;
    if (!_holding) return;
    unsigned long held = millis() - _holdStart;
    _holding = false;

    if (_fired) { _fired = false; return; }

    _clearBar();
    if (_sat && _sat->isOpen()) return;

    static unsigned long lastRecTime = 0;
    unsigned long now = millis();
    if (held < HOLD_MS) {
        if (now - lastRecTime >= 300) {
            lastRecTime = now;
            _flags |= FLAG_REC;
        }
    }
}

static void _onButtonEvent(ButtonId id) {
    if (_sat && _sat->isOpen()) return;

    static unsigned long lastRecTime    = 0;
    static unsigned long lastSoloTime   = 0;
    static unsigned long lastMuteTime   = 0;
    static unsigned long lastSelectTime = 0;
    static constexpr unsigned long DEBOUNCE_MS = 300;

    unsigned long now = millis();

    switch (id) {
        case ButtonId::SOLO:
            if (now - lastSoloTime < DEBOUNCE_MS) break;
            lastSoloTime = now;
            _flags |= FLAG_SOLO;
            break;
        case ButtonId::MUTE:
            if (now - lastMuteTime < DEBOUNCE_MS) break;
            lastMuteTime = now;
            _flags |= FLAG_MUTE;
            break;
        case ButtonId::SELECT:
            if (now - lastSelectTime < DEBOUNCE_MS) break;
            lastSelectTime = now;
            _flags |= FLAG_SELECT;
            break;
        case ButtonId::ENCODER_SELECT:
            _encoderBtnCount++;
            Encoder::reset();
            needsVPotRedraw = true;
            break;
        default: break;
    }
}

// ─────────────────────────────────────────────────────────────
void begin(LovyanGFX* tft, SatMenu* sat) {
    _tft   = tft;
    _sat   = sat;
    _flags = 0;
    buttonRec.setPressedHandler (_onRecPressed);
    buttonRec.setReleasedHandler(_onRecReleased);
    registerButtonEventCallback (_onButtonEvent);
}

void update() {
    if (!_holding || _fired) return;
    if (_sat && _sat->isOpen()) { _holding = false; _clearBar(); return; }

    unsigned long elapsed = millis() - _holdStart;

    // No mostrar nada hasta BAR_SHOW_MS — short press no deja rastro
    if (elapsed < BAR_SHOW_MS) return;

    float pct = constrain((float)elapsed / HOLD_MS, 0.0f, 1.0f);
    _drawBar(pct);

    if (elapsed >= HOLD_MS) {
        _fired   = true;
        _holding = false;
        _clearBar();
        if (_sat && !_sat->isOpen()) _sat->open();
    }
}

void setSatMenu(SatMenu* sat) { _sat = sat; }
uint8_t getButtonFlags()      { return _flags; }
void    clearButtonFlags()    { _flags = 0; }
uint8_t getEncoderButton()    { return _encoderBtnCount; }
void    clearEncoderButton()  { _encoderBtnCount = 0; }

} // namespace ButtonManager