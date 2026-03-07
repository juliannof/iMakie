// ============================================================
//  SatMenu.cpp  –  iMakie PTxx Track S2  (v2)
//  Todo el feedback en display. Sin Serial.
// ============================================================
#include "SatMenu.h"

// ─────────────────────────────────────────────────────────────
//  Tablas de menú
// ─────────────────────────────────────────────────────────────
const SatMenu::Item SatMenu::_mainItems[] = {
    {"ID","Identidad",   Scr::IDENTIDAD   },
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
    {"MN","PWM Minimo",  Scr::EDIT_PWMMIN},
    {"MX","PWM Maximo",  Scr::EDIT_PWMMAX},
};
const SatMenu::Item SatMenu::_touchItems[] = {
    {"EN","Habilitado",  Scr::TOUCH          },
    {"TH","Umbral %",    Scr::EDIT_TOUCHTHR  },
};
const SatMenu::Item SatMenu::_diagItems[] = {
    {"DP","Test Display",  Scr::TEST_DISPLAY  },
    {"EC","Test Encoder",  Scr::TEST_ENCODER  },
    {"NP","Test NeoPixel", Scr::TEST_NEOPIXEL },
    {"FD","Test Fader",    Scr::TEST_FADER    },
};
const int SatMenu::_mainN  = 6;
const int SatMenu::_identN = 2;
const int SatMenu::_motorN = 2;
const int SatMenu::_touchN = 2;
const int SatMenu::_diagN  = 4;

// ─────────────────────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────────────────────
SatMenu::SatMenu(LovyanGFX* tft)
    : _tft(tft),
      _neo(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800)
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
    _neo.begin();
    _neo.setBrightness(50);
    _neo.clear(); _neo.show();
    _load();
    _tmp = _cfg;
}

// ─────────────────────────────────────────────────────────────
void SatMenu::open() {
    if (_open) return;
    _open = true; _scr = Scr::MAIN;
    _cur = 0; _scrl = 0; _dirty = true; _tmp = _cfg;
    if (_cbMotorOff)   _cbMotorOff();
    if (_cbRS485Off)   _cbRS485Off();
    if (_cbBrightness) _cbBrightness(255);   // Brillo máximo en modo SAT
}
void SatMenu::close() {
    if (!_open) return;
    if (_cfg.trackId == 0) return;
    _open = false;
    _motorStop();
    _neo.clear(); _neo.show();
    if (_cbMotorOn)    _cbMotorOn();
    if (_cbRS485On)    _cbRS485On();
    if (_cbBrightness) _cbBrightness(_savedBrightness);  // Restaurar brillo previo
    _tft->fillScreen(C_BLACK);
}

// ─────────────────────────────────────────────────────────────
//  update — llamar en loop()
// ─────────────────────────────────────────────────────────────
void SatMenu::update() {
    if (!_open) return;

    bool live = (_scr == Scr::TEST_DISPLAY  ||
                 _scr == Scr::TEST_ENCODER  ||
                 _scr == Scr::TEST_NEOPIXEL ||
                 _scr == Scr::TEST_FADER);

    Btn b = _readBtn();

    // Toast auto-dismiss
    if (_scr == Scr::TOAST) {
        if (millis() - _toastT > 1300 || b != Btn::NONE)
            _goto(_toastRet);
        return;
    }

    if (!live && !_dirty && b == Btn::NONE) return;
    if (_dirty || live) { _render(); _dirty = false; }

    switch (_scr) {
        case Scr::MAIN:          _hMain(b);       break;
        case Scr::IDENTIDAD:     _hIdent(b);      break;
        case Scr::MOTOR:         _hMotor(b);      break;
        case Scr::TOUCH:         _hTouch(b);      break;
        case Scr::DIAG:          _hDiag(b);       break;
        case Scr::EDIT_TRACKID:
        case Scr::EDIT_PWMMIN:
        case Scr::EDIT_PWMMAX:
        case Scr::EDIT_TOUCHTHR: _hEditVal(b);    break;
        case Scr::EDIT_LABEL:    _hEditLbl(b);    break;
        case Scr::CONFIRM:       _hConfirm(b);    break;
        case Scr::TOAST:         _hToast(b);      break;
        case Scr::TEST_DISPLAY:  _tickTestDisplay(b);  break;
        case Scr::TEST_ENCODER:  _tickTestEncoder(b);  break;
        case Scr::TEST_NEOPIXEL: _tickTestNeopixel(b); break;
        case Scr::TEST_FADER:    _tickTestFader(b);    break;
        case Scr::CONFIG_WIFI:
            if (_cbWiFi) _cbWiFi();
            _toast("Lanzando WiFiManager...", Scr::MAIN);
            break;
        case Scr::REINICIAR:
            _confirm("Reiniciar dispositivo?", Scr::REINICIAR);
            break;
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
//  Render dispatcher (solo para pantallas estáticas)
// ─────────────────────────────────────────────────────────────
void SatMenu::_render() {
    switch (_scr) {
        case Scr::TEST_DISPLAY:  _tickTestDisplay(Btn::NONE);  return;
        case Scr::TEST_ENCODER:  _tickTestEncoder(Btn::NONE);  return;
        case Scr::TEST_NEOPIXEL: _tickTestNeopixel(Btn::NONE); return;
        case Scr::TEST_FADER:    _tickTestFader(Btn::NONE);    return;
        default: break;
    }

    _tft->fillScreen(C_BG);

    switch (_scr) {
        case Scr::MAIN:
            _drawHdr(_cfg.trackId == 0 ? "!SIN CONFIGURAR!" : "iMakie SAT");
            _drawList(_mainItems, _mainN);
            _drawHints("","","Salir","Entrar");
            // Banner de aviso si no hay ID
            if (_cfg.trackId == 0) {
                int W = _tft->width();
                _tft->fillRect(0, SAT_HDR_H, W, 20, C_ACCENT);
                _tft->setTextColor(C_WHITE, C_ACCENT);
                _tft->setTextSize(1);
                _tft->setTextDatum(textdatum_t::middle_center);
                _tft->drawString("Configura Track ID antes de usar", W/2, SAT_HDR_H + 10);
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
            int y = SAT_HDR_H + 4;
            bool en = _tmp.touchEnabled;
            _tft->fillRect(0, y, _tft->width(), SAT_ROW_H, _cur==0 ? C_ACCENT : C_BG);
            _drawBadge(6, y+(SAT_ROW_H-SAT_BADGE_H)/2, "EN",
                _cur==0?C_WHITE:C_ACCENT, _cur==0?C_ACCENT:C_WHITE);
            _tft->setTextColor(_cur==0?C_WHITE:C_TEXT, _cur==0?C_ACCENT:C_BG);
            _tft->setTextSize(1); _tft->setTextDatum(textdatum_t::middle_left);
            _tft->drawString("Habilitado", SAT_BADGE_W+14, y+SAT_ROW_H/2);
            _tft->setTextDatum(textdatum_t::middle_right);
            _tft->setTextColor(en?C_GREEN:C_DARK, _cur==0?C_ACCENT:C_BG);
            _tft->drawString(en?"ON":"OFF", _tft->width()-10, y+SAT_ROW_H/2);
            _drawDivider(y+SAT_ROW_H);
            y += SAT_ROW_H;
            char buf[8]; snprintf(buf,8,"%d%%",_tmp.touchThreshold);
            _tft->fillRect(0, y, _tft->width(), SAT_ROW_H, _cur==1?C_ACCENT:C_BG);
            _drawBadge(6, y+(SAT_ROW_H-SAT_BADGE_H)/2, "TH",
                _cur==1?C_WHITE:C_ACCENT, _cur==1?C_ACCENT:C_WHITE);
            _tft->setTextColor(_cur==1?C_WHITE:C_TEXT, _cur==1?C_ACCENT:C_BG);
            _tft->setTextDatum(textdatum_t::middle_left);
            _tft->drawString("Umbral", SAT_BADGE_W+14, y+SAT_ROW_H/2);
            _tft->setTextDatum(textdatum_t::middle_right);
            _tft->drawString(buf, _tft->width()-10, y+SAT_ROW_H/2);
            _drawHints("","","Atras","Editar");
            break;
        }
        case Scr::DIAG:
            _drawHdr("Diagnostico");
            _drawList(_diagItems, _diagN);
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
        case Scr::CONFIRM:  _drawConfirm(_confMsg); break;
        case Scr::TOAST:    _drawToast(_toastMsg);  break;
        default: break;
    }
}

// ─────────────────────────────────────────────────────────────
//  ██████████  TEST DISPLAY  ██████████
// ─────────────────────────────────────────────────────────────
/*
  Patrones (SELECT avanza, MUTE sale):
  0 – Relleno colores puros (rojo/verde/azul/blanco/negro)
  1 – Gradiente horizontal RGB565
  2 – Cuadrícula de píxeles aleatorios (checker)
  3 – Texto en todos los tamaños y posiciones
  4 – Formas geométricas (rectángulos, círculos, triángulos)
  5 – Barras verticales de escala de grises
  6 – Animación de barrido de líneas (frame continuo)
*/

static const uint16_t DISP_SOLID_COLORS[] = {
    C_RED, C_GREEN, C_BLUE, C_WHITE, C_BLACK,
    C_YELLOW, C_CYAN, C_ORANGE, C_ACCENT
};
static const char* DISP_SOLID_NAMES[] = {
    "Rojo","Verde","Azul","Blanco","Negro","Amarillo","Cyan","Naranja","Acento"
};
static const int DISP_SOLID_N = 9;

void SatMenu::_tickTestDisplay(Btn b) {
    int W = _tft->width(), H = _tft->height();

    if (b == Btn::BACK)  { _neo.clear(); _neo.show(); _goto(Scr::DIAG); return; }
    if (b == Btn::ENTER) { _dPat = (_dPat + 1) % 7; }
    if (b == Btn::UP)    { _dPat = (_dPat + 1) % 7; }
    if (b == Btn::DOWN)  { _dPat = (_dPat + 6) % 7; }  // prev

    switch (_dPat) {

        // ── 0: Colores sólidos ─────────────────────────────
        case 0: {
            static int ci = 0;
            if (b == Btn::ENTER || b == Btn::UP)   ci = (ci+1) % DISP_SOLID_N;
            if (b == Btn::DOWN)                     ci = (ci+DISP_SOLID_N-1) % DISP_SOLID_N;
            _tft->fillScreen(DISP_SOLID_COLORS[ci]);
            // Overlay con nombre del color
            uint16_t inv = ~DISP_SOLID_COLORS[ci];
            _tft->setTextColor(inv);
            _tft->setTextSize(2);
            _tft->setTextDatum(textdatum_t::middle_center);
            _tft->drawString(DISP_SOLID_NAMES[ci], W/2, H/2);
            _tft->setTextSize(1);
            _tft->drawString("SEL/SOL/REC=color  MUT=salir", W/2, H-14);
            break;
        }

        // ── 1: Gradiente horizontal ────────────────────────
        case 1: {
            for (int x = 0; x < W; x++) {
                uint8_t r = (x * 31) / W;
                uint8_t g = (x * 63) / W;
                uint8_t b16 = 31 - (x * 31) / W;
                uint16_t col = (r << 11) | (g << 5) | b16;
                _tft->drawFastVLine(x, 0, H, col);
            }
            _tft->setTextColor(C_WHITE);
            _tft->setTextSize(1);
            _tft->setTextDatum(textdatum_t::middle_center);
            _tft->drawString("Gradiente RGB — SEL=sig  MUT=salir", W/2, H/2);
            break;
        }

        // ── 2: Checker pattern ────────────────────────────
        case 2: {
            _tft->fillScreen(C_BLACK);
            for (int y = 0; y < H; y += 8)
                for (int x = 0; x < W; x += 8)
                    if (((x/8)+(y/8)) % 2 == 0)
                        _tft->fillRect(x, y, 8, 8, C_WHITE);
            _tft->setTextColor(C_ACCENT);
            _tft->setTextSize(1);
            _tft->setTextDatum(textdatum_t::middle_center);
            _tft->drawString("Checker 8px — SEL=sig  MUT=salir", W/2, H/2);
            break;
        }

        // ── 3: Texto multisize ─────────────────────────────
        case 3: {
            _tft->fillScreen(C_WHITE);
            int y = 4;
            _tft->setTextColor(C_BLACK);
            _tft->setTextDatum(textdatum_t::top_left);
            _tft->setTextSize(1); _tft->drawString("Texto tam 1 — ABCDEFGabcdefg0123", 2, y); y+=14;
            _tft->setTextSize(2); _tft->drawString("Tam 2 ABCabc123", 2, y); y+=22;
            _tft->setTextSize(3); _tft->drawString("Tam 3 ABC", 2, y); y+=30;
            _tft->setTextColor(C_ACCENT);
            _tft->setTextSize(2); _tft->drawString("Acento #e94560", 2, y); y+=22;
            _tft->setTextColor(C_BLUE);
            _tft->setTextSize(1); _tft->drawString("Track ID: 1  Label: CH-01  PWM: 40-220", 2, y); y+=16;
            _tft->setTextColor(C_DARK);
            _tft->setTextSize(1);
            _tft->drawString("SEL=sig  MUT=salir", 2, H-14);
            break;
        }

        // ── 4: Formas geométricas ─────────────────────────
        case 4: {
            _tft->fillScreen(C_BLACK);
            _tft->fillRect   (10, 40,  60, 40, C_RED);
            _tft->fillCircle (W/2, 80, 30,    C_GREEN);
            _tft->fillTriangle(10,H-20, W/2-10,H-80, W-10,H-20, C_BLUE);
            _tft->drawRect   (W-80,40,  60, 40, C_YELLOW);
            _tft->drawCircle (W/2, H/2, 20,    C_CYAN);
            _tft->drawRoundRect(20, H/2-20, 80, 40, 8, C_ACCENT);
            _tft->setTextColor(C_WHITE); _tft->setTextSize(1);
            _tft->setTextDatum(textdatum_t::bottom_center);
            _tft->drawString("Geometria — SEL=sig  MUT=salir", W/2, H-2);
            break;
        }

        // ── 5: Escala de grises ───────────────────────────
        case 5: {
            int bars = 32;
            int bw   = W / bars;
            for (int i = 0; i < bars; i++) {
                uint8_t v = (i * 255) / (bars - 1);
                uint8_t r5 = v >> 3, g6 = v >> 2, b5 = v >> 3;
                uint16_t col = (r5 << 11) | (g6 << 5) | b5;
                _tft->fillRect(i * bw, 0, bw, H - SAT_HINT_H, col);
            }
            _tft->fillRect(0, H-SAT_HINT_H, W, SAT_HINT_H, C_BLACK);
            _tft->setTextColor(C_WHITE); _tft->setTextSize(1);
            _tft->setTextDatum(textdatum_t::middle_center);
            _tft->drawString("Grises — SEL=sig  MUT=salir", W/2, H-SAT_HINT_H/2);
            break;
        }

        // ── 6: Barrido de líneas animado ─────────────────
        case 6: {
            static int lx = 0;
            static uint16_t lcol = C_ACCENT;
            // Borrar línea anterior (2px atrás)
            int prev = (lx + W - 4) % W;
            _tft->drawFastVLine(prev, 0, H - SAT_HINT_H, C_BLACK);
            // Dibujar línea nueva
            _tft->drawFastVLine(lx, 0, H - SAT_HINT_H, lcol);
            lx = (lx + 1) % W;
            if (lx == 0) {
                const uint16_t cols[] = {C_RED, C_GREEN, C_BLUE, C_CYAN, C_ACCENT};
                static int ci = 0; ci = (ci+1) % 5; lcol = cols[ci];
            }
            _tft->fillRect(0, H-SAT_HINT_H, W, SAT_HINT_H, C_BLACK);
            _tft->setTextColor(C_WHITE); _tft->setTextSize(1);
            _tft->setTextDatum(textdatum_t::middle_center);
            _tft->drawString("Barrido — SEL=sig  MUT=salir", W/2, H-SAT_HINT_H/2);
            break;
        }
    }
}

// ─────────────────────────────────────────────────────────────
//  ██████████  TEST ENCODER  ██████████
// ─────────────────────────────────────────────────────────────
/*
  Muestra en tiempo real:
  - Valor del contador  (grande, centro)
  - Dirección de la última rotación  (← →)
  - Estado del SW (presionado / libre)
  - Gráfica de barras del historial de posiciones
  - Pines A y B en crudo (HIGH/LOW)

  SOLO/REC = reset contador
  SELECT   = resetear historial
  MUTE     = salir
*/
void SatMenu::_tickTestEncoder(Btn b) {
    int W = _tft->width(), H = _tft->height();

    if (b == Btn::BACK) { _goto(Scr::DIAG); return; }
    if (b == Btn::UP || b == Btn::DOWN) { _encCnt = 0; memset(_encHist,0,ENC_HIST); }
    if (b == Btn::ENTER) { _encCnt = 0; memset(_encHist,0,ENC_HIST); }

    // ── Leer encoder (quadrature) ─────────────────────────
    int A = digitalRead(ENCODER_PIN_A);
    int B = digitalRead(ENCODER_PIN_B);
    unsigned long now = millis();

    if (A != _encLastA) {
        if (now - _encDebT > 3) {
            _encDebT = now;
            if (A == LOW) {
                _encCnt += (B == HIGH) ? 1 : -1;
                _encHist[_encHistIdx] = (int8_t)constrain(_encCnt, -63, 63);
                _encHistIdx = (_encHistIdx + 1) % ENC_HIST;
            }
        }
        _encLastA = A;
    }
    if (B != _encLastB) _encLastB = B;

    // ── Leer SW encoder ───────────────────────────────────
    bool sw = (digitalRead(ENCODER_SW_PIN) == LOW);
    _encSW = sw;

    // ── Dibujar pantalla ──────────────────────────────────
    _tft->fillScreen(C_BLACK);

    // Header
    _tft->fillRect(0, 0, W, SAT_HDR_H, C_BLACK);
    _tft->drawFastHLine(0, SAT_HDR_H-2, W, C_ACCENT);
    _tft->setTextColor(C_WHITE); _tft->setTextSize(1);
    _tft->setTextDatum(textdatum_t::middle_center);
    _tft->drawString("TEST ENCODER", W/2, SAT_HDR_H/2);

    int y = SAT_HDR_H + 6;

    // Pines A/B raw
    _tft->setTextDatum(textdatum_t::top_left);
    _tft->setTextSize(1);
    _tft->setTextColor(A==LOW ? C_GREEN : C_DARK, C_BLACK);
    _tft->drawString(A==LOW ? "A = LOW " : "A = HIGH", 6, y);
    _tft->setTextColor(B==LOW ? C_GREEN : C_DARK, C_BLACK);
    _tft->drawString(B==LOW ? "B = LOW " : "B = HIGH", W/2, y);
    y += 16;

    // SW
    _tft->setTextColor(sw ? C_YELLOW : C_DARK, C_BLACK);
    _tft->drawString(sw ? "SW = PULSADO  " : "SW = libre    ", 6, y);
    y += 18;

    // Contador (grande)
    char cbuf[8]; snprintf(cbuf, 8, "%+ld", _encCnt);
    _tft->setTextSize(4);
    _tft->setTextDatum(textdatum_t::middle_center);
    _tft->setTextColor(C_WHITE, C_BLACK);
    _tft->drawString(cbuf, W/2, y + 28);
    y += 64;

    // Dirección (flecha)
    _tft->setTextSize(2);
    int prev2 = (_encHistIdx + ENC_HIST - 1) % ENC_HIST;
    int prev3 = (_encHistIdx + ENC_HIST - 2) % ENC_HIST;
    const char* dir = "  =  ";
    if (_encHist[prev2] > _encHist[prev3])      dir = " >>  ";
    else if (_encHist[prev2] < _encHist[prev3]) dir = " <<  ";
    _tft->setTextColor(C_CYAN, C_BLACK);
    _tft->drawString(dir, W/2, y);
    y += 22;

    // Gráfica historial (barras verticales)
    int gW = W - 12;
    int gH = 40;
    int gX = 6;
    int gY = y;
    _tft->drawRect(gX-1, gY-1, gW+2, gH+2, C_DARK);

    int bw = gW / ENC_HIST;
    int cy = gY + gH/2;
    // línea central
    _tft->drawFastHLine(gX, cy, gW, C_DARK);

    for (int i = 0; i < ENC_HIST; i++) {
        int idx = (_encHistIdx + i) % ENC_HIST;
        int v   = _encHist[idx];
        int barH= abs(v) * (gH/2) / 63;
        uint16_t col = v >= 0 ? C_CYAN : C_ACCENT;
        int bx  = gX + i * bw;
        if (v >= 0)
            _tft->fillRect(bx, cy - barH, bw>1?bw-1:1, barH, col);
        else
            _tft->fillRect(bx, cy,        bw>1?bw-1:1, barH, col);
    }
    y += gH + 10;

    // Hints
    _tft->fillRect(0, H-SAT_HINT_H, W, SAT_HINT_H, C_BLACK);
    _tft->drawFastHLine(0, H-SAT_HINT_H, W, C_DARK);
    _tft->setTextSize(1); _tft->setTextColor(C_GRAY, C_BLACK);
    _tft->setTextDatum(textdatum_t::middle_left);
    _tft->drawString("SOL/REC=reset  SEL=hist  MUT=salir", 4, H-SAT_HINT_H/2);
}

// ─────────────────────────────────────────────────────────────
//  ██████████  TEST NEOPIXEL  ██████████
// ─────────────────────────────────────────────────────────────
/*
  Selector de pixel (0-3, 4=todos) + selector de color.
  Previsualización en pantalla con cuadros del color real.
  Animación rainbow con SELECT largo (toggle).

  SOLO  = siguiente pixel
  REC   = anterior pixel
  SELECT = siguiente color / activar
  MUTE  = salir
*/

static const uint32_t NEO_COLORS_RGB[] = {
    0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00,
    0xFF00FF, 0x00FFFF, 0xFFFFFF, 0xFF4500,
    0xE94560, 0x000000   // último = apagar
};
static const char* NEO_COLOR_NAMES[] = {
    "Rojo","Verde","Azul","Amarillo",
    "Magenta","Cyan","Blanco","Naranja",
    "Acento","Apagar"
};
static const int NEO_COLOR_N = 10;

void SatMenu::_tickTestNeopixel(Btn b) {
    int W = _tft->width(), H = _tft->height();

    if (b == Btn::BACK) {
        _neo.clear(); _neo.show();
        _goto(Scr::DIAG); return;
    }
    if (b == Btn::UP)   _neoSel = (_neoSel + 1) % 5;        // 0-3 + "todos"
    if (b == Btn::DOWN) _neoSel = (_neoSel + 4) % 5;
    if (b == Btn::ENTER){
        _neoColorIdx = (_neoColorIdx + 1) % NEO_COLOR_N;
        _neoAnim = false;
    }

    // Aplicar color
    if (!_neoAnim) {
        uint32_t c = NEO_COLORS_RGB[_neoColorIdx];
        uint8_t r = (c >> 16) & 0xFF;
        uint8_t g = (c >>  8) & 0xFF;
        uint8_t bv= (c      ) & 0xFF;
        if (_neoSel == 4) {
            for (int i = 0; i < NEOPIXEL_COUNT; i++)
                _neo.setPixelColor(i, _neo.Color(r, g, bv));
        } else {
            _neo.clear();
            _neo.setPixelColor(_neoSel, _neo.Color(r, g, bv));
        }
        _neo.show();
    } else {
        // Animación rainbow
        unsigned long now = millis();
        if (now - _neoAnimT > 30) {
            _neoAnimT = now;
            for (int i = 0; i < NEOPIXEL_COUNT; i++) {
                uint8_t hue = (_neoAnimStep + i * 64) & 0xFF;
                // simple HSV→RGB
                uint8_t r2,g2,b2;
                uint8_t h6 = hue / 43;
                uint8_t f  = (hue - h6 * 43) * 6;
                uint8_t q  = 255 - f;
                switch (h6 % 6) {
                    case 0: r2=255;g2=f  ;b2=0  ; break;
                    case 1: r2=q  ;g2=255;b2=0  ; break;
                    case 2: r2=0  ;g2=255;b2=f  ; break;
                    case 3: r2=0  ;g2=q  ;b2=255; break;
                    case 4: r2=f  ;g2=0  ;b2=255; break;
                    default:r2=255;g2=0  ;b2=q  ; break;
                }
                _neo.setPixelColor(i, _neo.Color(r2, g2, b2));
            }
            _neo.show();
            _neoAnimStep = (_neoAnimStep + 2) & 0xFF;
        }
    }

    // ── Dibujar pantalla ──────────────────────────────────
    _tft->fillScreen(C_BLACK);
    _drawHdr("TEST NEOPIXEL");

    int y = SAT_HDR_H + 10;

    // Selector de pixel
    _tft->setTextColor(C_WHITE, C_BLACK);
    _tft->setTextSize(1);
    _tft->setTextDatum(textdatum_t::middle_left);
    _tft->drawString("Pixel:", 6, y + 10);

    for (int i = 0; i < 5; i++) {
        int bx = 55 + i * 36;
        bool sel = (i == _neoSel);
        _tft->fillRoundRect(bx, y, 30, 20, 4,
            sel ? C_ACCENT : C_DARK);
        _tft->setTextColor(C_WHITE, sel ? C_ACCENT : C_DARK);
        _tft->setTextDatum(textdatum_t::middle_center);
        char lb[4]; snprintf(lb, 4, i<4 ? "%d" : "ALL", i);
        _tft->drawString(lb, bx + 15, y + 10);
    }
    y += 30;

    // Selector de color
    _tft->setTextColor(C_WHITE, C_BLACK);
    _tft->setTextDatum(textdatum_t::middle_left);
    _tft->drawString("Color:", 6, y + 10);

    // 10 cuadros de color
    int cx0 = 55;
    for (int i = 0; i < NEO_COLOR_N; i++) {
        int bx = cx0 + (i % 5) * 34;
        int by = y   + (i / 5) * 26;
        uint32_t c = NEO_COLORS_RGB[i];
        // convertir RGB888 → RGB565
        uint16_t col565 = ((c>>8)&0xF800) | ((c>>5)&0x07E0) | ((c>>3)&0x001F);
        bool sel = (i == _neoColorIdx);
        _tft->fillRect(bx, by, 30, 22,
            i == 9 ? C_DARK : col565);  // apagar = gris
        if (sel)
            _tft->drawRect(bx-1, by-1, 32, 24, C_WHITE);
    }
    y += 60;

    // Nombre color seleccionado
    _tft->setTextSize(1); _tft->setTextColor(C_ACCENT, C_BLACK);
    _tft->setTextDatum(textdatum_t::middle_center);
    _tft->drawString(NEO_COLOR_NAMES[_neoColorIdx], W/2, y);
    y += 18;

    // Preview grande del color seleccionado
    if (_neoSel < 4) {
        uint32_t c = NEO_COLORS_RGB[_neoColorIdx];
        uint16_t col565 = ((c>>8)&0xF800)|((c>>5)&0x07E0)|((c>>3)&0x001F);
        _tft->fillRoundRect(W/2-40, y, 80, 30, 6, col565);
        _tft->drawRoundRect(W/2-40, y, 80, 30, 6, C_WHITE);
    } else {
        // Mostrar los 4 colores activos
        for (int i = 0; i < 4; i++) {
            uint32_t c = NEO_COLORS_RGB[_neoColorIdx];
            uint16_t col565 = ((c>>8)&0xF800)|((c>>5)&0x07E0)|((c>>3)&0x001F);
            _tft->fillRoundRect(W/2 - 68 + i*36, y, 30, 30, 4, col565);
        }
    }
    y += 40;

    // Hints
    _tft->fillRect(0, H-SAT_HINT_H, W, SAT_HINT_H, C_BLACK);
    _tft->drawFastHLine(0, H-SAT_HINT_H, W, C_DARK);
    _tft->setTextSize(1); _tft->setTextColor(C_GRAY, C_BLACK);
    _tft->setTextDatum(textdatum_t::middle_left);
    _tft->drawString("SOL/REC=pixel  SEL=color  MUT=salir", 4, H-SAT_HINT_H/2);
}

// ─────────────────────────────────────────────────────────────
//  ██████████  TEST FADER  ██████████
// ─────────────────────────────────────────────────────────────
/*
  Pantalla dividida en 3 zonas:

  ┌─────────────────────────────┐
  │  FADER    ADC raw / %        │  zona 1: barra posición fader
  │  ████████████░░░░  72.3%     │
  │  [min 0   max 4095]          │
  ├─────────────────────────────┤
  │  TOUCH    raw / estado       │  zona 2: touch
  │  ●  TOCADO   raw=2400        │
  ├─────────────────────────────┤
  │  MOTOR   PWM: +150  [▲▲▲]   │  zona 3: motor manual
  │  IN1=PWM  IN2=0              │
  ├─────────────────────────────┤
  │  Gráfica historial fader     │  zona 4: miniplot 80 muestras
  └─────────────────────────────┘

  SOLO  = motor +pwm (subir)
  REC   = motor -pwm (bajar)
  SELECT = motor stop / calibrar (long press)
  MUTE  = salir
*/

static int _touchRead() {
    return (int)touchRead(FADER_TOUCH_PIN);   // Arduino API, no init necesario
}

void SatMenu::_tickTestFader(Btn b) {
    int W = _tft->width(), H = _tft->height();

    if (b == Btn::BACK)  {
        _motorStop();
        _goto(Scr::DIAG); return;
    }

    // Motor manual
    if (b == Btn::UP)    { _motPWM = constrain(_motPWM + 20, -255, 255); _motorDrive(_motPWM); }
    if (b == Btn::DOWN)  { _motPWM = constrain(_motPWM - 20, -255, 255); _motorDrive(_motPWM); }
    if (b == Btn::ENTER) { _motPWM = 0; _motorStop();
                           _fadCalMin = 4095; _fadCalMax = 0; }  // reset calibración

    // ── Leer ADC fader ───────────────────────────────────
    unsigned long now = millis();
    if (now - _fadT > 25) {
        _fadT = now;
        // Múltiples lecturas y promedio
        int sum = 0;
        for (int i = 0; i < 8; i++) sum += analogRead(FADER_POT_PIN);
        _fadRaw = sum / 8;
        if (_fadRaw < _fadCalMin) _fadCalMin = _fadRaw;
        if (_fadRaw > _fadCalMax) _fadCalMax = _fadRaw;
        int range = _fadCalMax - _fadCalMin;
        _fadPct = range > 50
            ? (float)(_fadRaw - _fadCalMin) / range
            : (float)_fadRaw / 4095.0f;
        _fadPct = constrain(_fadPct, 0.0f, 1.0f);

        // Actualizar historial
        if (now - _fadHistT > 50) {
            _fadHistT = now;
            _fadHist[_fadHistIdx] = (uint8_t)(_fadPct * 255);
            _fadHistIdx = (_fadHistIdx + 1) % FAD_HIST;
        }
    }

    // ── Leer touch ───────────────────────────────────────
    _tchRaw = _touchRead();
    // El ESP32-S2 touch: valor alto = sin contacto, bajo = tocado
    static int tchBase = 0;
    if (tchBase == 0 && _tchRaw > 100) tchBase = _tchRaw;
    _tchOn = (tchBase > 0) && (_tchRaw < (int)(tchBase * 0.80f));

    // ── Dibujar ──────────────────────────────────────────
    _tft->fillScreen(C_BLACK);
    _drawHdr("TEST FADER");

    int y  = SAT_HDR_H + 6;
    int mx = W - 8;

    // ── Zona 1: ADC / Posición ────────────────────────────
    _tft->setTextColor(C_CYAN, C_BLACK);
    _tft->setTextSize(1); _tft->setTextDatum(textdatum_t::top_left);
    _tft->drawString("FADER", 4, y);

    char buf[32];
    snprintf(buf, 32, "raw=%4d  %.1f%%  [%d-%d]",
             _fadRaw, _fadPct*100.f, _fadCalMin, _fadCalMax);
    _tft->setTextColor(C_WHITE, C_BLACK);
    _tft->drawString(buf, 44, y);
    y += 14;

    _drawHBar(4, y, W-8, 14, _fadPct, C_CYAN);
    y += 20;

    // Indicador de unidad gain (75%)
    int ugX = 4 + (int)((W-8) * 0.75f);
    _tft->drawFastVLine(ugX, y-20, 20, C_YELLOW);
    _tft->setTextColor(C_YELLOW, C_BLACK);
    _tft->setTextDatum(textdatum_t::top_center);
    _tft->drawString("0dB", ugX, y-8);

    // ── Zona 2: Touch ─────────────────────────────────────
    _drawDivider(y+4); y += 10;

    _tft->setTextColor(C_YELLOW, C_BLACK);
    _tft->setTextDatum(textdatum_t::top_left);
    _tft->drawString("TOUCH", 4, y);

    uint16_t tCol = _tchOn ? C_GREEN : C_DARK;
    _tft->fillCircle(55, y+5, 6, tCol);
    snprintf(buf, 32, " %s  raw=%5d  base=%5d",
             _tchOn ? "TOCADO  " : "libre   ", _tchRaw, tchBase);
    _tft->setTextColor(_tchOn ? C_GREEN : C_GRAY, C_BLACK);
    _tft->drawString(buf, 62, y);
    y += 20;

    // Barra touch ratio
    if (tchBase > 0) {
        float ratio = constrain(1.0f - (float)_tchRaw / tchBase, 0.0f, 1.0f);
        _drawHBar(4, y, W-8, 10, ratio, _tchOn ? C_GREEN : C_DARK);
    }
    y += 16;

    // ── Zona 3: Motor ─────────────────────────────────────
    _drawDivider(y+2); y += 8;

    _tft->setTextColor(C_ORANGE, C_BLACK);
    _tft->setTextDatum(textdatum_t::top_left);
    _tft->drawString("MOTOR", 4, y);

    // Barra PWM -255..+255 centrada
    float mPct = ((float)_motPWM + 255.f) / 510.f;
    int bx = 44, bw = W - 48;
    _tft->fillRect(bx, y, bw, 14, C_DARK);
    int center = bx + bw/2;
    int barEnd = bx + (int)(mPct * bw);
    if (barEnd >= center)
        _tft->fillRect(center, y, barEnd-center, 14, _motPWM > 0 ? C_GREEN : C_DARK);
    else
        _tft->fillRect(barEnd, y, center-barEnd, 14, C_ACCENT);
    // Línea central
    _tft->drawFastVLine(center, y, 14, C_WHITE);

    snprintf(buf, 32, " PWM %+d", _motPWM);
    _tft->setTextColor(C_WHITE, C_BLACK);
    _tft->drawString(buf, bx + bw + 2, y);
    y += 18;

    bool in1H = (_motPWM > 0), in2H = (_motPWM < 0);
    snprintf(buf, 32, "IN1=%s  IN2=%s  EN=%s",
             in1H?"PWM":"0", in2H?"PWM":"0",
             (abs(_motPWM) > 0)?"HIGH":"LOW");
    _tft->setTextColor(C_GRAY, C_BLACK); _tft->drawString(buf, 44, y);
    y += 16;

    // ── Zona 4: Gráfica historial ─────────────────────────
    _drawDivider(y); y += 4;

    int gH = H - SAT_HINT_H - y - 4;
    if (gH < 20) gH = 20;
    int gW = W - 8;
    _tft->drawRect(3, y, gW+2, gH+2, C_DARK);

    // Baseline 0dB (75%)
    int ugY = y + gH - (int)(0.75f * gH);
    _tft->drawFastHLine(4, ugY, gW, C_YELLOW);

    int pxW = gW / FAD_HIST;
    if (pxW < 1) pxW = 1;
    for (int i = 0; i < FAD_HIST; i++) {
        int idx = (_fadHistIdx + i) % FAD_HIST;
        int fh  = (_fadHist[idx] * gH) / 255;
        _tft->fillRect(4 + i * (gW / FAD_HIST), y + gH - fh, pxW, fh+1, C_CYAN);
    }
    y += gH + 4;

    // Hints
    _tft->fillRect(0, H-SAT_HINT_H, W, SAT_HINT_H, C_BLACK);
    _tft->drawFastHLine(0, H-SAT_HINT_H, W, C_DARK);
    _tft->setTextSize(1); _tft->setTextColor(C_GRAY, C_BLACK);
    _tft->setTextDatum(textdatum_t::middle_left);
    _tft->drawString("SOL=mot+  REC=mot-  SEL=stop/cal  MUT=salir", 4, H-SAT_HINT_H/2);
}

// ─────────────────────────────────────────────────────────────
//  Handlers menú (sin cambios respecto v1, compactos)
// ─────────────────────────────────────────────────────────────
void SatMenu::_hMain(Btn b) {
    if (b == Btn::UP)    { if (_cur > 0) { _cur--; _dirty=true; } }
    if (b == Btn::DOWN)  { if (_cur < _mainN-1) { _cur++; _dirty=true; } }
    if (b == Btn::BACK)  { close(); return; }
    if (b == Btn::ENTER) {
        switch (_mainItems[_cur].target) {
            case Scr::IDENTIDAD:   _goto(Scr::IDENTIDAD);   break;
            case Scr::MOTOR:       _goto(Scr::MOTOR);       break;
            case Scr::TOUCH:       _goto(Scr::TOUCH);       break;
            case Scr::DIAG:        _goto(Scr::DIAG);        break;
            case Scr::CONFIG_WIFI:
                if (_cbWiFi) _cbWiFi();
                _toast("Lanzando WiFiManager...", Scr::MAIN);
                break;
            case Scr::REINICIAR:
                _confirm("Reiniciar el dispositivo?", Scr::REINICIAR);
                break;
            default: break;
        }
    }
}
void SatMenu::_hIdent(Btn b) {
    if (b == Btn::UP)    { if (_cur > 0) { _cur--; _dirty=true; } }
    if (b == Btn::DOWN)  { if (_cur < _identN-1) { _cur++; _dirty=true; } }
    if (b == Btn::BACK)  _goto(Scr::MAIN);
    if (b == Btn::ENTER) {
        if (_cur == 0) { _eTitle="Track ID"; _eVal=max((int)_tmp.trackId, 1); _eMin=1; _eMax=8; _goto(Scr::EDIT_TRACKID); }
        else           { _lblIdx=0; _goto(Scr::EDIT_LABEL); }
    }
}
void SatMenu::_hMotor(Btn b) {
    if (b == Btn::UP)    { if (_cur > 0) { _cur--; _dirty=true; } }
    if (b == Btn::DOWN)  { if (_cur < _motorN-1) { _cur++; _dirty=true; } }
    if (b == Btn::BACK)  _goto(Scr::MAIN);
    if (b == Btn::ENTER) {
        if (_cur == 0) { _eTitle="PWM Minimo"; _eVal=_tmp.pwmMin; _eMin=0;  _eMax=120; _goto(Scr::EDIT_PWMMIN); }
        else           { _eTitle="PWM Maximo"; _eVal=_tmp.pwmMax; _eMin=50; _eMax=255; _goto(Scr::EDIT_PWMMAX); }
    }
}
void SatMenu::_hTouch(Btn b) {
    if (b == Btn::UP)    { if (_cur > 0) { _cur--; _dirty=true; } }
    if (b == Btn::DOWN)  { if (_cur < _touchN-1) { _cur++; _dirty=true; } }
    if (b == Btn::BACK)  _goto(Scr::MAIN);
    if (b == Btn::ENTER) {
        if (_cur == 0) {
            _tmp.touchEnabled = !_tmp.touchEnabled;
            _cfg.touchEnabled =  _tmp.touchEnabled;
            _save(); if (_cbSaved) _cbSaved(_cfg);
            _toast(_tmp.touchEnabled ? "Touch: ON" : "Touch: OFF", Scr::TOUCH);
        } else {
            _eTitle="Umbral Touch"; _eVal=_tmp.touchThreshold; _eMin=10; _eMax=90;
            _goto(Scr::EDIT_TOUCHTHR);
        }
    }
}
void SatMenu::_hDiag(Btn b) {
    if (b == Btn::UP)    { if (_cur > 0) { _cur--; _dirty=true; } }
    if (b == Btn::DOWN)  { if (_cur < _diagN-1) { _cur++; _dirty=true; } }
    if (b == Btn::BACK)  _goto(Scr::MAIN);
    if (b == Btn::ENTER) {
        switch (_cur) {
            case 0: _dPat=0; _goto(Scr::TEST_DISPLAY);  break;
            case 1: _encCnt=0; memset(_encHist,0,ENC_HIST); _goto(Scr::TEST_ENCODER);  break;
            case 2: _neoSel=0; _neoColorIdx=0; _goto(Scr::TEST_NEOPIXEL); break;
            case 3: _motPWM=0; _fadCalMin=4095; _fadCalMax=0;
                    _fadHistIdx=0; memset(_fadHist,0,FAD_HIST);
                    _goto(Scr::TEST_FADER); break;
        }
    }
}
void SatMenu::_hEditVal(Btn b) {
    if (b == Btn::UP)    { if (_eVal < _eMax) { _eVal++; _dirty=true; } }
    if (b == Btn::DOWN)  { if (_eVal > _eMin) { _eVal--; _dirty=true; } }
    if (b == Btn::BACK)  _back();
    if (b == Btn::ENTER) {
        switch (_scr) {
            case Scr::EDIT_TRACKID: _cfg.trackId=_tmp.trackId=(uint8_t)_eVal; break;
            case Scr::EDIT_PWMMIN:  _cfg.pwmMin =_tmp.pwmMin =(uint8_t)_eVal; break;
            case Scr::EDIT_PWMMAX:  _cfg.pwmMax =_tmp.pwmMax =(uint8_t)_eVal; break;
            case Scr::EDIT_TOUCHTHR:_cfg.touchThreshold=_tmp.touchThreshold=(uint8_t)_eVal; break;
            default: break;
        }
        _save(); if (_cbSaved) _cbSaved(_cfg);
        _toast("Guardado!", _prev);
    }
}
void SatMenu::_hEditLbl(Btn b) {
    if (b == Btn::UP) {
        char& c = _tmp.label[_lblIdx];
        c = (c>='A'&&c<'Z')?c+1:(c=='Z')?'a':(c>='a'&&c<'z')?c+1:(c=='z')?'0':(c>='0'&&c<'9')?c+1:(c=='9')?' ':' ';
        _dirty=true;
    }
    if (b == Btn::DOWN) {
        char& c = _tmp.label[_lblIdx];
        c = (c=='A')?' ':(c==' ')?'9':(c=='0')?'z':(c=='a')?'Z':(c>'a')?c-1:(c>'0')?c-1:(c>'A')?c-1:c;
        _dirty=true;
    }
    if (b == Btn::BACK) {
        if (_lblIdx > 0) { _lblIdx--; _dirty=true; }
        else { memcpy(_tmp.label, _cfg.label, 7); _goto(Scr::IDENTIDAD); }
    }
    if (b == Btn::ENTER) {
        if (_lblIdx < 5) { _lblIdx++; _dirty=true; }
        else {
            memcpy(_cfg.label, _tmp.label, 7);
            _save(); if (_cbSaved) _cbSaved(_cfg);
            _toast("Etiqueta guardada!", Scr::IDENTIDAD);
        }
    }
}
void SatMenu::_hConfirm(Btn b) {
    if (b == Btn::ENTER) {
        if (_confYes == Scr::REINICIAR) {
            _toast("Reiniciando...", Scr::MAIN);
            delay(900);
            if (_cbReboot) _cbReboot(); else ESP.restart();
        } else _goto(_confYes);
    }
    if (b == Btn::BACK) _back();
}
void SatMenu::_hToast(Btn b) {
    if (millis() - _toastT > 1300 || b != Btn::NONE) _goto(_toastRet);
}

// ─────────────────────────────────────────────────────────────
//  Primitivas de dibujo
// ─────────────────────────────────────────────────────────────
void SatMenu::_drawHdr(const char* t) {
    int W = _tft->width();
    _tft->fillRect(0, 0, W, SAT_HDR_H, C_BLACK);
    _tft->setTextColor(C_WHITE, C_BLACK);
    _tft->setTextSize(1);
    _tft->setTextDatum(textdatum_t::middle_center);
    _tft->drawString(t, W/2, SAT_HDR_H/2);
    _tft->fillRect(0, SAT_HDR_H-2, W, 2, C_ACCENT);
}
void SatMenu::_drawHints(const char* u, const char* d, const char* b, const char* e) {
    int W = _tft->width(), H = _tft->height();
    int y = H - SAT_HINT_H;
    _tft->fillRect(0, y, W, SAT_HINT_H, C_BLACK);
    _tft->drawFastHLine(0, y, W, C_DARK);
    _tft->setTextSize(1); _tft->setTextColor(C_GRAY, C_BLACK);
    const char* lbs[4]  = { u, d, b, e };
    const char* bns[4]  = { "REC","SOL","MUT","SEL" };
    int w4 = W / 4;
    for (int i = 0; i < 4; i++) {
        if (lbs[i] && lbs[i][0]) {
            char buf[16]; snprintf(buf, 16, "%s:%s", bns[i], lbs[i]);
            _tft->setTextDatum(textdatum_t::middle_center);
            _tft->drawString(buf, w4*i + w4/2, y + SAT_HINT_H/2);
        }
    }
}
void SatMenu::_drawList(const Item* items, int n) {
    int W = _tft->width(), H = _tft->height();
    int aH = H - SAT_HDR_H - SAT_HINT_H;
    int vis = aH / SAT_ROW_H; if (vis > n) vis = n;
    if (_cur < _scrl) _scrl = _cur;
    if (_cur >= _scrl + vis) _scrl = _cur - vis + 1;
    for (int i = 0; i < vis; i++) {
        int idx = i + _scrl;
        if (idx >= n) break;
        int y = SAT_HDR_H + i * SAT_ROW_H;
        bool sel = (idx == _cur);
        uint16_t bg = sel ? C_ACCENT : (idx%2==0 ? C_BG : C_GRAY);
        _tft->fillRect(0, y, W, SAT_ROW_H, bg);
        _drawBadge(6, y+(SAT_ROW_H-SAT_BADGE_H)/2, items[idx].badge,
            sel?C_WHITE:C_ACCENT, sel?C_ACCENT:C_WHITE);
        _tft->setTextColor(sel?C_WHITE:C_TEXT, bg);
        _tft->setTextSize(1); _tft->setTextDatum(textdatum_t::middle_left);
        _tft->drawString(items[idx].label, SAT_BADGE_W+14, y+SAT_ROW_H/2);
        _tft->setTextDatum(textdatum_t::middle_right);
        _tft->drawString(">", W-8, y+SAT_ROW_H/2);
        if (!sel) _drawDivider(y+SAT_ROW_H-1);
    }
    if (n > vis) {
        int bH = (aH * vis) / n;
        int bY = SAT_HDR_H + (aH * _scrl) / n;
        _tft->fillRect(W-4, SAT_HDR_H, 4, aH, C_GRAY);
        _tft->fillRect(W-4, bY, 4, bH, C_ACCENT);
    }
}
void SatMenu::_drawValEdit(const char* t, int v, int mn, int mx, const char* u) {
    int W = _tft->width(), H = _tft->height();
    _drawHdr(t);
    int cx = W/2, cy = H/2;
    _tft->setTextColor(C_ACCENT, C_BG); _tft->setTextSize(3);
    _tft->setTextDatum(textdatum_t::middle_center);
    _tft->drawString("^", cx, cy-46);
    _tft->drawString("v", cx, cy+46);
    char buf[12]; snprintf(buf, 12, "%d%s", v, u);
    _tft->setTextColor(C_TEXT, C_BG); _tft->setTextSize(4);
    _tft->drawString(buf, cx, cy);
    char rb[16]; snprintf(rb, 16, "[%d - %d]", mn, mx);
    _tft->setTextSize(1); _tft->setTextColor(C_DARK, C_BG);
    _tft->drawString(rb, cx, cy+28);
    _drawHints("","","Cancel","Guardar");
}
void SatMenu::_drawLblEdit() {
    int W = _tft->width(), H = _tft->height();
    int cx = W/2, cy = H/2;
    int charW = 32;
    int startX = cx - 3 * charW;
    _tft->setTextColor(C_TEXT, C_BG); _tft->setTextSize(1);
    _tft->setTextDatum(textdatum_t::middle_center);
    _tft->drawString("Navega con MUT/SEL, cambia con SOL/REC", cx, SAT_HDR_H + 18);
    for (int i = 0; i < 6; i++) {
        int x = startX + i * charW;
        bool active = (i == _lblIdx);
        _tft->fillRoundRect(x, cy-18, charW-2, 34, 4, active ? C_ACCENT : C_GRAY);
        char c[2] = { _tmp.label[i], 0 };
        _tft->setTextColor(active ? C_WHITE : C_TEXT, active ? C_ACCENT : C_GRAY);
        _tft->setTextSize(2); _tft->setTextDatum(textdatum_t::middle_center);
        _tft->drawString(c, x+(charW-2)/2, cy);
    }
    _tft->setTextColor(C_ACCENT, C_BG); _tft->setTextSize(1);
    _tft->setTextDatum(textdatum_t::middle_center);
    _tft->drawString("^", startX + _lblIdx*charW + (charW-2)/2, cy+24);
}
void SatMenu::_drawConfirm(const char* msg) {
    int W = _tft->width(), H = _tft->height();
    _drawHdr("Confirmar");
    int cx=W/2, cy=H/2;
    _tft->setTextColor(C_TEXT, C_BG); _tft->setTextSize(1);
    _tft->setTextDatum(textdatum_t::middle_center);
    _tft->drawString(msg, cx, cy-18);
    _tft->fillRoundRect(cx-68, cy+8, 54, 28, 6, C_ACCENT);
    _tft->fillRoundRect(cx+14, cy+8, 54, 28, 6, C_GRAY);
    _tft->setTextColor(C_WHITE, C_ACCENT); _tft->drawString("SI (SEL)", cx-41, cy+22);
    _tft->setTextColor(C_TEXT,  C_GRAY);   _tft->drawString("NO (MUT)", cx+41, cy+22);
}
void SatMenu::_drawToast(const char* msg) {
    int W = _tft->width(), H = _tft->height();
    int tw = W-40, th=40, tx=20, ty=H/2-20;
    _tft->fillRoundRect(tx, ty, tw, th, 8, C_BLACK);
    _tft->drawRoundRect(tx, ty, tw, th, 8, C_ACCENT);
    _tft->setTextColor(C_WHITE, C_BLACK); _tft->setTextSize(1);
    _tft->setTextDatum(textdatum_t::middle_center);
    _tft->drawString(msg, W/2, ty+th/2);
}
void SatMenu::_drawBadge(int x, int y, const char* t, uint16_t bg, uint16_t fg) {
    _tft->fillRoundRect(x, y, SAT_BADGE_W, SAT_BADGE_H, 3, bg);
    _tft->setTextColor(fg, bg); _tft->setTextSize(1);
    _tft->setTextDatum(textdatum_t::middle_center);
    _tft->drawString(t, x+SAT_BADGE_W/2, y+SAT_BADGE_H/2);
}
void SatMenu::_drawHBar(int x, int y, int w, int h, float pct, uint16_t col) {
    _tft->fillRect(x, y, w, h, C_DARK);
    int fw = (int)(w * constrain(pct, 0.0f, 1.0f));
    if (fw > 0) _tft->fillRect(x, y, fw, h, col);
    _tft->drawRect(x, y, w, h, C_GRAY);
}
void SatMenu::_drawDivider(int y) {
    _tft->drawFastHLine(0, y, _tft->width(), C_GRAY);
}

// ─────────────────────────────────────────────────────────────
//  Motor helpers
// ─────────────────────────────────────────────────────────────
void SatMenu::_motorStop() {
    if (_cbMotorDrv) { _cbMotorDrv(0); return; }
    digitalWrite(MOTOR_EN,  LOW);
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
}
void SatMenu::_motorDrive(int pwm) {
    if (_cbMotorDrv) { _cbMotorDrv(pwm); return; }
    if (pwm == 0) { _motorStop(); return; }
    digitalWrite(MOTOR_EN, HIGH);
    if (pwm > 0) {
        analogWrite(MOTOR_IN1, constrain(pwm, 0, 255));
        digitalWrite(MOTOR_IN2, LOW);
    } else {
        digitalWrite(MOTOR_IN1, LOW);
        analogWrite(MOTOR_IN2, constrain(-pwm, 0, 255));
    }
}

// ─────────────────────────────────────────────────────────────
//  NVS
// ─────────────────────────────────────────────────────────────
void SatMenu::_load() {
    _prefs.begin("ptxx", true);
    _cfg.trackId        = _prefs.getUChar("trackId", 0);  // 0 = no configurado → SAT al arrancar
    _cfg.pwmMin         = _prefs.getUChar("pwmMin",  40);
    _cfg.pwmMax         = _prefs.getUChar("pwmMax",  220);
    _cfg.touchEnabled   = _prefs.getBool ("touchEn", true);
    _cfg.touchThreshold = _prefs.getUChar("touchThr",80);
    String lbl = _prefs.getString("label","CH-01 ");
    strncpy(_cfg.label, lbl.c_str(), 6); _cfg.label[6]='\0';
    _prefs.end();
}
void SatMenu::_save() {
    _prefs.begin("ptxx", false);
    _prefs.putUChar("trackId",  _cfg.trackId);
    _prefs.putUChar("pwmMin",   _cfg.pwmMin);
    _prefs.putUChar("pwmMax",   _cfg.pwmMax);
    _prefs.putBool ("touchEn",  _cfg.touchEnabled);
    _prefs.putUChar("touchThr", _cfg.touchThreshold);
    _prefs.putString("label",   String(_cfg.label).substring(0,6));
    _prefs.end();
}

// ─────────────────────────────────────────────────────────────
//  Toast / Confirm
// ─────────────────────────────────────────────────────────────
void SatMenu::_toast(const char* msg, Scr ret) {
    _toastMsg=msg; _toastRet=ret; _toastT=millis();
    _scr=Scr::TOAST; _dirty=true;
}
void SatMenu::_confirm(const char* msg, Scr yes) {
    _confMsg=msg; _confYes=yes; _prev=_scr;
    _scr=Scr::CONFIRM; _dirty=true;
}