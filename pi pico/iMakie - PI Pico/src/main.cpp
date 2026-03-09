#include <Arduino.h>
#include <hardware/pio.h>
#include <hardware/clocks.h>
#include <hardware/gpio.h>

// ═══════════════════════════════════════════════════
//  CONFIGURACIÓN Y PINES
// ═══════════════════════════════════════════════════
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

// Deadzone con histéresis
#define MOTOR_DEADZONE_STOP   50   // para cuando el motor está andando
#define MOTOR_DEADZONE_START  70   // umbral para arrancar
#define MOTOR_PWM_FREQ        20000 // 20 kHz — inaudible, mejor para DRV8833

// Timeout RX: resetear buffer si no llega byte en este tiempo
#define RS485_RX_TIMEOUT_MS   5

// Estructuras de datos
struct __attribute__((packed)) MasterPacket {
    uint8_t  header; uint8_t  id; char trackName[7]; uint8_t  flags;
    uint16_t faderTarget; uint8_t  vuLevel; uint8_t  connected; uint8_t  crc;
};
struct __attribute__((packed)) SlavePacket {
    uint8_t  header; uint8_t  id; uint16_t faderPos; uint8_t  touchState;
    uint8_t  buttons; int8_t  encoderDelta; uint8_t  encoderButton; uint8_t  crc;
};

// ═══════════════════════════════════════════════════
//  BLOQUE PIO — UART TX puro, sin sideset
//
//  BUG CRÍTICO CORREGIDO: la versión anterior usaba sideset para
//  controlar EN. La instrucción pull (instr 0) tenía side=0, lo que
//  bajaba EN entre cada byte, fragmentando la trama RS485.
//
//  Solución: PIO maneja SOLO los bits UART (sin sideset).
//  EN se controla manualmente desde CPU antes/después de toda la trama.
//
//  clkdiv = sys_clk / (8 × baud) → 1 ciclo PIO = 1/8 de bit
//  Cada bit = 8 ciclos PIO ✅
// ═══════════════════════════════════════════════════
static const uint16_t rs485_tx_instructions[] = {
    0x8060, // 0: pull block           (espera dato en FIFO)
    0xe027, // 1: set x, 7             (contador 8 bits)
    0xe700, // 2: set pins, 0 [7]      (Start Bit: 1+7 = 8 ciclos) ✅
    0x6001, // 3: out pins, 1          (Data bit: 1 ciclo)
    0x0643, // 4: jmp x--, 3 [6]       (loop: 1+6+1 = 8 ciclos) ✅
    0xe701  // 5: set pins, 1 [7]      (Stop Bit: 1+7 = 8 ciclos) ✅
};
static const struct pio_program rs485_tx_program = {
    .instructions = rs485_tx_instructions, .length = 6, .origin = -1
};

// ═══════════════════════════════════════════════════
//  VARIABLES GLOBALES
// ═══════════════════════════════════════════════════
volatile int      motorTarget = 2048;

// Intercambio atómico de float entre cores via uint32_t ✅
// Cortex-M0+: stores de 32 bits alineados son atómicos en hardware
volatile uint32_t emaRaw = 0;

MasterPacket _rxPacket;
bool         _newData = false;
PIO          _pio = pio0;
uint         _sm  = 0;

// ═══════════════════════════════════════════════════
//  HELPERS ATÓMICOS
// ═══════════════════════════════════════════════════
static inline void ema_write(float v) {
    uint32_t tmp;
    memcpy(&tmp, &v, sizeof(tmp));
    emaRaw = tmp;
}

static inline float ema_read() {
    uint32_t raw = emaRaw;
    float v;
    memcpy(&v, &raw, sizeof(v));
    return v;
}

// ═══════════════════════════════════════════════════
//  CRC
// ═══════════════════════════════════════════════════
uint8_t rs485_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++)
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
    }
    return crc;
}

// ═══════════════════════════════════════════════════
//  SETUP PIO
// ═══════════════════════════════════════════════════
void setupRS485PIO() {
    uint offset = pio_add_program(_pio, &rs485_tx_program);
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + 5);

    // Sin sideset: EN lo controla la CPU manualmente ✅
    sm_config_set_out_pins(&c, RS485_TX_PIN, 1);
    sm_config_set_set_pins(&c, RS485_TX_PIN, 1);

    pio_gpio_init(_pio, RS485_TX_PIN);

    // EN como GPIO puro, no PIO ✅
    gpio_init(RS485_EN_PIN);
    gpio_set_dir(RS485_EN_PIN, GPIO_OUT);
    gpio_put(RS485_EN_PIN, 0); // LOW en reposo = receive mode

    // Idle HIGH en TX (RS485 MARK state)
    pio_sm_set_pins_with_mask(_pio, _sm, (1u << RS485_TX_PIN), (1u << RS485_TX_PIN));
    pio_sm_set_consecutive_pindirs(_pio, _sm, RS485_TX_PIN, 1, true);

    float div = (float)clock_get_hz(clk_sys) / (8.0f * RS485_BAUD);
    sm_config_set_clkdiv(&c, div);
    sm_config_set_out_shift(&c, true, false, 32);

    pio_sm_init(_pio, _sm, offset, &c);
    pio_sm_set_enabled(_pio, _sm, true);
}

// ═══════════════════════════════════════════════════
//  RX
// ═══════════════════════════════════════════════════
void rs485Update() {
    static uint8_t  _rxBuf[sizeof(MasterPacket)];
    static uint8_t  _rxCount    = 0;
    static uint32_t _lastByteMs = 0;

    // Timeout: paquete incompleto → resetear ✅
    if (_rxCount > 0 && (millis() - _lastByteMs) > RS485_RX_TIMEOUT_MS) {
        _rxCount = 0;
    }

    while (Serial2.available()) {
        uint8_t b = Serial2.read();
        _lastByteMs = millis();

        if (_rxCount == 0 && b != RS485_START_BYTE) continue;

        _rxBuf[_rxCount++] = b;
        if (_rxCount >= sizeof(MasterPacket)) {
            uint8_t crc = rs485_crc8(_rxBuf, sizeof(MasterPacket) - 1);
            if (crc == _rxBuf[sizeof(MasterPacket) - 1] && _rxBuf[1] == MY_SLAVE_ID) {
                memcpy(&_rxPacket, _rxBuf, sizeof(MasterPacket));
                _newData = true;
            }
            _rxCount = 0;
        }
    }
}

// ═══════════════════════════════════════════════════
//  TX
// ═══════════════════════════════════════════════════
void rs485SendResponse() {
    SlavePacket tx = {};
    tx.header   = RS485_RESP_BYTE;
    tx.id       = MY_SLAVE_ID;
    tx.faderPos = (uint16_t)ema_read();
    tx.crc      = rs485_crc8((const uint8_t*)&tx, sizeof(SlavePacket) - 1);

    // EN HIGH antes de toda la trama — se mantiene durante TODOS los bytes ✅
    gpio_put(RS485_EN_PIN, 1);
    delayMicroseconds(2); // settling del transceptor

    uint8_t* p = (uint8_t*)&tx;
    for (size_t i = 0; i < sizeof(SlavePacket); i++) {
        pio_sm_put_blocking(_pio, _sm, p[i]);
    }

    // Esperar que FIFO y shift register terminen de vaciar.
    // A 500 kbps: 1 byte (10 bits) = 20µs → 25µs de margen ✅
    while (!pio_sm_is_tx_fifo_empty(_pio, _sm)) tight_loop_contents();
    delayMicroseconds(25);

    // Liberar el bus → receive mode
    gpio_put(RS485_EN_PIN, 0);
}

// ═══════════════════════════════════════════════════
//  CORE 0 — Comunicaciones
// ═══════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);

    // Buffer RX ampliado antes de begin() ✅
    Serial2.setFIFOSize(128);
    Serial2.setRX(RS485_RX_PIN); // GP5
    Serial2.setTX(255);          // GP4 pertenece al PIO — UART TX deshabilitado ✅
    Serial2.begin(RS485_BAUD, SERIAL_8N1);

    setupRS485PIO();
    Serial.println("System Ready: PIO RS485 & Dual Core");
}

void loop() {
    rs485Update();
    if (_newData) {
        _newData = false;
        rs485SendResponse();
        // constrain explícito: protección si master envía fuera de rango ✅
        motorTarget = constrain(map(_rxPacket.faderTarget, 0, 16383, 0, 4095), 0, 4095);
    }
}

// ═══════════════════════════════════════════════════
//  CORE 1 — Motor y ADC
// ═══════════════════════════════════════════════════
void setup1() {
    analogReadResolution(12);
    analogWriteFreq(MOTOR_PWM_FREQ); // 20 kHz ✅
    analogWriteRange(255);

    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    pinMode(MOTOR_EN,  OUTPUT);
    digitalWrite(MOTOR_EN, HIGH);

    float init = (float)analogRead(FADER_PIN);
    ema_write(init);
}

void loop1() {
    static uint32_t lastControl  = 0;
    static float    emaLocal     = 0.0f;
    static bool     motorRunning = false; // estado para histéresis ✅

    if (millis() - lastControl >= 5) {
        lastControl = millis();

        int raw  = analogRead(FADER_PIN);
        emaLocal = 0.15f * raw + 0.85f * emaLocal;
        ema_write(emaLocal);

        int error  = (int)motorTarget - (int)emaLocal;
        int absErr = abs(error);

        // Histéresis: deadzone diferente para parar vs arrancar ✅
        // Evita chatter cuando el error oscila alrededor del umbral
        int deadzone = motorRunning ? MOTOR_DEADZONE_STOP : MOTOR_DEADZONE_START;

        if (absErr < deadzone) {
            digitalWrite(MOTOR_IN1, LOW);
            digitalWrite(MOTOR_IN2, LOW);
            motorRunning = false;
        } else {
            motorRunning = true;
            int pwm = constrain(map(absErr, 0, 1000, 60, 230), 60, 230);
            if (error > 0) {
                digitalWrite(MOTOR_IN2, LOW);
                analogWrite(MOTOR_IN1, pwm);
            } else {
                digitalWrite(MOTOR_IN1, LOW);
                analogWrite(MOTOR_IN2, pwm);
            }
        }
    }
}