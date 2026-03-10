// ============================================================
//  main.cpp  —  iMakie PTxx Track S2
//  REC long-press 1 s → SAT (gestionado en ButtonManager)
//  Sin Serial. Todo el feedback en display.
// ============================================================
#include <Arduino.h>
#include "config.h"
#include "display/Display.h"
#include "display/LovyanGFX_config.h"
#include "hardware/encoder/Encoder.h"
#include "hardware/Hardware.h"
#include "RS485/RS485.h"
#include "protocol.h"
#include "button/ButtonManager.h"
#include "menu/SatMenu.h"
#include <driver/touch_sensor.h>   // touch_pad_init, touch_pad_read_raw_data
#include "hardware/Motor/Motor.h"
#include "fader/FaderADC.h"

FaderADC faderADC;


// ─── Display objects ──────────────────────────────────────────
LGFX        tft;
LGFX_Sprite header(&tft), mainArea(&tft), vuSprite(&tft), vPotSprite(&tft);


// ─── Externs de Display.cpp ───────────────────────────────────
extern void initDisplay();
extern void updateDisplay();
extern void setVPotLevel(int8_t level);
extern int  currentVPotLevel;
extern bool needsVPotRedraw;

// ─── Variables de estado de canal ─────────────────────────────
String assignmentString = "CH-01 ";
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

// ─── RS485 response packet ────────────────────────────────────
static SlavePacket _resp = {};

// ─── Flag: suspendido por SAT ─────────────────────────────────
static bool _suspended = false;

// ─── SAT menu ─────────────────────────────────────────────────
static SatMenu* satMenu = nullptr;

// ─── Forward ──────────────────────────────────────────────────
extern void handleButtonLedState(ButtonId id);

// ─── Motor helpers ────────────────────────────────────────────
static void motorStop() {
    digitalWrite(MOTOR_EN,  LOW);
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
}
static void motorDrive(int pwm) {
    if (pwm == 0) { motorStop(); return; }
    digitalWrite(MOTOR_EN, HIGH);
    if (pwm > 0) {
        analogWrite(MOTOR_IN1, constrain( pwm, 0, 255));
        digitalWrite(MOTOR_IN2, LOW);
    } else {
        digitalWrite(MOTOR_IN1, LOW);
        analogWrite(MOTOR_IN2, constrain(-pwm, 0, 255));
    }
}

// ─── Stubs ADC / Touch ────────────────────────────────────────
static uint16_t readFaderADC() { return Motor::getRawADC(); }
static uint8_t  readFaderTouch() {
    // touchRead() es la API Arduino del ESP32-S2 para capacitive touch.
    // Valor alto = sin contacto, valor bajo = tocado.
    uint32_t v = touchRead(FADER_TOUCH_PIN);   // GPIO1 / Touch1
    static uint32_t base = 0;
    if (base == 0 && v > 100) base = v;
    return (base > 0 && v < base * 0.80f) ? 1 : 0;
}

// =============================================================
//  onMasterData  —  RS485
// =============================================================
static void onMasterData(const MasterPacket& pkt) {
    ConnectionState newState = pkt.connected ?
        ConnectionState::CONNECTED : ConnectionState::DISCONNECTED;
    if (newState != logicConnectionState) {
        logicConnectionState = newState;
        needsTOTALRedraw = true;
    }
    if (logicConnectionState != ConnectionState::CONNECTED) return;

    char nameBuf[8] = {};
    memcpy(nameBuf, pkt.trackName, 7); nameBuf[7] = '\0';
    if (trackName != nameBuf) { trackName = String(nameBuf); needsHeaderRedraw = true; }

    bool nr = (pkt.flags & FLAG_REC)    != 0;
    bool ns = (pkt.flags & FLAG_SOLO)   != 0;
    bool nm = (pkt.flags & FLAG_MUTE)   != 0;
    bool nq = (pkt.flags & FLAG_SELECT) != 0;
    if (recStates    != nr) { recStates    = nr; handleButtonLedState(ButtonId::REC);    needsMainAreaRedraw = true; }
    if (soloStates   != ns) { soloStates   = ns; handleButtonLedState(ButtonId::SOLO);   needsMainAreaRedraw = true; }
    if (muteStates   != nm) { muteStates   = nm; handleButtonLedState(ButtonId::MUTE);   needsMainAreaRedraw = true; }
    if (selectStates != nq) { selectStates = nq; handleButtonLedState(ButtonId::SELECT); needsHeaderRedraw   = true; }

    float newVu = pkt.vuLevel / 127.0f;
    if (fabsf(vuLevels - newVu) > 0.01f) {
        vuLevels = newVu;
        if (vuLevels > vuPeakLevels) { vuPeakLevels = vuLevels; vuPeakLastUpdateTime = millis(); }
        vuLastUpdateTime = millis();
        needsVUMetersRedraw = true;
    }

    float newFader = pkt.faderTarget / 16383.0f;
    if (!_suspended && fabsf(faderPositions - newFader) > 0.001f) {
        faderPositions = newFader;
        Motor::setTarget(pkt.faderTarget);  // ← reemplaza el TODO
    }
}

void setup() {
    Serial.begin(115200);
    
    // Motor pins off — PRIMERO
    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    pinMode(MOTOR_EN,  OUTPUT);
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    digitalWrite(MOTOR_EN,  LOW);

    dacWrite(FADER_VCC_PIN, 77);
    delay(30);
    faderADC.begin();

    initDisplay();        // LovyanGFX primero
    initHardware();
    setVPotLevel(VPOT_DEFAULT_LEVEL);
    Encoder::begin();
    
    satMenu = new SatMenu(&tft);
    // ... callbacks ...
    
    uint8_t slaveId = satMenu->getConfig().trackId;
    rs485.begin(slaveId);
    if (slaveId == 0) satMenu->open();
    
    ButtonManager::begin(&tft, satMenu);

    Motor::begin();   // ← ÚLTIMO, todo ya inicializado
}

// =============================================================
//  LOOP
// =============================================================
void loop() {
    // SAT abierto: solo menú
    if (satMenu && satMenu->isOpen()) {
        satMenu->update();
        return;
    }

    // Actualizar barra de progreso del long-press REC
    ButtonManager::update();
    if (satMenu && satMenu->isOpen()) return;

    // RS485
    if (!_suspended) {
        rs485.update();
        static unsigned long lastRxTime = 0;

        if (rs485.hasNewData()) {
            lastRxTime = millis();
            if (logicConnectionState != ConnectionState::CONNECTED) {
                logicConnectionState = ConnectionState::CONNECTED;
                needsTOTALRedraw = true;
            }
            onMasterData(rs485.getData());

            _resp.faderPos      = readFaderADC();
            _resp.touchState    = readFaderTouch();
            _resp.buttons       = ButtonManager::getButtonFlags();
            _resp.encoderDelta  = (int8_t)constrain(Encoder::getCount(), -127, 127);
            _resp.encoderButton = 0;
            rs485.sendResponse(_resp);

            ButtonManager::clearButtonFlags();
            Encoder::reset();
        }

        if (millis() - lastRxTime > 500) {
            if (vuLevels > 0.0f) {
                vuLevels = 0.0f; vuPeakLevels = 0.0f;
                needsVUMetersRedraw = true;
            }
            if (logicConnectionState != ConnectionState::DISCONNECTED) {
                logicConnectionState = ConnectionState::DISCONNECTED;
                needsTOTALRedraw = true;
            }
        }
    }
    // ── Fader + Motor — orden crítico ──
    faderADC.update();
    Motor::setADC(faderADC.getFaderPos());  // ← debe ir ANTES de update()

    if (readFaderTouch()) {
        Motor::stop();
    } else {
        Motor::update();
    }


    // Encoder
    Encoder::update();
    if (Encoder::hasChanged()) {
        int newLevel = constrain((int)Encoder::getCount(), -7, 7);
        if (newLevel != Encoder::currentVPotLevel) {
            Encoder::currentVPotLevel = newLevel;
            needsVPotRedraw = true;
        }
    }

    // Botones (Button2 tick — necesario para que Button2 detecte eventos)
    updateButtons();

    // Display
    updateDisplay();


    updateAllNeopixels();
    

}