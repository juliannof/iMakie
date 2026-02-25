// src/display/Display.h
#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
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
 *
 * @param sprite       Sprite destino donde se dibuja.
 * @param x            Posición X (esquina superior izquierda).
 * @param y            Posición Y (esquina superior izquierda).
 * @param w            Anchura en píxeles.
 * @param h            Altura en píxeles.
 * @param label        Texto a mostrar en el botón.
 * @param active       true = botón encendido, false = apagado.
 * @param activeColor  Color RGB565 cuando el botón está activo.
 * @param textColor    Color RGB565 del texto.
 * @param inactiveColor Color RGB565 cuando el botón está inactivo (por defecto gris oscuro).
 */
void drawButton(TFT_eSprite &sprite,
                int x, int y, int w, int h,
                const char* label,
                bool active,
                uint16_t activeColor,
                uint16_t textColor     = TFT_WHITE,
                uint16_t inactiveColor = 0x2104);

/**
 * @brief Dibuja todos los vúmetros en el área dedicada.
 */
void drawVUMeters();

/**
 * @brief Dibuja un único vúmetro con nivel instantáneo y de pico.
 */
void drawMeter(TFT_eSprite &sprite,
               uint16_t x, uint16_t y, uint16_t w, uint16_t h,
               float level, float peakLevel, bool isClipping);

/**
 * @brief Gestiona la lógica de sincronización del display de tiempo.
 *        Copia los datos de los buffers "sucios" a los "limpios" si se ha
 *        actualizado la información y ha pasado el tiempo de espera.
 */
void handleDisplaySynchronization();
