#pragma once

#include <Arduino.h>
#include "RS485.h"
#include "../protocol.h"
#include "../hardware/fader/FaderADC.h"   // ← era "hardware/..." sin ../
#include "../SAT/SatMenu.h"

namespace RS485Handler {

    void onMasterData(const MasterPacket& pkt);
    SlavePacket buildResponse(FaderADC& faderADC, SatMenu& satMenu);
    void checkTimeout(unsigned long lastRxTime);

} // namespace RS485Handler