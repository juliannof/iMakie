#pragma once
// ============================================================
//  SatMenu.h  –  iMakie PTxx Track S2
//  Sprite único 240×280 — sin flicker.
//  Fondo negro, texto blanco, acento rojo puro.
// ============================================================
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <Preferences.h>
#include "../config.h"

// ─── Colores RGB565 ───────────────────────────────────────────
#define C_BG      0x0000   // negro
#define C_TEXT    0xFFFF   // blanco
#define C_ACCENT  0xF800   // rojo puro
#define C_GRAY    0x4208   // gris medio
#define C_DARK    0x2104   // gris oscuro
#define C_BLACK   0x0000
#define C_WHITE   0xFFFF
#define C_GREEN   0x07E0
#define C_RED     0xF800
#define C_YELLOW  0xFFE0
#define C_BLUE    0x001F
#define C_CYAN    0x07FF
#define C_ORANGE  0xFD20

// ─── Layout ───────────────────────────────────────────────────
#define SAT_HDR_H   36
#define SAT_HINT_H  18
#define SAT_ROW_H   36
#define SAT_BADGE_W 28
#define SAT_BADGE_H 20
#define SAT_DEB_MS  160

// ─────────────────────────────────────────────────────────────
struct SatConfig {
    uint8_t trackId;
    char    label[7];
    uint8_t pwmMin;
    uint8_t pwmMax;
    bool    touchEnabled;
    uint8_t touchThreshold;
};

using CbVoid   = std::function<void()>;
using CbMotor  = std::function<void(int pwm)>;
using CbConfig = std::function<void(const SatConfig&)>;

// ─────────────────────────────────────────────────────────────
class SatMenu {
public:
    explicit SatMenu(LovyanGFX* tft);

    void onMotorOff       (CbVoid   cb) { _cbMotorOff   = cb; }
    void onMotorOn        (CbVoid   cb) { _cbMotorOn    = cb; }
    void onMotorDrive     (CbMotor  cb) { _cbMotorDrv   = cb; }
    void onRS485Off       (CbVoid   cb) { _cbRS485Off   = cb; }
    void onRS485On        (CbVoid   cb) { _cbRS485On    = cb; }
    void onLedsOff        (CbVoid   cb) { _cbLedsOff    = cb; }
    void onReboot         (CbVoid   cb) { _cbReboot     = cb; }
    void onWiFiConfig     (CbVoid   cb) { _cbWiFiConfig = cb; }
    void onWiFiOta        (CbVoid   cb) { _cbWiFiOta    = cb; }
    void onConfigSaved    (CbConfig cb) { _cbSaved      = cb; }
    void onSuspendSprites (CbVoid   cb) { _cbSuspend    = cb; }
    void onRestoreSprites (CbVoid   cb) { _cbRestore    = cb; }
    void onBrightness     (std::function<void(uint8_t)> cb) { _cbBrightness = cb; }

    void update();
    void open();
    void close();
    void showStatus(const char* msg);
    bool isOpen() const { return _open; }
    const SatConfig& getConfig() const { return _cfg; }

private:
    enum class Scr {
        MAIN, IDENTIDAD, MOTOR, TOUCH, DIAG,
        CONFIG_WIFI, WIFI, REINICIAR,
        EDIT_TRACKID, EDIT_LABEL,
        EDIT_PWMMIN, EDIT_PWMMAX, EDIT_TOUCHTHR,
        CONFIRM, TOAST,
        TEST_DISPLAY, TEST_ENCODER, TEST_FADER,
    };

    struct Item { const char* badge; const char* label; Scr target; };

    // ── Core ──────────────────────────────────────────────────
    LovyanGFX*   _tft;
    LGFX_Sprite  _spr;       // sprite único 240×280
    bool         _open  = false;
    Scr          _scr   = Scr::MAIN;
    Scr          _prev  = Scr::MAIN;
    int          _cur   = 0;
    int          _scrl  = 0;
    bool         _dirty = true;
    SatConfig    _cfg, _tmp;

    // ── Editores ─────────────────────────────────────────────
    int         _eVal=0, _eMin=0, _eMax=255;
    const char* _eTitle="";
    int         _lblIdx = 0;

    // ── Toast / Confirm ───────────────────────────────────────
    unsigned long _toastT   = 0;
    const char*   _toastMsg = "";
    Scr           _toastRet = Scr::MAIN;
    const char*   _confMsg  = "";
    Scr           _confYes  = Scr::MAIN;

    // ── Debounce ──────────────────────────────────────────────
    unsigned long _debT = 0;

    // ── NVS ───────────────────────────────────────────────────
    Preferences _prefs;

    // ── Callbacks ─────────────────────────────────────────────
    CbVoid   _cbMotorOff, _cbMotorOn, _cbRS485Off, _cbRS485On, _cbReboot, _cbLedsOff;
    CbVoid   _cbWiFiConfig, _cbWiFiOta, _cbSuspend, _cbRestore;
    CbMotor  _cbMotorDrv;
    CbConfig _cbSaved;
    std::function<void(uint8_t)> _cbBrightness;

    // ── Test Display ──────────────────────────────────────────
    int           _dPat   = 0;
    unsigned long _dAutoT = 0;

    // ── Test Encoder ──────────────────────────────────────────
    long          _encCnt     = 0;
    int           _encLastA   = HIGH;
    int           _encLastB   = HIGH;
    bool          _encSW      = false;
    unsigned long _encDebT    = 0;
    static const int ENC_HIST = 20;
    int8_t        _encHist[ENC_HIST] = {};
    int           _encHistIdx = 0;

    // ── Test Fader ────────────────────────────────────────────
    int           _fadRaw    = 0;
    float         _fadPct    = 0.0f;
    int           _tchRaw    = 0;
    bool          _tchOn     = false;
    int           _motPWM    = 0;
    int           _fadCalMin = 4095;
    int           _fadCalMax = 0;
    unsigned long _fadT      = 0;
    static const int FAD_HIST = 80;
    uint8_t       _fadHist[FAD_HIST] = {};
    int           _fadHistIdx = 0;
    unsigned long _fadHistT   = 0;

    // ── Btn enum ──────────────────────────────────────────────
    enum class Btn { NONE, UP, DOWN, BACK, ENTER };
    Btn _readBtn();

    // ── Render ────────────────────────────────────────────────
    void _render();
    void _push() { _spr.pushSprite(0, 0); }

    // ── Primitivas (todas sobre _spr) ─────────────────────────
    void _drawHdr(const char* t);
    void _drawHints(const char* u, const char* d, const char* b, const char* e);
    void _drawList(const Item* items, int n);
    void _drawValEdit(const char* t, int v, int mn, int mx, const char* u="");
    void _drawLblEdit();
    void _drawConfirm(const char* msg);
    void _drawToast(const char* msg);
    void _drawBadge(int x, int y, const char* t, uint16_t bg, uint16_t fg);
    void _drawHBar(int x, int y, int w, int h, float pct, uint16_t col);
    void _drawDivider(int y);

    // ── Handlers ──────────────────────────────────────────────
    void _hMain(Btn b);
    void _hIdent(Btn b);
    void _hMotor(Btn b);
    void _hTouch(Btn b);
    void _hDiag(Btn b);
    void _hWifi(Btn b);
    void _hEditVal(Btn b);
    void _hEditLbl(Btn b);
    void _hConfirm(Btn b);
    void _hToast(Btn b);

    // ── Tests (live) ──────────────────────────────────────────
    void _tickTestDisplay(Btn b);
    void _tickTestEncoder(Btn b);
    void _tickTestFader(Btn b);

    // ── Motor ─────────────────────────────────────────────────
    void _motorStop();
    void _motorDrive(int pwm);

    // ── NVS ───────────────────────────────────────────────────
    void _load();
    void _save();

    void _goto(Scr s) { _prev=_scr; _scr=s; _dirty=true; _cur=0; _scrl=0; }
    void _back()      { _goto(_prev); }
    void _toast(const char* msg, Scr ret);
    void _confirm(const char* msg, Scr yes);

    static const Item _mainItems[];
    static const Item _identItems[];
    static const Item _motorItems[];
    static const Item _touchItems[];
    static const Item _diagItems[];
    static const Item _wifiItems[];
    static const int  _mainN, _identN, _motorN, _touchN, _diagN, _wifiN;
};