// src/hardware/Neopixels/Neopixel.h
#pragma once

#include <Arduino.h>
#include <NeoPixelBus.h>
#include "../Hardware.h"   // ButtonId
#include "../../config.h"

// ─────────────────────────────────────────────
//  Tipo público — usar en SatMenu y cualquier
//  módulo que necesite acceder al strip
// ─────────────────────────────────────────────
using NeoStrip = NeoPixelBus<NeoGrbFeature, NeoEsp32I2s0800KbpsMethod>;

extern NeoStrip neopixels;

// ─────────────────────────────────────────────
//  API pública
// ─────────────────────────────────────────────

// Llama una vez DESPUÉS de initDisplay()
void initNeopixels();

// Fija el color de un pixel (aplica brillo global).
// No llama Show() internamente — usa showNeopixels()
void setNeopixelState(int idx, uint8_t r, uint8_t g, uint8_t b);

// Apaga un pixel individual
void clearNeopixel(int idx);

// Apaga todos los pixels
void clearAllNeopixels();

// Envía buffer al hardware (un solo Show() por ciclo)
void showNeopixels();

// Brillo global 0-255 (escala los valores R/G/B en setNeopixelState)
void setNeopixelGlobalBrightness(uint8_t brightness);
uint8_t getNeopixelBrightness();
// Flag para mantener el azul de espera de handshake
extern bool neoWaitingHandshake;  // true = mantener azul, ignorar updateAllNeopixels

void handleButtonLedState(ButtonId id);
void updateAllNeopixels();

