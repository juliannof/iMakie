#include <Arduino.h>
#include <hardware/uart.h>

#define MY_SLAVE_ID     1
#define FADER_PIN       26
#define MOTOR_IN1       14
#define MOTOR_IN2       15
#define MOTOR_EN        27
#define RS485_TX_PIN    4
#define RS485_RX_PIN    5
#define RS485_EN_PIN    28
#define RS485_BAUD      500000

#define RS485_START_BYTE  0xAA
#define RS485_RESP_BYTE   0xBB

struct __attribute__((packed)) MasterPacket {
    uint8_t  header;
    uint8_t  id;
    char     trackName[7];
    uint8_t  flags;
    uint16_t faderTarget;
    uint8_t  vuLevel;
    uint8_t  connected;
    uint8_t  crc;
};
static_assert(sizeof(MasterPacket) == 15, "MasterPacket debe ser 15 bytes");

struct __attribute__((packed)) SlavePacket {
    uint8_t  header;
    uint8_t  id;
    uint16_t faderPos;
    uint8_t  touchState;
    uint8_t  buttons;
    int8_t   encoderDelta;
    uint8_t  encoderButton;
    uint8_t  crc;
};
static_assert(sizeof(SlavePacket) == 9, "SlavePacket debe ser 9 bytes");

inline uint8_t rs485_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++)
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
    }
    return crc;
}

volatile int   motorTarget = 2048;
volatile float emaValue    = 0;

enum class RxState : uint8_t { WAIT_HEADER, RECEIVE_PACKET };
RxState      _rxState    = RxState::WAIT_HEADER;
uint8_t      _rxBuf[sizeof(MasterPacket)];
uint8_t      _rxBytesGot = 0;
MasterPacket _rxPacket;
bool         _newData    = false;

void rs485Update() {
    while (Serial2.available()) {
        uint8_t byte = Serial2.read();
        switch (_rxState) {
            case RxState::WAIT_HEADER:
                if (byte == RS485_START_BYTE) {
                    _rxBuf[0]   = byte;
                    _rxBytesGot = 1;
                    _rxState    = RxState::RECEIVE_PACKET;
                }
                break;
            case RxState::RECEIVE_PACKET:
                _rxBuf[_rxBytesGot++] = byte;
                if (_rxBytesGot >= sizeof(MasterPacket)) {
                    uint8_t crc = rs485_crc8(_rxBuf, sizeof(MasterPacket) - 1);
                    if (crc == _rxBuf[sizeof(MasterPacket) - 1] &&
                        _rxBuf[1] == MY_SLAVE_ID) {
                        memcpy(&_rxPacket, _rxBuf, sizeof(MasterPacket));
                        _newData = true;
                    }
                    _rxState    = RxState::WAIT_HEADER;
                    _rxBytesGot = 0;
                }
                break;
        }
    }
}

void rs485SendResponse() {
    SlavePacket tx = {};
    tx.header   = RS485_RESP_BYTE;
    tx.id       = MY_SLAVE_ID;
    tx.faderPos = (uint16_t)emaValue;
    tx.crc      = rs485_crc8((const uint8_t*)&tx, sizeof(SlavePacket) - 1);

    digitalWrite(RS485_EN_PIN, HIGH);
    delayMicroseconds(10);
    Serial2.write((const uint8_t*)&tx, sizeof(SlavePacket));
    Serial2.flush();
    delayMicroseconds(10);
    digitalWrite(RS485_EN_PIN, LOW);

    // Limpia eco del FIFO
    delayMicroseconds(200);
    while (Serial2.available()) Serial2.read();

    // Limpia framing errors del PL011
    hw_clear_bits(&uart_get_hw(uart1)->rsr, UART_UARTRSR_BITS);
}

const int   PWM_MIN    = 50;
const int   PWM_MAX    = 220;
const int   DEAD_ZONE  = 80;
const int   MIN_ERROR  = 100;
const int   BRAKE_DIST = 80;
const int   SLEW_MAX   = 12;
const float GAMMA      = 0.6f;
int         currentPWM = 0;

void motorForward(int pwm) { digitalWrite(MOTOR_IN2, LOW);  analogWrite(MOTOR_IN1, pwm); }
void motorReverse(int pwm) { digitalWrite(MOTOR_IN1, LOW);  analogWrite(MOTOR_IN2, pwm); }
void motorStop()           { digitalWrite(MOTOR_IN1, LOW);  digitalWrite(MOTOR_IN2, LOW); currentPWM = 0; }

int calcPWM(int error) {
    float errNorm = constrain(abs(error) / 4096.0f, 0.0f, 1.0f);
    return (int)(PWM_MIN + pow(errNorm, GAMMA) * (PWM_MAX - PWM_MIN));
}

void updateMotor(int error) {
    if (abs(error) <= DEAD_ZONE) { motorStop(); return; }
    if (abs(error) <  MIN_ERROR) { motorStop(); return; }
    int targetPWM = (abs(error) < BRAKE_DIST) ? PWM_MIN : calcPWM(error);
    targetPWM  = constrain(targetPWM, currentPWM - SLEW_MAX, currentPWM + SLEW_MAX);
    currentPWM = targetPWM;
    if (error > 0) motorForward(currentPWM);
    else           motorReverse(currentPWM);
}

void setup() {
    Serial.begin(115200);

    for (int p = 0; p <= 8; p++) {
        if (p != RS485_TX_PIN && p != RS485_RX_PIN)
            pinMode(p, INPUT_PULLDOWN);
    }

    pinMode(RS485_EN_PIN, OUTPUT);
    digitalWrite(RS485_EN_PIN, LOW);
    pinMode(RS485_RX_PIN, INPUT);
    Serial2.setFIFOSize(256);
    Serial2.setTX(RS485_TX_PIN);
    Serial2.setRX(RS485_RX_PIN);
    Serial2.begin(RS485_BAUD, SERIAL_8N1);

    Serial.println("=== RP2040 Core0: RS485 ===");
}

void loop() {
    static uint32_t lastPrint = 0;
    static uint32_t pktCount  = 0;

    rs485Update();

    if (_newData) {
        _newData = false;
        pktCount++;
        rs485SendResponse();
        motorTarget = map(_rxPacket.faderTarget, 0, 16383, 0, 4095);
    }

    if (millis() - lastPrint >= 200) {
        lastPrint = millis();
        Serial.print("PKT:"); Serial.print(pktCount);
        Serial.print(" POS:"); Serial.print((int)emaValue);
        Serial.print(" TGT:"); Serial.print((int)motorTarget);
        Serial.print(" ERR:"); Serial.print((int)motorTarget - (int)emaValue);
        Serial.print(" PWM:"); Serial.println(currentPWM);
    }
}

void setup1() {
    analogReadResolution(12);
    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    pinMode(MOTOR_EN,  OUTPUT);
    digitalWrite(MOTOR_EN, HIGH);
    motorStop();
    emaValue = analogRead(FADER_PIN);
}

void loop1() {
    static uint32_t lastControl = 0;
    uint32_t now = millis();
    if (now - lastControl >= 5) {
        lastControl = now;
        int raw  = analogRead(FADER_PIN);
        emaValue = 0.1f * raw + 0.9f * emaValue;
        updateMotor((int)motorTarget - (int)emaValue);
    }
}