// ============================================================
//  main.cpp  —  iMakie PTxx Track S2
//  Solo orquesta. Cero lógica de negocio aquí.
// ============================================================
#include <Arduino.h>
#include "config.h"
#include "display/Display.h"
#include "display/LovyanGFX_config.h"
#include "OTA/OtaManager.h"
#include "hardware/fader/FaderADC.h"
#include "hardware/fader/FaderTouch.h"
#include "hardware/encoder/Encoder.h"
#include "hardware/Hardware.h"
#include "hardware/Neopixels/Neopixel.h"
#include "hardware/Motor/Motor.h"
#include "RS485/RS485.h"
#include "RS485/RS485Handler.h"       // ← onMasterData vive aquí
#include "protocol.h"
#include "hardware/button/ButtonManager.h"
#include "../SAT/SatMenu.h"
#include <driver/dac_oneshot.h>



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
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    delay(1500);

    otaManager.begin();                          // ← AÑADIR
    log_i("OtaManager OK");

    log_i("=== iMakie PTxx BOOT ===");

    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    pinMode(MOTOR_EN,  OUTPUT);
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    digitalWrite(MOTOR_EN,  LOW);
    log_i("Motor pins init OK");

    dac_oneshot_handle_t _dacHandle;
    dac_oneshot_config_t _dacCfg = { .chan_id = DAC_CHAN_0 };
    dac_oneshot_new_channel(&_dacCfg, &_dacHandle);
    dac_oneshot_output_voltage(_dacHandle, 77);
    delay(30);
    faderADC.begin();
    log_i("Fader ADC OK");

    initDisplay();
    log_i("Display OK");

    initNeopixels();
    log_i("NeoPixels OK");

    initHardware();
    log_i("Hardware OK");

    FaderTouch::init();
    FaderTouch::onTouch([]()   { digitalWrite(LED_BUILTIN_PIN, HIGH); });
    FaderTouch::onRelease([]() { digitalWrite(LED_BUILTIN_PIN, LOW);  });
    log_i("FaderTouch OK");

    setVPotLevel(VPOT_DEFAULT_LEVEL);
    Encoder::begin();
    log_i("Encoder OK");

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
    satMenu->onWiFiConfig([]() { otaManager.launchPortal();    });  // ← AÑADIR
    satMenu->onWiFiOta   ([]() { otaManager.enableForUpload(); }); // ← AÑADIR
    log_i("SatMenu OK");

    uint8_t slaveId = satMenu->getConfig().trackId;
    log_i("Track ID: %d", slaveId);
    rs485.begin(slaveId);
    if (slaveId == 0) {
        log_w("Track ID=0 — forzando SAT menu");
        satMenu->open();
    }

    ButtonManager::begin(&tft, satMenu);
    log_i("ButtonManager OK");

    Motor::begin();
    log_i("Motor OK");

    log_i("=== BOOT completo | heap libre: %d bytes ===", ESP.getFreeHeap());
}

// =============================================================
//  loop
// =============================================================
void loop() {
    otaManager.tick();

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

    if (FaderTouch::isTouched()) {   // ← sustituye isFaderTouched
        Motor::stop();
    } else {
        Motor::update();
    }

    // ── Encoder ───────────────────────────────────────────────
    Encoder::update();
    if (Encoder::hasChanged()) {
        int newLevel = constrain((int)(Encoder::getCount() / 4), -7, 7);
        if (newLevel != Encoder::currentVPotLevel) {
            Encoder::currentVPotLevel = newLevel;
            needsVPotRedraw = true;
        }
    }

    // ── Hardware (botones + touch) ────────────────────────────
    updateButtons();
    FaderTouch::update();    // ← faltaba

    // ── Display ───────────────────────────────────────────────
    updateDisplay();

    // ── NeoPixels ─────────────────────────────────────────────
    updateAllNeopixels();
}