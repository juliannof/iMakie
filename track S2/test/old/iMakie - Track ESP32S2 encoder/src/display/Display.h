// src/display/Display.h
#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "../config.h" // <-- Ya confirmamos que necesitábamos esta inclusión.

// --- Declaración de banderas globales de redibujo ---
extern bool needsTOTALRedraw;      
extern bool needsMainAreaRedraw;   
extern bool needsHeaderRedraw;     
extern bool needsVUMetersRedraw;   

extern volatile ConnectionState logicConnectionState; // Ahora 'ConnectionState' será reconocida.

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
 */
void drawMainArea(); 

/**
 * @brief Dibuja todos los vúmetros en el área dedicada.
 */
void drawVUMeters(); 

/**
 * @brief Dibuja un único vúmetro con nivel instantáneo y de pico.
 */
void drawMeter(TFT_eSprite &sprite, uint16_t x, uint16_t y, uint16_t w, uint16_t h, float level, float peakLevel, bool isClipping);

