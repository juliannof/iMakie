#pragma once

#include <Arduino.h>
#include "RS485.h"          // ← mismo nivel, no "../"
#include "../protocol.h"
#include "hardware/fader/FaderADC.h" 
#include "../menu/SatMenu.h"

namespace RS485Handler {

    // Procesa un paquete recibido del maestro S3.
    // Actualiza estado de canal, motor, display flags, etc.
    void onMasterData(const MasterPacket& pkt);

    // Construye el SlavePacket de respuesta con la telemetría local.
    SlavePacket buildResponse(FaderADC& faderADC, SatMenu& satMenu);

    // Evalúa timeout de comunicación y actualiza estado de conexión.
    void checkTimeout(unsigned long lastRxTime);

} // namespace RS485Handler