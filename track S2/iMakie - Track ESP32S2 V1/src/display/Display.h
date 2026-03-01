// src/display/Display.h
#pragma once
#include <Arduino.h>
#include "LovyanGFX_config.h"   // ← reemplaza <TFT_eSPI.h>
#include "../config.h"

extern bool needsTOTALRedraw;
extern bool needsMainAreaRedraw;
extern bool needsHeaderRedraw;
extern bool needsVUMetersRedraw;
extern bool needsVPotRedraw;

extern volatile ConnectionState logicConnectionState;

void initDisplay();
void updateDisplay();
void setScreenBrightness(uint8_t brightness);
void drawOfflineScreen();
void drawInitializingScreen();
void drawHeaderSprite();
void drawMainArea();
void drawVUMeters();
void handleVUMeterDecay();
void setVPotLevel(int8_t level);

void drawButton(LGFX_Sprite &sprite,                  // ← LGFX_Sprite
                uint16_t x, uint16_t y,
                uint16_t w, uint16_t h,
                const char* label,
                bool active,
                uint16_t activeColor);

void drawMeter(LGFX_Sprite &sprite,                   // ← LGFX_Sprite
               uint16_t x, uint16_t y,
               uint16_t w, uint16_t h,
               float level, float peakLevel,
               bool isClipping);