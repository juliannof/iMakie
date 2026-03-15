// ============================================================
//  ButtonManager.cpp  —  iMakie PTxx Track S2
// ============================================================
#include "ButtonManager.h"
#include "../menu/SatMenu.h"
#include "../display/Display.h"
#include "../protocol.h"          // FLAG_REC, FLAG_SOLO, FLAG_MUTE, FLAG_SELECT

// ─── Variables de canal (definidas en main.cpp) ───────────────
extern bool recStates, soloStates, muteStates, selectStates;
extern bool needsMainAreaRedraw, needsHeaderRedraw;
extern void handleButtonLedState(ButtonId id);

// ─── Button2 instances (definidas en Hardware.cpp) ───────────
extern Button2 buttonRec;

namespace ButtonManager {

// ─── Privado ──────────────────────────────────────────────────
static LovyanGFX* _tft      = nullptr;
static SatMenu*   _sat      = nullptr;
static uint8_t    _flags    = 0;

static uint8_t _encoderBtnCount = 0;

// Long-press state
static bool          _holding    = false;
static unsigned long _holdStart  = 0;
static int           _lastBarW   = -1;
static bool          _fired      = false;

static constexpr unsigned long HOLD_MS   = 1000;
static constexpr int           BAR_H     = 6;
static constexpr int           BAR_MAR   = 16;
static constexpr int           LABEL_H   = 14;
static constexpr uint16_t      COL_ACCENT = 0xE94A;
static constexpr uint16_t      COL_TRACK  = 0x4208;
static constexpr uint16_t      COL_BG     = 0x0000;
static constexpr uint16_t      COL_TEXT   = 0xFFFF;

// ─── Dibujo barra de progreso ─────────────────────────────────
static void _drawBar(float pct) {
    if (!_tft) return;
    int bx = BAR_MAR;
    int bw = _tft->width() - BAR_MAR * 2;
    int by = _tft->height() - BAR_H - 4;
    int fill = (int)(bw * pct);

    if (fill == _lastBarW) return;
    _lastBarW = fill;

    if (pct == 0.0f) {
        _tft->setTextColor(COL_TEXT, COL_BG);
        _tft->setTextSize(1);
        _tft->setTextDatum(textdatum_t::middle_center);
        _tft->drawString("Mantener para menu SAT...",
                         _tft->width() / 2, by - LABEL_H / 2 - 1);
    }

    _tft->fillRect(bx, by, bw, BAR_H, COL_TRACK);
    if (fill > 0)
        _tft->fillRect(bx, by, fill, BAR_H, COL_ACCENT);
}

static void _clearBar() {
    if (!_tft) return;
    int by = _tft->height() - BAR_H - 4;
    _tft->fillRect(0, by - LABEL_H - 2,
                   _tft->width(), BAR_H + LABEL_H + 8, COL_BG);
    _lastBarW = -1;
}

// ─── Callbacks Button2 para REC ───────────────────────────────
// Button2 llama a pressed cuando el pin baja
static void _onRecPressed(Button2& btn) {
    (void)btn;
    if (_sat && _sat->isOpen()) return;
    _holding   = true;
    _fired     = false;
    _holdStart = millis();
    _lastBarW  = -1;
    _drawBar(0.0f);
}

// Button2 llama a released cuando el pin sube
static void _onRecReleased(Button2& btn) {
    (void)btn;
    if (!_holding) return;

    unsigned long held = millis() - _holdStart;
    _holding = false;

    if (_fired) {
        // El SAT ya se abrió — no hacer nada más
        _fired = false;
        return;
    }

    // Short press (<1 s): comportamiento normal de REC
    _clearBar();
    if (_sat && _sat->isOpen()) return;

    if (held < HOLD_MS) {
        recStates = !recStates;
        handleButtonLedState(ButtonId::REC);
        needsMainAreaRedraw = true;
        if (recStates) _flags |=  FLAG_REC;
        else           _flags &= ~FLAG_REC;
    }
}

// ─── Callback genérico para los demás botones ─────────────────
static void _onButtonEvent(ButtonId id) {
    if (_sat && _sat->isOpen()) return;

    switch (id) {
        case ButtonId::SOLO:
            soloStates = !soloStates;
            handleButtonLedState(ButtonId::SOLO);
            needsMainAreaRedraw = true;
            if (soloStates) _flags |=  FLAG_SOLO;
            else            _flags &= ~FLAG_SOLO;
            break;
        case ButtonId::MUTE:
            muteStates = !muteStates;
            handleButtonLedState(ButtonId::MUTE);
            needsMainAreaRedraw = true;
            if (muteStates) _flags |=  FLAG_MUTE;
            else            _flags &= ~FLAG_MUTE;
            break;
        case ButtonId::SELECT:
            selectStates = !selectStates;
            handleButtonLedState(ButtonId::SELECT);
            needsHeaderRedraw = true;
            if (selectStates) _flags |=  FLAG_SELECT;
            else              _flags &= ~FLAG_SELECT;
            break;
        case ButtonId::ENCODER_SELECT:
            _encoderBtnCount++;
            Encoder::reset();
            needsVPotRedraw = true;
            break;
        default: break;
    }
}

// ─── API pública ──────────────────────────────────────────────
void begin(LovyanGFX* tft, SatMenu* sat) {
    _tft   = tft;
    _sat   = sat;
    _flags = 0;

    // REC: gestión manual via pressed/released para long-press
    buttonRec.setPressedHandler (_onRecPressed);
    buttonRec.setReleasedHandler(_onRecReleased);

    // El resto via callback genérico de Hardware
    // (REC ya no llega aquí porque tiene sus propios handlers)
    registerButtonEventCallback(_onButtonEvent);
}

void update() {
    // Progreso del long-press de REC mientras se mantiene
    if (!_holding || _fired) return;
    if (_sat && _sat->isOpen()) { _holding = false; _clearBar(); return; }

    unsigned long elapsed = millis() - _holdStart;
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
uint8_t getEncoderButton()  { return _encoderBtnCount; }
void    clearEncoderButton(){ _encoderBtnCount = 0; }

} // namespace ButtonManager