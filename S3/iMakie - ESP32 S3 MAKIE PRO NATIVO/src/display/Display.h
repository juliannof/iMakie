// src/display/Display.h
#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>        // ← TFT_eSPI.h → LovyanGFX.hpp
#include "../config.h"

// --- Declaración de banderas globales de redibujo ---
extern bool needsTOTALRedraw;
extern bool needsMainAreaRedraw;
extern bool needsHeaderRedraw;
extern bool needsVUMetersRedraw;

extern volatile ConnectionState logicConnectionState;

/**
 * @brief Inicializa la pantalla TFT y los sprites.
 */
void initDisplay();

/**
 * @brief Función principal que orquesta el redibujado de la pantalla.
 */
void updateDisplay();

/**
 * @brief Establece el brillo de la pantalla TFT.
 * @param brightness El nuevo nivel de brillo (0 = apagado, 255 = máximo).
 */
void setScreenBrightness(uint8_t brightness);

/**
 * @brief Dibuja la pantalla de estado "desconectado" o "esperando sesión".
 */
void drawOfflineScreen();

/**
 * @brief Dibuja la pantalla de estado "MIDI Handshake completo, esperando datos DAW".
 */
void drawInitializingScreen();

/**
 * @brief Dibuja la cabecera de la pantalla (tiempo, tempo, asignación).
 */
void drawHeaderSprite();

/**
 * @brief Dibuja el área principal con botones y nombres de pista.
 *        Usa tabla de despacho interna — no requiere parámetros.
 */
void drawMainArea();

/**
 * @brief Navega a la siguiente página y redibuja el área principal.
 */
void nextPage();

/**
 * @brief Dibuja un botón con estado activo/inactivo en un sprite dado.
 */
void drawButton(LGFX_Sprite &sprite,        // ← TFT_eSprite → LGFX_Sprite
                int x, int y, int w, int h,
                const char* label,
                bool active,
                uint16_t activeColor,
                uint16_t textColor      = TFT_WHITE,
                uint16_t inactiveColor  = 0x2104);

/**
 * @brief Dibuja todos los vúmetros en el área dedicada.
 */
void drawVUMeters();

/**
 * @brief Dibuja un único vúmetro con nivel instantáneo y de pico.
 */
void drawMeter(LGFX_Sprite &sprite,         // ← TFT_eSprite → LGFX_Sprite
               uint16_t x, uint16_t y, uint16_t w, uint16_t h,
               float level, float peakLevel, bool isClipping);

/**
 * @brief Gestiona la lógica de sincronización del display de tiempo.
 */
//void handleDisplaySynchronization();