#pragma once
#include <Arduino.h>

// ============================================================
//  protocol.h  –  Estructuras RS485 compartidas
//  Master (ESP32-S3) ↔ Slave (ESP32-S2)
// ============================================================

#define RS485_START_BYTE  0xAA
#define RS485_RESP_BYTE   0xBB

// --- FLAGS ---
#define FLAG_REC    (1 << 0)
#define FLAG_SOLO   (1 << 1)
#define FLAG_MUTE   (1 << 2)
#define FLAG_SELECT (1 << 3)

// --- CRC8 ---
inline uint8_t rs485_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
        }
    }
    return crc;
}

// --- Master → Slave (14 bytes) ---
struct __attribute__((packed)) MasterPacket {
    uint8_t  header;        // 0xAA
    uint8_t  id;            // 1-17
    char     trackName[7];  // Mackie Scribble Strip (7 chars, sin null)
    uint8_t  flags;         // FLAG_REC | FLAG_SOLO | FLAG_MUTE | FLAG_SELECT
    uint16_t faderTarget;   // Pitch Bend 14-bit: 0-16383
    uint8_t  vuLevel;       // 0-127
    uint8_t  crc;
};
static_assert(sizeof(MasterPacket) == 14, "MasterPacket debe ser 14 bytes");

// --- Slave → Master (9 bytes) ---
struct __attribute__((packed)) SlavePacket {
    uint8_t  header;        // 0xBB
    uint8_t  id;            // MY_SLAVE_ID
    uint16_t faderPos;      // ADC 12-bit: 0-4095
    uint8_t  touchState;    // 0=libre 1=tocado
    uint8_t  buttons;       // FLAG_REC | FLAG_SOLO | FLAG_MUTE | FLAG_SELECT
    int8_t   encoderDelta;  // rotación acumulada (-127..+127)
    uint8_t  encoderButton; // push encoder: 0/1
    uint8_t  crc;
};
static_assert(sizeof(SlavePacket) == 9, "SlavePacket debe ser 9 bytes");