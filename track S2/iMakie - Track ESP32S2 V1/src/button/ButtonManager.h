#pragma once
// ============================================================
//  ButtonManager.h  —  iMakie PTxx Track S2
//
//  Gestión centralizada de eventos de botones.
//  Incluye detección de long-press de REC (1 s) → SAT.
//  El long-press usa el mismo Button2 que el short-press;
//  no hay doble lectura del pin.
//
//  Llamar en setup():  ButtonManager::begin(&tft, satMenu)
//  Llamar en loop():   ButtonManager::update()   ← barra de progreso
// ============================================================
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "../hardware/Hardware.h"
#include "../hardware/encoder/Encoder.h"

class SatMenu;

namespace ButtonManager {

    // setup(): registra callback en Hardware + configura long-press
    void begin(LovyanGFX* tft, SatMenu* sat);

    // loop(): actualiza barra de progreso mientras REC está mantenido
    void update();

    // Actualizar referencia al SAT si se crea después del begin
    void setSatMenu(SatMenu* sat);

    // Byte de flags RS485 (se actualiza en cada evento de botón)
    uint8_t getButtonFlags();
    void    clearButtonFlags();
    uint8_t getEncoderButton();
    void    clearEncoderButton();

} // namespace ButtonManager