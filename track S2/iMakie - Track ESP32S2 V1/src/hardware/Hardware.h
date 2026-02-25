// src/hardware/Hardware.h
#pragma once

#include <Arduino.h>
#include <Button2.h> // Incluir la librería Button2

// ===================================
// --- ENUMERACIONES (Tipos de Datos) ---
// ===================================
enum class ButtonId {
    REC = 0, // Asignar valores para usar directamente como índice para onButtonPressCallbacks
    SOLO,
    MUTE,
    SELECT,
    ENCODER_SELECT, // <<<< Hasta aquí son 5 elementos para el array onButtonPressCallbacks
    FADER_TOUCH,    // <<<< Este y los siguientes no van en el array onButtonPressCallbacks
    UNKNOWN         // <<<< AÑADIDO: Valor para botones no identificados
};

// ===================================
// --- TIPOS DE CALLBACKS ---
// ===================================
typedef void (*ButtonPressCallback)();
typedef void (*ButtonEventCallback)(ButtonId buttonId);


// ** Botones **
extern Button2 buttonRec;
extern Button2 buttonSolo;
extern Button2 buttonMute;
extern Button2 buttonSelect;
extern Button2 buttonEncoderSelect;

// ** Sensor Táctil **
extern volatile bool isFaderTouched;


// ===================================
// --- FUNCIONES PÚBLICAS ---
// ===================================
void initHardware(); // Inicialización de todo el hardware

// ** Encoder **
bool updateEncoder(); // Actualiza el estado del encoder rotatorio

// ** Botones **
void updateButtons(); // Llama a .loop() para todos los botones Button2
// Los callbacks específicos para REC, SOLO, etc. si se usan índices, o por ButtonId directamente
void registerButtonPressCallback(ButtonId id, ButtonPressCallback callback);
void registerButtonEventCallback(ButtonEventCallback callback);
void registerFaderTouchCallback(ButtonPressCallback callback);
void registerFaderReleaseCallback(ButtonPressCallback callback);

// ** Neopixel **
void setNeopixelState(int neopixelIndex, uint8_t r, uint8_t g, uint8_t b);
void showNeopixels(); // Envía los colores actualizados a los Neopixels
void setNeopixelGlobalBrightness(uint8_t brightness);

// void updateNeopixelSequence(); // Descomentar si quieres iniciar la secuencia desde main.cpp
