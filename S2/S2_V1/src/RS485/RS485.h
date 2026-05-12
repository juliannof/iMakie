#pragma once
#include <Arduino.h>
#include "../config.h"
#include "../protocol.h"

class RS485Slave {
public:
    void begin(uint8_t myId);
    void update();                          // llamar en loop()

    bool hasNewData() const { return _newData; }
    const MasterPacket& getData() {         // consume el flag
        _newData = false;
        return _rxPacket;
    }

    void sendResponse(const SlavePacket& pkt);
    void printStats()  const;

    // llamado desde onReceive — no usar directamente
    void _onReceiveISR();

private:
    void _processBuffer();

    uint8_t  _myId      = 1;

    // Buffer circular
    static constexpr uint16_t CB_SIZE = 256;
    volatile uint8_t  _cb[CB_SIZE];
    volatile uint16_t _cbHead = 0;
    volatile uint16_t _cbTail = 0;

    // Máquina de estados RX
    enum class RxState : uint8_t { WAIT_HEADER, RECEIVE_PACKET };
    RxState _rxState     = RxState::WAIT_HEADER;
    uint8_t _rxBuf[sizeof(MasterPacket)];
    uint8_t _rxBytesGot  = 0;

    MasterPacket _rxPacket;
    bool         _newData = false;

    // Estadísticas
    uint32_t _rxCount    = 0;
    uint32_t _crcErrors  = 0;
    uint32_t _wrongId    = 0;
    uint32_t _overflow   = 0;
};

extern RS485Slave rs485;