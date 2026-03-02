// ============================================================
//  RS485.cpp  –  Slave ESP32-S2
//  onReceive (IRAM_ATTR) → buffer circular → parseo en loop()
// ============================================================
#include "RS485.h"
#include "../config.h"

RS485Slave rs485;

void RS485Slave::begin(uint8_t myId) {
    _myId = myId;

    pinMode(RS485_ENABLE_PIN, OUTPUT);
    digitalWrite(RS485_ENABLE_PIN, LOW);

    Serial1.setRxBufferSize(512);
    Serial1.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
    Serial1.onReceive([](){ rs485._onReceiveISR(); });

    Serial.printf("[RS485] Slave ID:%d RX:%d TX:%d EN:%d BAUD:%d\n",
                     _myId, RS485_RX_PIN, RS485_TX_PIN, RS485_ENABLE_PIN, RS485_BAUD);
    Serial.printf("[RS485] MasterPacket:%u bytes SlavePacket:%u bytes\n",
                     sizeof(MasterPacket), sizeof(SlavePacket));
}

void RS485Slave::update() {
    _processBuffer();
}

void RS485Slave::sendResponse(const SlavePacket& pkt) {
    SlavePacket tx  = pkt;
    tx.header       = RS485_RESP_BYTE;
    tx.id           = _myId;
    tx.crc          = rs485_crc8((const uint8_t*)&tx, sizeof(SlavePacket) - 1);

    digitalWrite(RS485_ENABLE_PIN, HIGH);
    delayMicroseconds(10);
    Serial1.write((const uint8_t*)&tx, sizeof(SlavePacket));
    Serial1.flush();
    digitalWrite(RS485_ENABLE_PIN, LOW);

    _rxCount++;  // reutilizamos como TX count — renombrar si hace falta
}

void IRAM_ATTR RS485Slave::_onReceiveISR() {
    while (Serial1.available()) {
        uint16_t next = (_cbHead + 1) % CB_SIZE;
        if (next != _cbTail) {
            _cb[_cbHead] = Serial1.read();
            _cbHead = next;
        } else {
            Serial1.read();  // descartar: overflow
            _overflow++;
        }
    }
}

void RS485Slave::_processBuffer() {
    noInterrupts();
    uint16_t head = _cbHead;
    interrupts();

    while (_cbTail != head) {
        uint8_t byte = _cb[_cbTail];
        _cbTail = (_cbTail + 1) % CB_SIZE;

        switch (_rxState) {
            case RxState::WAIT_HEADER:
                if (byte == RS485_START_BYTE) {
                    _rxBuf[0]    = byte;
                    _rxBytesGot  = 1;
                    _rxState     = RxState::RECEIVE_PACKET;
                }
                break;

            case RxState::RECEIVE_PACKET:
                _rxBuf[_rxBytesGot++] = byte;
                if (_rxBytesGot >= sizeof(MasterPacket)) {
                    // Verificar CRC
                    uint8_t crc = rs485_crc8(_rxBuf, sizeof(MasterPacket) - 1);
                    if (crc != _rxBuf[sizeof(MasterPacket) - 1]) {
                        _crcErrors++;
                        Serial.printf("[RS485] CRC error calc=0x%02X recv=0x%02X\n",
                                         crc, _rxBuf[sizeof(MasterPacket) - 1]);
                    } else if (_rxBuf[1] != _myId) {
                        _wrongId++;
                    } else {
                        memcpy(&_rxPacket, _rxBuf, sizeof(MasterPacket));
                        _newData = true;
                        _rxCount++;
                    }
                    _rxState    = RxState::WAIT_HEADER;
                    _rxBytesGot = 0;
                }
                break;
        }
    }
}

void RS485Slave::printStats() const {
    Serial.printf("[RS485] RX:%u CRC_ERR:%u WRONG_ID:%u OVERFLOW:%u\n",
                     _rxCount, _crcErrors, _wrongId, _overflow);
}