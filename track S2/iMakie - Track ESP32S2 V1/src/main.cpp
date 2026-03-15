// ============================================================
//  main.cpp  —  iMakie PTxx Track S2
//  Solo orquesta. Cero lógica de negocio aquí.
// ============================================================
#include <Arduino.h>
#include "config.h"
#include "display/Display.h"
#include "display/LovyanGFX_config.h"
#include "hardware/encoder/Encoder.h"
#include "hardware/Hardware.h"
#include "hardware/Neopixels/Neopixel.h"
#include "hardware/Motor/Motor.h"
#include "RS485/RS485.h"
#include "RS485/RS485Handler.h"       // ← onMasterData vive aquí
#include "protocol.h"
#include "button/ButtonManager.h"
#include "menu/SatMenu.h"
#include "fader/FaderADC.h"

// ─── Objetos globales ──────────────────────────────────────────
LGFX        tft;
LGFX_Sprite header(&tft), mainArea(&tft), vuSprite(&tft), vPotSprite(&tft);
FaderADC    faderADC;

// ─── Estado de canal (externs consumidos por Display / Hardware) ──
String trackName        = "Track  ";
bool  recStates    = false;
bool  soloStates   = false;
bool  muteStates   = false;
bool  selectStates = true;
bool  vuClipState  = false;
float vuPeakLevels    = 0.0f;
float faderPositions  = 0.0f;
float vuLevels        = 0.0f;
unsigned long vuLastUpdateTime     = 0;
unsigned long vuPeakLastUpdateTime = 0;

// ─── Estado de conexión ───────────────────────────────────────
static bool _suspended = false;

// ─── SAT menu ─────────────────────────────────────────────────
static SatMenu* satMenu = nullptr;

// =============================================================
//  setup
// =============================================================
void setup() {
    // Motor pins — PRIMERO, antes de cualquier init
    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    pinMode(MOTOR_EN,  OUTPUT);
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    digitalWrite(MOTOR_EN,  LOW);

    dacWrite(FADER_VCC_PIN, 77);
    delay(30);
    faderADC.begin();

    initDisplay();       // 1. LovyanGFX — reserva periféricos SPI/DMA
    initNeopixels();     // 2. NeoPixelBus I2S — DESPUÉS del display
    initHardware();      // 3. Botones, encoder, touch
    setVPotLevel(VPOT_DEFAULT_LEVEL);
    Encoder::begin();

    // SAT menu
    satMenu = new SatMenu(&tft);
    satMenu->onMotorOff  ([]() { Motor::stop();    _suspended = true;  });
    satMenu->onMotorOn   ([]() { Motor::begin();   _suspended = false; });
    satMenu->onMotorDrive([](int pwm) { Motor::driveRaw(pwm); });
    satMenu->onRS485Off  ([]() { _suspended = true;  });
    satMenu->onRS485On   ([]() { _suspended = false; });
    satMenu->onReboot    ([]() { ESP.restart(); });
    satMenu->onBrightness([](uint8_t b) { setScreenBrightness(b); }, 255);
    satMenu->onConfigSaved([](const SatConfig& cfg) {
        rs485.begin(cfg.trackId);
    });

    uint8_t slaveId = satMenu->getConfig().trackId;
    rs485.begin(slaveId);
    if (slaveId == 0) satMenu->open();

    ButtonManager::begin(&tft, satMenu);

    Motor::begin();      // ÚLTIMO — todo ya inicializado
}

// =============================================================
//  loop
// =============================================================
void loop() {
    // SAT abierto: solo menú
    if (satMenu && satMenu->isOpen()) {
        satMenu->update();
        return;
    }

    // Long-press REC → barra de progreso
    ButtonManager::update();
    if (satMenu && satMenu->isOpen()) return;

    // ── RS485 ────────────────────────────────────────────────
    if (!_suspended) {
        rs485.update();
        static unsigned long lastRxTime = 0;

        if (rs485.hasNewData()) {
            lastRxTime = millis();
            RS485Handler::onMasterData(rs485.getData());   // ← lógica en su módulo

            SlavePacket resp = RS485Handler::buildResponse(faderADC, *satMenu);
            rs485.sendResponse(resp);

            ButtonManager::clearButtonFlags();
            ButtonManager::clearEncoderButton();
            Encoder::reset();
        }

        RS485Handler::checkTimeout(lastRxTime);            // desconexión por silencio
    }

    // ── Fader + Motor ─────────────────────────────────────────
    faderADC.update();
    Motor::setADC(faderADC.getFaderPos());

    if (isFaderTouched) {
        Motor::stop();
    } else {
        Motor::update();
    }

    // ── Encoder ───────────────────────────────────────────────
    Encoder::update();
    if (Encoder::hasChanged()) {
        int newLevel = constrain((int)Encoder::getCount(), -7, 7);
        if (newLevel != Encoder::currentVPotLevel) {
            Encoder::currentVPotLevel = newLevel;
            needsVPotRedraw = true;
        }
    }

    // ── Hardware (botones + touch) ────────────────────────────
    updateButtons();

    // ── Display ───────────────────────────────────────────────
    updateDisplay();

    // ── NeoPixels ─────────────────────────────────────────────
    updateAllNeopixels();
}