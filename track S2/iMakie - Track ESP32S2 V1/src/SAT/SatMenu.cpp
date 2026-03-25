// ============================================================
//  SatMenu.cpp  –  iMakie PTxx Track S2
//  Sprite único 240×280. Todo dibujo va a _spr.
//  Al final de cada frame: _push() → pushSprite(0,0).
// ============================================================
#include "SatMenu.h"

// ─────────────────────────────────────────────────────────────
//  Tablas de menú
// ─────────────────────────────────────────────────────────────
const SatMenu::Item SatMenu::_mainItems[] = {
    {"ID","Identidad",   Scr::IDENTIDAD   },
    {"OT","Activar OTA", Scr::OTA         },
    {"MT","Motor",       Scr::MOTOR       },
    {"TC","Touch",       Scr::TOUCH       },
    {"DG","Diagnostico", Scr::DIAG        },
    {"WF","Config WiFi", Scr::CONFIG_WIFI },
    {"RB","Reiniciar",   Scr::REINICIAR   },
};
const SatMenu::Item SatMenu::_identItems[] = {
    {"ID","Track ID (1-8)",   Scr::EDIT_TRACKID},
    {"LB","Etiqueta (6 chr)", Scr::EDIT_LABEL  },
};
const SatMenu::Item SatMenu::_motorItems[] = {
    {"MN","PWM Minimo",  Scr::EDIT_PWMMIN },
    {"MX","PWM Maximo",  Scr::EDIT_PWMMAX },
};
const SatMenu::Item SatMenu::_touchItems[] = {
    {"EN","Habilitado",  Scr::TOUCH        },
    {"TH","Umbral %",    Scr::EDIT_TOUCHTHR},
};
const SatMenu::Item SatMenu::_diagItems[] = {
    {"DP","Test Display",  Scr::TEST_DISPLAY  },
    {"EC","Test Encoder",  Scr::TEST_ENCODER  },
    {"FD","Test Fader",    Scr::TEST_FADER    },
    {"NP","Test Botones",  Scr::TEST_NEOPIXEL },
};
const SatMenu::Item SatMenu::_wifiItems[] = {
    {"CF","Configurar red", Scr::WIFI},
    {"OT","Activar OTA",    Scr::WIFI},
};
const int SatMenu::_mainN  = 7;
const int SatMenu::_identN = 2;
const int SatMenu::_motorN = 2;
const int SatMenu::_touchN = 2;
const int SatMenu::_diagN  = 4;
const int SatMenu::_wifiN  = 2;

// ─────────────────────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────────────────────
SatMenu::SatMenu(LovyanGFX* tft)
    : _tft(tft),
      _spr(tft)
{
    pinMode(BUTTON_PIN_REC,    INPUT_PULLUP);
    pinMode(BUTTON_PIN_SOLO,   INPUT_PULLUP);
    pinMode(BUTTON_PIN_MUTE,   INPUT_PULLUP);
    pinMode(BUTTON_PIN_SELECT, INPUT_PULLUP);
    pinMode(ENCODER_PIN_A,  INPUT);
    pinMode(ENCODER_PIN_B,  INPUT);
    pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    pinMode(MOTOR_EN,  OUTPUT);
    _motorStop();
    _load();
    _tmp = _cfg;
}

// ─────────────────────────────────────────────────────────────
//  open / close
// ─────────────────────────────────────────────────────────────
void SatMenu::open() {
    if (_open) return;
    _open = true;
    _scr = Scr::MAIN; _cur = 0; _scrl = 0; _dirty = true; _tmp = _cfg;

    if (_cbMotorOff)   _cbMotorOff();
    if (_cbRS485Off)   _cbRS485Off();
    if (_cbLedsOff)    _cbLedsOff();
    if (_cbBrightness) _cbBrightness(255);

    if (_cbSuspend)    _cbSuspend();

    _spr.setColorDepth(16);
    _spr.setPsram(true);
    _spr.createSprite(_tft->width(), _tft->height());
    _spr.setTextFont(1);

    _spr.fillScreen(C_BG);
    _push();
}

void SatMenu::close() {
    if (!_open) return;
    if (_cfg.trackId == 0) return;
    _open = false;

    _motorStop();
    _spr.deleteSprite();             // liberar PSRAM

    if (_cbMotorOn)    _cbMotorOn();
    if (_cbRS485On)    _cbRS485On();
    if (_cbRestore)    _cbRestore();  // recrear sprites normales
    _tft->fillScreen(C_BLACK);
}

// ─────────────────────────────────────────────────────────────
//  update
// ─────────────────────────────────────────────────────────────
void SatMenu::update() {
    if (!_open) return;

    bool live = (_scr == Scr::TEST_DISPLAY   ||
                 _scr == Scr::TEST_ENCODER   ||
                 _scr == Scr::TEST_FADER     ||
                 _scr == Scr::TEST_NEOPIXEL);

    Btn b = _readBtn();

    // Toast: auto-dismiss
    if (_scr == Scr::TOAST) {
        if (millis() - _toastT > 1300 || b != Btn::NONE) _goto(_toastRet);
        else { _render(); }
        return;
    }

    if (!live && !_dirty && b == Btn::NONE) return;
    if (_dirty || live) { _render(); _dirty = false; }

    switch (_scr) {
        case Scr::MAIN:          _hMain(b);            break;
        case Scr::IDENTIDAD:     _hIdent(b);           break;
        case Scr::MOTOR:         _hMotor(b);           break;
        case Scr::TOUCH:         _hTouch(b);           break;
        case Scr::DIAG:          _hDiag(b);            break;
        case Scr::WIFI:          _hWifi(b);            break;
        case Scr::EDIT_TRACKID:
        case Scr::EDIT_PWMMIN:
        case Scr::EDIT_PWMMAX:
        case Scr::EDIT_TOUCHTHR: _hEditVal(b);         break;
        case Scr::EDIT_LABEL:    _hEditLbl(b);         break;
        case Scr::CONFIRM:       _hConfirm(b);         break;
        case Scr::TOAST:         _hToast(b);           break;
        case Scr::TEST_DISPLAY:  _tickTestDisplay(b);  break;
        case Scr::TEST_ENCODER:  _tickTestEncoder(b);  break;
        case Scr::TEST_FADER:    _tickTestFader(b);    break;
        case Scr::TEST_NEOPIXEL: _tickTestNeopixel(b); break;
        case Scr::REINICIAR:
            _confirm("Reiniciar dispositivo?", Scr::REINICIAR); break;
        default: break;
    }
}

// ─────────────────────────────────────────────────────────────
//  Lectura botones
// ─────────────────────────────────────────────────────────────
SatMenu::Btn SatMenu::_readBtn() {
    unsigned long n = millis();
    if (n - _debT < SAT_DEB_MS) return Btn::NONE;
    if (digitalRead(BUTTON_PIN_REC)    == LOW) { _debT=n; return Btn::UP;    }
    if (digitalRead(BUTTON_PIN_SOLO)   == LOW) { _debT=n; return Btn::DOWN;  }
    if (digitalRead(BUTTON_PIN_MUTE)   == LOW) { _debT=n; return Btn::BACK;  }
    if (digitalRead(BUTTON_PIN_SELECT) == LOW) { _debT=n; return Btn::ENTER; }
    return Btn::NONE;
}

// ─────────────────────────────────────────────────────────────
//  Render dispatcher
// ─────────────────────────────────────────────────────────────
void SatMenu::_render() {
    // Los tests live se renderizan solos y hacen _push() al final
    switch (_scr) {
        case Scr::TEST_DISPLAY:  _tickTestDisplay(Btn::NONE);  return;
        case Scr::TEST_ENCODER:  _tickTestEncoder(Btn::NONE);  return;
        case Scr::TEST_FADER:    _tickTestFader(Btn::NONE);    return;
        case Scr::TEST_NEOPIXEL: _tickTestNeopixel(Btn::NONE); return;
        default: break;
    }

    // Pantallas estáticas: limpiar sprite y redibujar
    _spr.fillScreen(C_BG);

    switch (_scr) {
        case Scr::MAIN:
            _drawHdr(_cfg.trackId == 0 ? "!SIN CONFIGURAR!" : "iMakie SAT");
            _drawList(_mainItems, _mainN);
            _drawHints("","","Salir","Entrar");
            if (_cfg.trackId == 0) {
                int W = _spr.width();
                _spr.fillRect(0, SAT_HDR_H, W, 20, C_ACCENT);
                _spr.setTextColor(C_WHITE, C_ACCENT);
                _spr.setTextSize(1);
                _spr.setTextDatum(textdatum_t::middle_center);
                _spr.drawString("Configura Track ID antes de usar", W/2, SAT_HDR_H+10);
            }
            break;
        case Scr::IDENTIDAD:
            _drawHdr("Identidad");
            _drawList(_identItems, _identN);
            _drawHints("","","Atras","Entrar");
            break;
        case Scr::MOTOR:
            _drawHdr("Motor");
            _drawList(_motorItems, _motorN);
            _drawHints("","","Atras","Entrar");
            break;
        case Scr::TOUCH: {
            _drawHdr("Touch");
            int W = _spr.width();
            int y = SAT_HDR_H + 4;
            bool en = _tmp.touchEnabled;
            _spr.fillRect(0, y, W, SAT_ROW_H, _cur==0 ? C_ACCENT : C_BG);
            _drawBadge(6, y+(SAT_ROW_H-SAT_BADGE_H)/2, "EN",
                _cur==0?C_WHITE:C_ACCENT, _cur==0?C_ACCENT:C_WHITE);
            _spr.setTextColor(_cur==0?C_WHITE:C_TEXT, _cur==0?C_ACCENT:C_BG);
            _spr.setTextSize(1); _spr.setTextDatum(textdatum_t::middle_left);
            _spr.drawString("Habilitado", SAT_BADGE_W+14, y+SAT_ROW_H/2);
            _spr.setTextDatum(textdatum_t::middle_right);
            _spr.setTextColor(en?C_GREEN:C_GRAY, _cur==0?C_ACCENT:C_BG);
            _spr.drawString(en?"ON":"OFF", W-10, y+SAT_ROW_H/2);
            _drawDivider(y+SAT_ROW_H);
            y += SAT_ROW_H;
            char buf[8]; snprintf(buf,8,"%d%%",_tmp.touchThreshold);
            _spr.fillRect(0, y, W, SAT_ROW_H, _cur==1?C_ACCENT:C_BG);
            _drawBadge(6, y+(SAT_ROW_H-SAT_BADGE_H)/2, "TH",
                _cur==1?C_WHITE:C_ACCENT, _cur==1?C_ACCENT:C_WHITE);
            _spr.setTextColor(_cur==1?C_WHITE:C_TEXT, _cur==1?C_ACCENT:C_BG);
            _spr.setTextDatum(textdatum_t::middle_left);
            _spr.drawString("Umbral", SAT_BADGE_W+14, y+SAT_ROW_H/2);
            _spr.setTextDatum(textdatum_t::middle_right);
            _spr.drawString(buf, W-10, y+SAT_ROW_H/2);
            _drawHints("","","Atras","Editar");
            break;
        }
        case Scr::DIAG:
            _drawHdr("Diagnostico");
            _drawList(_diagItems, _diagN);
            _drawHints("","","Atras","Entrar");
            break;
        case Scr::WIFI:
            _drawHdr("Config WiFi");
            _drawList(_wifiItems, _wifiN);
            _drawHints("","","Atras","Entrar");
            break;
        case Scr::EDIT_TRACKID:
        case Scr::EDIT_PWMMIN:
        case Scr::EDIT_PWMMAX:
        case Scr::EDIT_TOUCHTHR:
            _drawValEdit(_eTitle, _eVal, _eMin, _eMax,
                _scr==Scr::EDIT_TOUCHTHR ? "%" : "");
            break;
        case Scr::EDIT_LABEL:
            _drawHdr("Etiqueta");
            _drawLblEdit();
            _drawHints("Car+","Car-","Pos<","Pos>");
            break;
        case Scr::CONFIRM: _drawConfirm(_confMsg); break;
        case Scr::TOAST:   _drawToast(_toastMsg);  break;
        default: break;
    }

    _push();
}

// ─────────────────────────────────────────────────────────────
//  TEST DISPLAY
// ─────────────────────────────────────────────────────────────
static const uint16_t DISP_SOLID_COLORS[] = {
    C_RED,C_GREEN,C_BLUE,C_WHITE,C_BLACK,C_YELLOW,C_CYAN,C_ORANGE,C_ACCENT
};
static const char* DISP_SOLID_NAMES[] = {
    "Rojo","Verde","Azul","Blanco","Negro","Amarillo","Cyan","Naranja","Acento"
};
static const int DISP_SOLID_N = 9;

void SatMenu::_tickTestDisplay(Btn b) {
    int W = _spr.width(), H = _spr.height();

    if (b == Btn::BACK) { _goto(Scr::DIAG); return; }
    if (b == Btn::ENTER || b == Btn::UP) _dPat = (_dPat+1) % 7;
    if (b == Btn::DOWN)                  _dPat = (_dPat+6) % 7;

    switch (_dPat) {
        case 0: {
            static int ci = 0;
            if (b == Btn::ENTER || b == Btn::UP) ci = (ci+1) % DISP_SOLID_N;
            if (b == Btn::DOWN)                  ci = (ci+DISP_SOLID_N-1) % DISP_SOLID_N;
            _spr.fillScreen(DISP_SOLID_COLORS[ci]);
            uint16_t inv = ~DISP_SOLID_COLORS[ci];
            _spr.setTextColor(inv); _spr.setTextSize(2);
            _spr.setTextDatum(textdatum_t::middle_center);
            _spr.drawString(DISP_SOLID_NAMES[ci], W/2, H/2);
            _spr.setTextSize(1);
            _spr.drawString("SEL/REC/SOL=color  MUT=salir", W/2, H-14);
            break;
        }
        case 1: {
            for (int x=0;x<W;x++) {
                uint8_t r=(x*31)/W, g=(x*63)/W, bv=31-(x*31)/W;
                _spr.drawFastVLine(x,0,H,(uint16_t)((r<<11)|(g<<5)|bv));
            }
            _spr.setTextColor(C_WHITE); _spr.setTextSize(1);
            _spr.setTextDatum(textdatum_t::middle_center);
            _spr.drawString("Gradiente RGB — SEL=sig  MUT=salir",W/2,H/2);
            break;
        }
        case 2: {
            _spr.fillScreen(C_BLACK);
            for (int y=0;y<H;y+=8) for (int x=0;x<W;x+=8)
                if (((x/8)+(y/8))%2==0) _spr.fillRect(x,y,8,8,C_WHITE);
            _spr.setTextColor(C_ACCENT); _spr.setTextSize(1);
            _spr.setTextDatum(textdatum_t::middle_center);
            _spr.drawString("Checker 8px — SEL=sig  MUT=salir",W/2,H/2);
            break;
        }
        case 3: {
            _spr.fillScreen(C_WHITE);
            int y=4;
            _spr.setTextColor(C_BLACK); _spr.setTextDatum(textdatum_t::top_left);
            _spr.setTextSize(1); _spr.drawString("Texto tam 1 — ABCDEFGabcdefg0123",2,y); y+=14;
            _spr.setTextSize(2); _spr.drawString("Tam 2 ABCabc123",2,y); y+=22;
            _spr.setTextSize(3); _spr.drawString("Tam 3 ABC",2,y); y+=30;
            _spr.setTextColor(C_ACCENT);
            _spr.setTextSize(2); _spr.drawString("Acento rojo",2,y); y+=22;
            _spr.setTextColor(C_DARK); _spr.setTextSize(1);
            _spr.drawString("SEL=sig  MUT=salir",2,H-14);
            break;
        }
        case 4: {
            _spr.fillScreen(C_BLACK);
            _spr.fillRect(10,40,60,40,C_RED);
            _spr.fillCircle(W/2,80,30,C_GREEN);
            _spr.fillTriangle(10,H-20,W/2-10,H-80,W-10,H-20,C_BLUE);
            _spr.drawRect(W-80,40,60,40,C_YELLOW);
            _spr.drawCircle(W/2,H/2,20,C_CYAN);
            _spr.drawRoundRect(20,H/2-20,80,40,8,C_ACCENT);
            _spr.setTextColor(C_WHITE); _spr.setTextSize(1);
            _spr.setTextDatum(textdatum_t::bottom_center);
            _spr.drawString("Geometria — SEL=sig  MUT=salir",W/2,H-2);
            break;
        }
        case 5: {
            int bars=32, bw=W/bars;
            for (int i=0;i<bars;i++) {
                uint8_t v=(i*255)/(bars-1);
                uint16_t col=((v>>3)<<11)|((v>>2)<<5)|(v>>3);
                _spr.fillRect(i*bw,0,bw,H-SAT_HINT_H,col);
            }
            _spr.fillRect(0,H-SAT_HINT_H,W,SAT_HINT_H,C_BLACK);
            _spr.setTextColor(C_WHITE); _spr.setTextSize(1);
            _spr.setTextDatum(textdatum_t::middle_center);
            _spr.drawString("Grises — SEL=sig  MUT=salir",W/2,H-SAT_HINT_H/2);
            break;
        }
        case 6: {
            static int lx=0; static uint16_t lcol=C_ACCENT;
            _spr.drawFastVLine((lx+W-4)%W,0,H-SAT_HINT_H,C_BLACK);
            _spr.drawFastVLine(lx,0,H-SAT_HINT_H,lcol);
            lx=(lx+1)%W;
            if (lx==0) {
                static int ci=0;
                const uint16_t cols[]={C_RED,C_GREEN,C_BLUE,C_CYAN,C_ACCENT};
                ci=(ci+1)%5; lcol=cols[ci];
            }
            _spr.fillRect(0,H-SAT_HINT_H,W,SAT_HINT_H,C_BLACK);
            _spr.setTextColor(C_WHITE); _spr.setTextSize(1);
            _spr.setTextDatum(textdatum_t::middle_center);
            _spr.drawString("Barrido — SEL=sig  MUT=salir",W/2,H-SAT_HINT_H/2);
            break;
        }
    }
    _push();
}

// ─────────────────────────────────────────────────────────────
//  TEST ENCODER
// ─────────────────────────────────────────────────────────────
void SatMenu::_tickTestEncoder(Btn b) {
    int W = _spr.width(), H = _spr.height();

    if (b == Btn::BACK) { _goto(Scr::DIAG); return; }
    if (b == Btn::UP || b == Btn::DOWN) { _encCnt=0; memset(_encHist,0,ENC_HIST); }
    if (b == Btn::ENTER) { _encCnt=0; memset(_encHist,0,ENC_HIST); }

    int A = digitalRead(ENCODER_PIN_A);
    int B = digitalRead(ENCODER_PIN_B);
    unsigned long now = millis();
    if (A != _encLastA) {
        if (now - _encDebT > 3) {
            _encDebT = now;
            if (A == LOW) {
                _encCnt += (B == HIGH) ? 1 : -1;
                _encHist[_encHistIdx] = (int8_t)constrain(_encCnt,-63,63);
                _encHistIdx = (_encHistIdx+1) % ENC_HIST;
            }
        }
        _encLastA = A;
    }
    if (B != _encLastB) _encLastB = B;
    bool sw = (digitalRead(ENCODER_SW_PIN) == LOW);
    _encSW = sw;

    _spr.fillScreen(C_BG);
    _spr.fillRect(0,0,W,SAT_HDR_H,C_BG);
    _spr.setTextColor(C_WHITE,C_BG); _spr.setTextSize(1);
    _spr.setTextDatum(textdatum_t::middle_center);
    _spr.drawString("TEST ENCODER",W/2,SAT_HDR_H/2);
    _spr.drawFastHLine(0,SAT_HDR_H-2,W,C_ACCENT);

    int y = SAT_HDR_H+6;
    _spr.setTextDatum(textdatum_t::top_left); _spr.setTextSize(1);
    _spr.setTextColor(A==LOW?C_GREEN:C_GRAY,C_BG);
    _spr.drawString(A==LOW?"A = LOW ":"A = HIGH",6,y);
    _spr.setTextColor(B==LOW?C_GREEN:C_GRAY,C_BG);
    _spr.drawString(B==LOW?"B = LOW ":"B = HIGH",W/2,y); y+=16;
    _spr.setTextColor(sw?C_YELLOW:C_GRAY,C_BG);
    _spr.drawString(sw?"SW = PULSADO  ":"SW = libre    ",6,y); y+=18;

    char cbuf[8]; snprintf(cbuf,8,"%+ld",_encCnt);
    _spr.setTextSize(4); _spr.setTextDatum(textdatum_t::middle_center);
    _spr.setTextColor(C_WHITE,C_BG);
    _spr.drawString(cbuf,W/2,y+28); y+=64;

    _spr.setTextSize(2);
    int p2=(_encHistIdx+ENC_HIST-1)%ENC_HIST, p3=(_encHistIdx+ENC_HIST-2)%ENC_HIST;
    const char* dir="  =  ";
    if (_encHist[p2]>_encHist[p3]) dir=" >>  ";
    else if (_encHist[p2]<_encHist[p3]) dir=" <<  ";
    _spr.setTextColor(C_CYAN,C_BG);
    _spr.drawString(dir,W/2,y); y+=22;

    int gW=W-12,gH=40,gX=6,gY=y;
    _spr.drawRect(gX-1,gY-1,gW+2,gH+2,C_DARK);
    int bw2=gW/ENC_HIST, cy2=gY+gH/2;
    _spr.drawFastHLine(gX,cy2,gW,C_DARK);
    for (int i=0;i<ENC_HIST;i++) {
        int idx=(_encHistIdx+i)%ENC_HIST, v=_encHist[idx];
        int barH=abs(v)*(gH/2)/63;
        uint16_t col=v>=0?C_CYAN:C_ACCENT;
        int bx=gX+i*bw2;
        if (v>=0) _spr.fillRect(bx,cy2-barH,bw2>1?bw2-1:1,barH,col);
        else      _spr.fillRect(bx,cy2,bw2>1?bw2-1:1,barH,col);
    }

    _spr.fillRect(0,H-SAT_HINT_H,W,SAT_HINT_H,C_BG);
    _spr.drawFastHLine(0,H-SAT_HINT_H,W,C_DARK);
    _spr.setTextSize(1); _spr.setTextColor(C_GRAY,C_BG);
    _spr.setTextDatum(textdatum_t::middle_left);
    _spr.drawString("REC/SOL=reset  SEL=hist  MUT=salir",4,H-SAT_HINT_H/2);
    _push();
}

// ─────────────────────────────────────────────────────────────
//  TEST FADER
// ─────────────────────────────────────────────────────────────
static int _touchRead() { return (int)touchRead(FADER_TOUCH_PIN); }

void SatMenu::_tickTestFader(Btn b) {
    int W = _spr.width(), H = _spr.height();

    if (b == Btn::BACK)  { _motorStop(); _goto(Scr::DIAG); return; }
    if (b == Btn::UP)    { _motPWM=constrain(_motPWM+20,-255,255); _motorDrive(_motPWM); }
    if (b == Btn::DOWN)  { _motPWM=constrain(_motPWM-20,-255,255); _motorDrive(_motPWM); }
    if (b == Btn::ENTER) { _motPWM=0; _motorStop(); _fadCalMin=4095; _fadCalMax=0; }

    unsigned long now=millis();
    if (now-_fadT>25) {
        _fadT=now;
        int sum=0; for (int i=0;i<8;i++) sum+=analogRead(FADER_POT_PIN);
        _fadRaw=sum/8;
        if (_fadRaw<_fadCalMin) _fadCalMin=_fadRaw;
        if (_fadRaw>_fadCalMax) _fadCalMax=_fadRaw;
        int range=_fadCalMax-_fadCalMin;
        _fadPct=range>50?(float)(_fadRaw-_fadCalMin)/range:(float)_fadRaw/4095.0f;
        _fadPct=constrain(_fadPct,0.0f,1.0f);
        if (now-_fadHistT>50) {
            _fadHistT=now;
            _fadHist[_fadHistIdx]=(uint8_t)(_fadPct*255);
            _fadHistIdx=(_fadHistIdx+1)%FAD_HIST;
        }
    }
    _tchRaw=_touchRead();
    static int tchBase=0;
    if (tchBase==0&&_tchRaw>100) tchBase=_tchRaw;
    _tchOn=(tchBase>0)&&(_tchRaw<(int)(tchBase*0.80f));

    // ── Componer frame en sprite ──────────────────────────
    _spr.fillScreen(C_BG);
    _drawHdr("TEST FADER");
    int y=SAT_HDR_H+6;
    char buf[32];

    // FADER
    _spr.setTextColor(C_CYAN,C_BG); _spr.setTextSize(1);
    _spr.setTextDatum(textdatum_t::top_left);
    _spr.drawString("FADER",4,y);
    snprintf(buf,32,"raw=%4d  %.1f%%  [%d-%d]",_fadRaw,_fadPct*100.f,_fadCalMin,_fadCalMax);
    _spr.setTextColor(C_TEXT,C_BG); _spr.drawString(buf,44,y); y+=14;
    _drawHBar(4,y,W-8,14,_fadPct,C_CYAN);
    int ugX=4+(int)((W-8)*0.75f);
    _spr.drawFastVLine(ugX,y,14,C_YELLOW);
    y+=20;
    _spr.setTextColor(C_YELLOW,C_BG); _spr.setTextDatum(textdatum_t::top_center);
    _spr.drawString("0dB",ugX,y); y+=12;

    // TOUCH
    _drawDivider(y+2); y+=8;
    _spr.setTextColor(C_YELLOW,C_BG); _spr.setTextDatum(textdatum_t::top_left);
    _spr.drawString("TOUCH",4,y);
    _spr.fillCircle(55,y+5,6,_tchOn?C_GREEN:C_DARK);
    snprintf(buf,32," %s  raw=%5d  base=%5d",_tchOn?"TOCADO  ":"libre   ",_tchRaw,tchBase);
    _spr.setTextColor(_tchOn?C_GREEN:C_GRAY,C_BG); _spr.drawString(buf,62,y); y+=20;
    if (tchBase>0) {
        float ratio=constrain(1.0f-(float)_tchRaw/tchBase,0.0f,1.0f);
        _drawHBar(4,y,W-8,10,ratio,_tchOn?C_GREEN:C_DARK);
    }
    y+=16;

    // MOTOR
    _drawDivider(y+2); y+=8;
    _spr.setTextColor(C_ORANGE,C_BG); _spr.setTextDatum(textdatum_t::top_left);
    _spr.drawString("MOTOR",4,y);
    float mPct=((float)_motPWM+255.f)/510.f;
    int bx=44, bw=W-48;
    _spr.fillRect(bx,y,bw,14,C_DARK);
    int center=bx+bw/2, barEnd=bx+(int)(mPct*bw);
    if (barEnd>=center) _spr.fillRect(center,y,barEnd-center,14,_motPWM>0?C_GREEN:C_DARK);
    else                _spr.fillRect(barEnd,y,center-barEnd,14,C_ACCENT);
    _spr.drawFastVLine(center,y,14,C_WHITE);
    snprintf(buf,32," PWM %+d",_motPWM);
    _spr.setTextColor(C_TEXT,C_BG); _spr.drawString(buf,bx+bw+2,y); y+=18;
    snprintf(buf,32,"IN1=%s  IN2=%s  EN=%s",
        _motPWM>0?"PWM":"0",_motPWM<0?"PWM":"0",abs(_motPWM)>0?"HIGH":"LOW");
    _spr.setTextColor(C_GRAY,C_BG); _spr.drawString(buf,44,y); y+=16;

    // HISTORIAL
    _drawDivider(y); y+=4;
    int gH=H-SAT_HINT_H-y-4; if (gH<20) gH=20;
    int gW=W-8;
    _spr.drawRect(3,y,gW+2,gH+2,C_DARK);
    _spr.drawFastHLine(4,y+gH-(int)(0.75f*gH),gW,C_YELLOW);
    int pxW=gW/FAD_HIST; if (pxW<1) pxW=1;
    for (int i=0;i<FAD_HIST;i++) {
        int idx=(_fadHistIdx+i)%FAD_HIST;
        int fh=(_fadHist[idx]*gH)/255;
        _spr.fillRect(4+i*(gW/FAD_HIST),y+gH-fh,pxW,fh+1,C_CYAN);
    }

    // HINTS
    _spr.fillRect(0,H-SAT_HINT_H,W,SAT_HINT_H,C_BG);
    _spr.drawFastHLine(0,H-SAT_HINT_H,W,C_DARK);
    _spr.setTextSize(1); _spr.setTextColor(C_GRAY,C_BG);
    _spr.setTextDatum(textdatum_t::middle_left);
    _spr.drawString("REC=mot+  SOL=mot-  SEL=stop/cal  MUT=salir",4,H-SAT_HINT_H/2);
    _push();
}

// ─────────────────────────────────────────────────────────────
//  Handlers menú
// ─────────────────────────────────────────────────────────────
void SatMenu::_hMain(Btn b) {
    if (b == Btn::UP)   { if (_cur>0)        { _cur--; _dirty=true; } }
    if (b == Btn::DOWN) { if (_cur<_mainN-1) { _cur++; _dirty=true; } }
    if (b == Btn::BACK) { close(); return; }
    if (b == Btn::ENTER) {
        switch (_mainItems[_cur].target) {
            case Scr::IDENTIDAD:   _goto(Scr::IDENTIDAD); break;
            case Scr::OTA:
                if (_cbWiFiOta) _cbWiFiOta();
                _toast("Activando OTA...", Scr::MAIN);
            break;
            case Scr::MOTOR:       _goto(Scr::MOTOR);     break;
            case Scr::TOUCH:       _goto(Scr::TOUCH);     break;
            case Scr::DIAG:        _goto(Scr::DIAG);      break;
            case Scr::CONFIG_WIFI: _goto(Scr::WIFI);      break;
            case Scr::REINICIAR:   _confirm("Reiniciar el dispositivo?", Scr::REINICIAR); break;
            default: break;
        }
    }
}
void SatMenu::_hWifi(Btn b) {
    if (b == Btn::UP)   { if (_cur>0)        { _cur--; _dirty=true; } }
    if (b == Btn::DOWN) { if (_cur<_wifiN-1) { _cur++; _dirty=true; } }
    if (b == Btn::BACK) { _goto(Scr::MAIN); return; }
    if (b == Btn::ENTER) {
        if (_cur==0) {
            if (_cbWiFiConfig) _cbWiFiConfig();
            _toast("Lanzando portal WiFi...", Scr::MAIN);
        } else {
            if (_cbWiFiOta) _cbWiFiOta();
            _toast("Activando OTA...", Scr::MAIN);
        }
    }
}
void SatMenu::_hIdent(Btn b) {
    if (b == Btn::UP)   { if (_cur>0)         { _cur--; _dirty=true; } }
    if (b == Btn::DOWN) { if (_cur<_identN-1) { _cur++; _dirty=true; } }
    if (b == Btn::BACK) _goto(Scr::MAIN);
    if (b == Btn::ENTER) {
        if (_cur==0) { _eTitle="Track ID"; _eVal=max((int)_tmp.trackId,1); _eMin=1; _eMax=8; _goto(Scr::EDIT_TRACKID); }
        else         { _lblIdx=0; _goto(Scr::EDIT_LABEL); }
    }
}
void SatMenu::_hMotor(Btn b) {
    if (b == Btn::UP)   { if (_cur>0)         { _cur--; _dirty=true; } }
    if (b == Btn::DOWN) { if (_cur<_motorN-1) { _cur++; _dirty=true; } }
    if (b == Btn::BACK) _goto(Scr::MAIN);
    if (b == Btn::ENTER) {
        if (_cur==0) { _eTitle="PWM Minimo"; _eVal=_tmp.pwmMin; _eMin=0;  _eMax=120; _goto(Scr::EDIT_PWMMIN); }
        else         { _eTitle="PWM Maximo"; _eVal=_tmp.pwmMax; _eMin=50; _eMax=255; _goto(Scr::EDIT_PWMMAX); }
    }
}
void SatMenu::_hTouch(Btn b) {
    if (b == Btn::UP)   { if (_cur>0)         { _cur--; _dirty=true; } }
    if (b == Btn::DOWN) { if (_cur<_touchN-1) { _cur++; _dirty=true; } }
    if (b == Btn::BACK) _goto(Scr::MAIN);
    if (b == Btn::ENTER) {
        if (_cur==0) {
            _tmp.touchEnabled=!_tmp.touchEnabled;
            _cfg.touchEnabled=_tmp.touchEnabled;
            _save(); if (_cbSaved) _cbSaved(_cfg);
            _toast(_tmp.touchEnabled?"Touch: ON":"Touch: OFF", Scr::TOUCH);
        } else {
            _eTitle="Umbral Touch"; _eVal=_tmp.touchThreshold; _eMin=10; _eMax=90;
            _goto(Scr::EDIT_TOUCHTHR);
        }
    }
}
void SatMenu::_hDiag(Btn b) {
    if (b == Btn::UP)   { if (_cur>0)        { _cur--; _dirty=true; } }
    if (b == Btn::DOWN) { if (_cur<_diagN-1) { _cur++; _dirty=true; } }
    if (b == Btn::BACK) _goto(Scr::MAIN);
    if (b == Btn::ENTER) {
        switch (_cur) {
            case 0: _dPat=0; _goto(Scr::TEST_DISPLAY); break;
            case 1: _encCnt=0; memset(_encHist,0,ENC_HIST); _goto(Scr::TEST_ENCODER); break;
            case 2: _motPWM=0; _fadCalMin=4095; _fadCalMax=0;
                    _fadHistIdx=0; memset(_fadHist,0,FAD_HIST);
                    _goto(Scr::TEST_FADER); break;
            case 3: memset(_neoPressed,0,sizeof(_neoPressed));
                    _goto(Scr::TEST_NEOPIXEL); break;
        }
    }
}
void SatMenu::_hEditVal(Btn b) {
    if (b == Btn::UP)   { if (_eVal<_eMax) { _eVal++; _dirty=true; } }
    if (b == Btn::DOWN) { if (_eVal>_eMin) { _eVal--; _dirty=true; } }
    if (b == Btn::BACK) _back();
    if (b == Btn::ENTER) {
        switch (_scr) {
            case Scr::EDIT_TRACKID:  _cfg.trackId=_tmp.trackId=(uint8_t)_eVal; break;
            case Scr::EDIT_PWMMIN:   _cfg.pwmMin =_tmp.pwmMin =(uint8_t)_eVal; break;
            case Scr::EDIT_PWMMAX:   _cfg.pwmMax =_tmp.pwmMax =(uint8_t)_eVal; break;
            case Scr::EDIT_TOUCHTHR: _cfg.touchThreshold=_tmp.touchThreshold=(uint8_t)_eVal; break;
            default: break;
        }
        _save(); if (_cbSaved) _cbSaved(_cfg);
        _toast("Guardado!", _prev);
    }
}
void SatMenu::_hEditLbl(Btn b) {
    if (b == Btn::UP) {
        char& c=_tmp.label[_lblIdx];
        c=(c>='A'&&c<'Z')?c+1:(c=='Z')?'a':(c>='a'&&c<'z')?c+1:(c=='z')?'0':(c>='0'&&c<'9')?c+1:(c=='9')?' ':' ';
        _dirty=true;
    }
    if (b == Btn::DOWN) {
        char& c=_tmp.label[_lblIdx];
        c=(c=='A')?' ':(c==' ')?'9':(c=='0')?'z':(c=='a')?'Z':(c>'a')?c-1:(c>'0')?c-1:(c>'A')?c-1:c;
        _dirty=true;
    }
    if (b == Btn::BACK) {
        if (_lblIdx>0) { _lblIdx--; _dirty=true; }
        else { memcpy(_tmp.label,_cfg.label,7); _goto(Scr::IDENTIDAD); }
    }
    if (b == Btn::ENTER) {
        if (_lblIdx<5) { _lblIdx++; _dirty=true; }
        else { memcpy(_cfg.label,_tmp.label,7); _save(); if (_cbSaved) _cbSaved(_cfg); _toast("Etiqueta guardada!",Scr::IDENTIDAD); }
    }
}
void SatMenu::_hConfirm(Btn b) {
    if (b == Btn::ENTER) {
        if (_confYes==Scr::REINICIAR) {
            _toast("Reiniciando...",Scr::MAIN); delay(900);
            if (_cbReboot) _cbReboot(); else ESP.restart();
        } else _goto(_confYes);
    }
    if (b == Btn::BACK) _back();
}
void SatMenu::_hToast(Btn b) {
    if (millis()-_toastT>1300||b!=Btn::NONE) _goto(_toastRet);
}

// ─────────────────────────────────────────────────────────────
//  Primitivas de dibujo — todas sobre _spr
// ─────────────────────────────────────────────────────────────
void SatMenu::_drawHdr(const char* t) {
    int W=_spr.width();
    _spr.fillRect(0,0,W,SAT_HDR_H,C_BG);
    _spr.setTextColor(C_TEXT,C_BG); _spr.setTextSize(1);
    _spr.setTextDatum(textdatum_t::middle_center);
    _spr.drawString(t,W/2,SAT_HDR_H/2);
    _spr.fillRect(0,SAT_HDR_H-2,W,2,C_ACCENT);   // línea roja bajo header
}
void SatMenu::_drawHints(const char* u,const char* d,const char* b,const char* e) {
    int W=_spr.width(),H=_spr.height(),y=H-SAT_HINT_H;
    _spr.fillRect(0,y,W,SAT_HINT_H,C_BG);
    _spr.drawFastHLine(0,y,W,C_DARK);
    _spr.setTextSize(1); _spr.setTextColor(C_GRAY,C_BG);
    const char* lbs[4]={u,d,b,e};
    const char* bns[4]={"REC","SOL","MUT","SEL"};
    int w4=W/4;
    for (int i=0;i<4;i++) {
        if (lbs[i]&&lbs[i][0]) {
            char buf[16]; snprintf(buf,16,"%s:%s",bns[i],lbs[i]);
            _spr.setTextDatum(textdatum_t::middle_center);
            _spr.drawString(buf,w4*i+w4/2,y+SAT_HINT_H/2);
        }
    }
}
void SatMenu::_drawList(const Item* items,int n) {
    int W=_spr.width(),H=_spr.height();
    int aH=H-SAT_HDR_H-SAT_HINT_H;
    int vis=aH/SAT_ROW_H; if (vis>n) vis=n;
    if (_cur<_scrl) _scrl=_cur;
    if (_cur>=_scrl+vis) _scrl=_cur-vis+1;
    for (int i=0;i<vis;i++) {
        int idx=i+_scrl; if (idx>=n) break;
        int y=SAT_HDR_H+i*SAT_ROW_H;
        bool sel=(idx==_cur);
        // Fila seleccionada: rojo. Resto: negro con separador blanco
        uint16_t bg=sel?C_ACCENT:C_BG;
        _spr.fillRect(0,y,W,SAT_ROW_H,bg);
        _drawBadge(6,y+(SAT_ROW_H-SAT_BADGE_H)/2,items[idx].badge,
            sel?C_WHITE:C_ACCENT, sel?C_ACCENT:C_WHITE);
        _spr.setTextColor(sel?C_WHITE:C_TEXT,bg);
        _spr.setTextSize(1); _spr.setTextDatum(textdatum_t::middle_left);
        _spr.drawString(items[idx].label,SAT_BADGE_W+14,y+SAT_ROW_H/2);
        _spr.setTextDatum(textdatum_t::middle_right);
        _spr.drawString(">",W-8,y+SAT_ROW_H/2);
        if (!sel) _drawDivider(y+SAT_ROW_H-1);
    }
    if (n>vis) {
        int bH=(aH*vis)/n, bY=SAT_HDR_H+(aH*_scrl)/n;
        _spr.fillRect(W-4,SAT_HDR_H,4,aH,C_DARK);
        _spr.fillRect(W-4,bY,4,bH,C_ACCENT);
    }
}
void SatMenu::_drawValEdit(const char* t,int v,int mn,int mx,const char* u) {
    int W=_spr.width(),H=_spr.height();
    _drawHdr(t);
    int cx=W/2,cy=H/2;
    _spr.setTextColor(C_ACCENT,C_BG); _spr.setTextSize(3);
    _spr.setTextDatum(textdatum_t::middle_center);
    _spr.drawString("^",cx,cy-46); _spr.drawString("v",cx,cy+46);
    char buf[12]; snprintf(buf,12,"%d%s",v,u);
    _spr.setTextColor(C_TEXT,C_BG); _spr.setTextSize(4);
    _spr.drawString(buf,cx,cy);
    char rb[16]; snprintf(rb,16,"[%d - %d]",mn,mx);
    _spr.setTextSize(1); _spr.setTextColor(C_GRAY,C_BG);
    _spr.drawString(rb,cx,cy+28);
    _drawHints("","","Cancel","Guardar");
}
void SatMenu::_drawLblEdit() {
    int W=_spr.width(),H=_spr.height();
    int cx=W/2,cy=H/2,charW=32,startX=cx-3*charW;
    _spr.setTextColor(C_TEXT,C_BG); _spr.setTextSize(1);
    _spr.setTextDatum(textdatum_t::middle_center);
    _spr.drawString("Navega con MUT/SEL, cambia con REC/SOL",cx,SAT_HDR_H+18);
    for (int i=0;i<6;i++) {
        int x=startX+i*charW; bool active=(i==_lblIdx);
        _spr.fillRoundRect(x,cy-18,charW-2,34,4,active?C_ACCENT:C_DARK);
        char c[2]={_tmp.label[i],0};
        _spr.setTextColor(active?C_WHITE:C_TEXT,active?C_ACCENT:C_DARK);
        _spr.setTextSize(2); _spr.setTextDatum(textdatum_t::middle_center);
        _spr.drawString(c,x+(charW-2)/2,cy);
    }
    _spr.setTextColor(C_ACCENT,C_BG); _spr.setTextSize(1);
    _spr.setTextDatum(textdatum_t::middle_center);
    _spr.drawString("^",startX+_lblIdx*charW+(charW-2)/2,cy+24);
}
void SatMenu::_drawConfirm(const char* msg) {
    int W=_spr.width(),H=_spr.height();
    _drawHdr("Confirmar");
    int cx=W/2,cy=H/2;
    _spr.setTextColor(C_TEXT,C_BG); _spr.setTextSize(1);
    _spr.setTextDatum(textdatum_t::middle_center);
    _spr.drawString(msg,cx,cy-18);
    _spr.fillRoundRect(cx-68,cy+8,54,28,6,C_ACCENT);
    _spr.fillRoundRect(cx+14,cy+8,54,28,6,C_DARK);
    _spr.setTextColor(C_WHITE,C_ACCENT); _spr.drawString("SI (SEL)",cx-41,cy+22);
    _spr.setTextColor(C_TEXT, C_DARK);   _spr.drawString("NO (MUT)",cx+41,cy+22);
}
void SatMenu::_drawToast(const char* msg) {
    // Toast: overlay sobre el frame actual — no limpiar pantalla
    int W=_spr.width(),H=_spr.height();
    int tw=W-40,th=40,tx=20,ty=H/2-20;
    _spr.fillRoundRect(tx,ty,tw,th,8,C_BG);
    _spr.drawRoundRect(tx,ty,tw,th,8,C_ACCENT);
    _spr.setTextColor(C_TEXT,C_BG); _spr.setTextSize(1);
    _spr.setTextDatum(textdatum_t::middle_center);
    _spr.drawString(msg,W/2,ty+th/2);
}
void SatMenu::_drawBadge(int x,int y,const char* t,uint16_t bg,uint16_t fg) {
    _spr.fillRoundRect(x,y,SAT_BADGE_W,SAT_BADGE_H,3,bg);
    _spr.setTextColor(fg,bg); _spr.setTextSize(1);
    _spr.setTextDatum(textdatum_t::middle_center);
    _spr.drawString(t,x+SAT_BADGE_W/2,y+SAT_BADGE_H/2);
}
void SatMenu::_drawHBar(int x,int y,int w,int h,float pct,uint16_t col) {
    _spr.fillRect(x,y,w,h,C_DARK);
    int fw=(int)(w*constrain(pct,0.0f,1.0f));
    if (fw>0) _spr.fillRect(x,y,fw,h,col);
    _spr.drawRect(x,y,w,h,C_GRAY);
}
void SatMenu::_drawDivider(int y) {
    _spr.drawFastHLine(0,y,_spr.width(),C_GRAY);
}

// ─────────────────────────────────────────────────────────────
//  Motor
// ─────────────────────────────────────────────────────────────
void SatMenu::_motorStop() {
    if (_cbMotorDrv) { _cbMotorDrv(0); return; }
    digitalWrite(MOTOR_EN,LOW); digitalWrite(MOTOR_IN1,LOW); digitalWrite(MOTOR_IN2,LOW);
}
void SatMenu::_motorDrive(int pwm) {
    if (_cbMotorDrv) { _cbMotorDrv(pwm); return; }
    if (pwm==0) { _motorStop(); return; }
    digitalWrite(MOTOR_EN,HIGH);
    if (pwm>0) { analogWrite(MOTOR_IN1,constrain(pwm,0,255)); digitalWrite(MOTOR_IN2,LOW); }
    else       { digitalWrite(MOTOR_IN1,LOW); analogWrite(MOTOR_IN2,constrain(-pwm,0,255)); }
}

// ─────────────────────────────────────────────────────────────
//  NVS
// ─────────────────────────────────────────────────────────────
void SatMenu::_load() {
    _prefs.begin("ptxx",true);
    _cfg.trackId        = _prefs.getUChar("trackId",0);
    _cfg.pwmMin         = _prefs.getUChar("pwmMin", 40);
    _cfg.pwmMax         = _prefs.getUChar("pwmMax", 220);
    _cfg.touchEnabled   = _prefs.getBool ("touchEn",true);
    _cfg.touchThreshold = _prefs.getUChar("touchThr",80);
    String lbl=_prefs.getString("label","CH-01 ");
    strncpy(_cfg.label,lbl.c_str(),6); _cfg.label[6]='\0';
    _prefs.end();
}
void SatMenu::_save() {
    _prefs.begin("ptxx",false);
    _prefs.putUChar("trackId", _cfg.trackId);
    _prefs.putUChar("pwmMin",  _cfg.pwmMin);
    _prefs.putUChar("pwmMax",  _cfg.pwmMax);
    _prefs.putBool ("touchEn", _cfg.touchEnabled);
    _prefs.putUChar("touchThr",_cfg.touchThreshold);
    _prefs.putString("label",  String(_cfg.label).substring(0,6));
    _prefs.end();
}

// ─────────────────────────────────────────────────────────────
//  Toast / Confirm
// ─────────────────────────────────────────────────────────────
void SatMenu::_toast(const char* msg,Scr ret) {
    _toastMsg=msg; _toastRet=ret; _toastT=millis();
    _scr=Scr::TOAST; _dirty=true;
}
void SatMenu::_confirm(const char* msg,Scr yes) {
    _confMsg=msg; _confYes=yes; _prev=_scr;
    _scr=Scr::CONFIRM; _dirty=true;
}


// ─────────────────────────────────────────────────────────────
//  TEST NEOPIXEL + BOTONES
//  Cada botón enciende su NeoPixel mientras está pulsado.
//  REC→0 rojo, SOLO→1 amarillo, MUTE→2 verde, SELECT→3 blanco
//  Salir: MUTE largo (vuelve a DIAG) — o los 4 pulsados a la vez
// ─────────────────────────────────────────────────────────────
void SatMenu::_tickTestNeopixel(Btn b) {
    int W = _spr.width(), H = _spr.height();

    // Leer pines directamente — sin debounce para ver respuesta inmediata
    bool p[4] = {
        digitalRead(BUTTON_PIN_REC)    == LOW,
        digitalRead(BUTTON_PIN_SOLO)   == LOW,
        digitalRead(BUTTON_PIN_MUTE)   == LOW,
        digitalRead(BUTTON_PIN_SELECT) == LOW
    };

    // Long-press MUTE para salir (1 segundo)
    if (p[2]) {
        if (_neoMuteHoldT == 0) _neoMuteHoldT = millis();
        if (millis() - _neoMuteHoldT >= 1000) {
            if (_cbLedsTest) for (int i = 0; i < 4; i++) _cbLedsTest(i, 0, 0, 0);
            _neoMuteHoldT = 0;
            _goto(Scr::DIAG);
            return;
        }
    } else {
        _neoMuteHoldT = 0;
    }

    // Colores por botón
    static const uint8_t COLORS[4][3] = {
        {255,   0,   0},   // REC    → rojo
        {255, 200,   0},   // SOLO   → amarillo
        {  0, 255,   0},   // MUTE   → verde
        {200, 200, 200},   // SELECT → blanco
    };
    static const char* NAMES[4] = {"REC", "SOLO", "MUTE", "SEL"};

    // Actualizar LEDs via callback
    if (_cbLedsTest) {
        for (int i = 0; i < 4; i++) {
            if (p[i]) _cbLedsTest(i, COLORS[i][0], COLORS[i][1], COLORS[i][2]);
            else      _cbLedsTest(i, 0, 0, 0);
        }
    }

    // Dibujar pantalla
    _spr.fillScreen(C_BG);
    _drawHdr("TEST BOTONES + LEDS");

    int y = SAT_HDR_H + 10;
    for (int i = 0; i < 4; i++) {
        uint16_t col = p[i] ?
            _spr.color565(COLORS[i][0], COLORS[i][1], COLORS[i][2]) : C_DARK;
        int bx = 10 + i * 56, bw = 50, bh = 50;
        _spr.fillRoundRect(bx, y, bw, bh, 6, col);
        _spr.setTextColor(p[i] ? C_BLACK : C_GRAY, col);
        _spr.setTextSize(1);
        _spr.setTextDatum(textdatum_t::middle_center);
        _spr.drawString(NAMES[i], bx + bw/2, y + bh/2);
    }

    y += 70;
    _spr.setTextColor(C_TEXT, C_BG);
    _spr.setTextSize(1);
    _spr.setTextDatum(textdatum_t::middle_center);
    _spr.drawString("Pulsa cada boton", W/2, y); y += 18;
    _spr.setTextColor(C_GRAY, C_BG);
    _spr.drawString("Se enciende su LED", W/2, y);

    _spr.fillRect(0, H - SAT_HINT_H, W, SAT_HINT_H, C_BG);
    _spr.drawFastHLine(0, H - SAT_HINT_H, W, C_DARK);
    _spr.setTextColor(C_GRAY, C_BG);
    _spr.setTextDatum(textdatum_t::middle_center);
    _spr.drawString("MUT solo = salir", W/2, H - SAT_HINT_H/2);

    _push();
}

void SatMenu::showStatus(const char* msg) {
    if (!_open || !_spr.width()) return;
    _spr.fillRect(0, 120, _spr.width(), 40, C_BG);
    _spr.setTextColor(C_TEXT, C_BG);
    _spr.setTextSize(1);
    _spr.setTextDatum(textdatum_t::middle_center);
    _spr.drawString(msg, _spr.width()/2, 140);
    _push();
}