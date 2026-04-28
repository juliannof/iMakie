// ============================================================
//  main.cpp  —  iMakie PTxx Track S2
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
#include "RS485/RS485Handler.h"
#include "protocol.h"
#include "hardware/button/ButtonManager.h"
#include "SAT/SatMenu.h"
#include "display/SpriteUtils.h"
#include "nvs/NVSValidator.h"
#include <driver/dac_oneshot.h>


// ─── Objetos globales ─────────────────────────────────────────
LGFX        tft;
LGFX_Sprite header(&tft), mainArea(&tft), vuSprite(&tft), vPotSprite(&tft);
FaderADC    faderADC;

// ─── Estado de canal ──────────────────────────────────────────
String trackName        = "Track  ";
bool  recStates    = false;
bool  soloStates   = false;
bool  muteStates   = false;
bool  selectStates = false;
bool  vuClipState  = false;
float vuPeakLevels    = 0.0f;
float faderPositions  = 0.0f;
float vuLevels        = 0.0f;
unsigned long vuLastUpdateTime     = 0;
unsigned long vuPeakLastUpdateTime = 0;

static bool     _suspended = false;
static SatMenu* satMenu    = nullptr;

// ─────────────────────────────────────────────────────────────
//  Callbacks SAT
// ─────────────────────────────────────────────────────────────
static void _satMotorOff()  { Motor::stop();   _suspended = true;  }
static void _satMotorOn()   { Motor::begin();  _suspended = false; }
static void _satBrightness(uint8_t b) { setScreenBrightness(b); }
static void _satRS485Off()  { _suspended = true;  }
static void _satRS485On()   { _suspended = false; needsTOTALRedraw = true; }
static void _satReboot()    { ESP.restart(); }
static void _satMotorDrive(int pwm) { Motor::driveRaw(pwm); }
static void _satConfigSaved(const SatConfig& cfg) { rs485.begin(cfg.trackId); }
static void _satWiFiOta()   { otaManager.enableForUpload(); }
static void _satSerialOta() { otaManager.enableSerialMode(); }
static void _satLedsOff() {
    clearAllNeopixels();
    showNeopixels();


}
static void _satSuspendSprites() {
    header.deleteSprite();
    mainArea.deleteSprite();
    vuSprite.deleteSprite();
    vPotSprite.deleteSprite();
    log_i("Sprites suspendidos | PSRAM libre: %d", ESP.getFreePsram());
}
static void _satRestoreSprites() {
    mainArea.setColorDepth(16);
    mainArea.setPsram(true);
    mainArea.createSprite(MAINAREA_WIDTH, MAINAREA_HEIGHT);

    header.setColorDepth(16);
    header.setPsram(true);
    header.createSprite(TFT_WIDTH, HEADER_HEIGHT);

    vuSprite.setColorDepth(16);
    vuSprite.setPsram(true);
    vuSprite.createSprite(TFT_WIDTH - MAINAREA_WIDTH, MAINAREA_HEIGHT);

    vPotSprite.setColorDepth(16);
    vPotSprite.setPsram(true);
    vPotSprite.createSprite(TFT_WIDTH, VPOT_HEIGHT);

    _logSpriteAlloc("header",    header);
    _logSpriteAlloc("mainArea",  mainArea);
    _logSpriteAlloc("vuSprite",  vuSprite);
    _logSpriteAlloc("vPotSprite",vPotSprite);
    needsTOTALRedraw = true;
}
static void _otaStatus(const char* msg) {
    if (satMenu && satMenu->isOpen())
        satMenu->showStatus(msg);
}
static void _satLedsTest(int idx, uint8_t r, uint8_t g, uint8_t b) {
    log_i("[SAT-LED] idx=%d rgb=(%d,%d,%d)", idx, r, g, b);
    setNeopixelState(idx, r, g, b);
    showNeopixels();
}

// =============================================================
//  setup
// =============================================================
void setup() {
    Motor::init();
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    delay(500);          // 2 segundos visible

    
    initNeopixels();
    log_i("NeoPixels OK");
    
    initDisplay();
    log_i("Display OK");

    // Validar NVS antes de cualquier cosa
    if (NVSValidator::validate() == NVSStatus::CORRUPTED) {
        NVSValidator::reset();  // Repara y reinicia
        return;  // Nunca llegará aquí
    }

    drawSplashScreen();
    
    otaManager.begin();
    
    dac_oneshot_handle_t _dacHandle;
    dac_oneshot_config_t _dacCfg = { .chan_id = DAC_CHAN_0 };
    dac_oneshot_new_channel(&_dacCfg, &_dacHandle);
    dac_oneshot_output_voltage(_dacHandle, 50);
    delay(100);
    faderADC.begin();
    log_i("Fader iniciado.");
    


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
    satMenu->onMotorOff      (_satMotorOff);
    satMenu->onMotorOn       (_satMotorOn);
    satMenu->onMotorDrive    (_satMotorDrive);
    satMenu->onBrightness    (_satBrightness);
    satMenu->onRS485Off      (_satRS485Off);
    satMenu->onRS485On       (_satRS485On);
    satMenu->onReboot        (_satReboot);
    satMenu->onConfigSaved   (_satConfigSaved);
    satMenu->onWiFiOta       (_satWiFiOta);
    satMenu->onSerialOta     (_satSerialOta);
    satMenu->onLedsTest      (_satLedsTest);
    satMenu->onLedsOff       (_satLedsOff);
    satMenu->onSuspendSprites(_satSuspendSprites);
    satMenu->onRestoreSprites(_satRestoreSprites);

    otaManager.onStatus(_otaStatus);
    log_i("SatMenu OK");

    ButtonManager::begin(&tft, satMenu);
    log_i("ButtonManager OK");

    Motor::begin();
    log_i("Motor OK");

    if (psramFound()) {
        log_i("PSRAM: %u KB total, %u KB libre",
            ESP.getPsramSize() / 1024, ESP.getFreePsram() / 1024);
    } else {
        log_e("ERROR: PSRAM no detectada");
    }

    uint8_t slaveId = satMenu->getConfig().trackId;  // ← mover aquí arriba
    Motor::off();   // ← asegurar motor apagado antes de arrancar RS485

    log_i("Track ID: %d", slaveId);
    rs485.begin(slaveId);

    setScreenBrightness(255);
    log_i("=== BOOT completo | heap libre: %d bytes ===", ESP.getFreeHeap());
}

// =============================================================
//  loop
// =============================================================
void loop() {
    otaManager.tick();

    if (otaManager.isOtaActive()) return;

    if (satMenu && satMenu->isOpen()) {
        satMenu->update();
        return;
    }

    ButtonManager::update();
    if (satMenu && satMenu->isOpen()) return;

    if (!_suspended) {
        rs485.update();
        static unsigned long lastRxTime = millis();

        if (rs485.hasNewData()) {
            lastRxTime = millis();
            RS485Handler::onMasterData(rs485.getData());

            SlavePacket resp = RS485Handler::buildResponse(faderADC, *satMenu);
            rs485.sendResponse(resp);

            ButtonManager::clearButtonFlags();
            ButtonManager::clearEncoderButton();
        }

        RS485Handler::checkTimeout(lastRxTime);
    }

    faderADC.update();
    FaderTouch::update();

    Motor::setADC(faderADC.getFaderPos());

    if (FaderTouch::isTouched()) {
        Motor::stop();
    } else {
        Motor::update();
    }

    // Solo procesar encoder para VPot si SAT no lo está consumiendo
    if (!satMenu->isEncoderConsumed()) {
        Encoder::update();
        if (Encoder::hasChanged()) {
            int newLevel = constrain((int)(Encoder::getCount() / 4), -7, 7);
            if (newLevel != Encoder::currentVPotLevel) {
                Encoder::currentVPotLevel = newLevel;
                needsVPotRedraw = true;
            }
        }
    }

    Encoder::reset();  // Resetear DESPUÉS de procesar VPot

    updateButtons();
    updateDisplay();
    updateAllNeopixels();
}