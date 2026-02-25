// src/display/Display.cpp
#include <Arduino.h>
#include <driver/ledc.h>
#include "Display.h"        // Incluye las declaraciones públicas de este módulo
#include "hardware/encoder/Encoder.h"
#include "../config.h"      // Para #defines y ConnectionState enum


// ===================================
//  ACCESO A tft y sprites (extern)
// ===================================
extern TFT_eSPI tft;
extern TFT_eSprite header;
extern TFT_eSprite mainArea;
extern TFT_eSprite vuSprite;
extern TFT_eSprite vPotSprite;

// --- Variables "Privadas" de este Módulo con namespace anónimo ---
namespace {
    const int TFT_BL_CHANNEL = 0;
    uint8_t screenBrightness = 255;
}

// --- Prototipos de las funciones internas de este archivo .cpp ---
// ESTOS PROTOTIPOS DEBEN ESTAR AQUÍ, ANTES DE CUALQUIER FUNCIÓN QUE LOS USE.
void drawMainArea();
void drawVUMeters();
void drawHeaderSprite();
void drawOfflineScreen();
void drawInitializingScreen(); 
void drawButton(TFT_eSprite &sprite, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char* label, bool active, uint16_t activeColor);
void drawMeter(TFT_eSprite &sprite, uint16_t x, uint16_t y, uint16_t w, uint16_t h, float level, float peakLevel, bool isClipping);

// <<<<<<<<<<<<<<<<<<<<<<<< MOVIDO AQUÍ: EL PROTOTIPO DE drawVPotDisplay() >>>>>>>>>>>>>>>>>>>>
void drawVPotDisplay(); // Sin parámetro, usa la variable interna del módulo.
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// --- Variables Externas (definidas aquí, declaradas 'extern' en Display.h) ---
bool needsTOTALRedraw = true;
bool needsMainAreaRedraw = false;
bool needsHeaderRedraw = false;
bool needsVUMetersRedraw = false;
bool needsVPotRedraw = false; // Bandera para indicar cambio en vpot
volatile ConnectionState logicConnectionState = ConnectionState::CONNECTED; // Definición del extern


// --- ESTADOS DE CANAL (Declarados 'extern' aquí, definidos en main.cpp) ---
// Estas variables se actualizarán y decaerán en Display.cpp o main.cpp.
extern String assignmentString;
extern String trackName;
extern bool recStates;      
extern bool soloStates;     
extern bool muteStates;     
extern bool selectStates;   
extern bool vuClipState;
extern float vuPeakLevels;
extern float faderPositions;
extern float vuLevels; 
extern unsigned long vuLastUpdateTime;
extern unsigned long vuPeakLastUpdateTime;

// === VARIABLE INTERNA DEL MÓDULO PARA ALMACENAR EL NIVEL ACTUAL DEL VPot ===
static int8_t currentVPotLevel = VPOT_DEFAULT_LEVEL;

// =========================================================================
//  IMPLEMENTACIÓN DE setVPotLevel() (PÚBLICA)
// =========================================================================
void setVPotLevel(int8_t level) {
    if (level < VPOT_MIN_LEVEL) level = VPOT_MIN_LEVEL;
    if (level > VPOT_MAX_LEVEL) level = VPOT_MAX_LEVEL;
    if (level != currentVPotLevel) {
        currentVPotLevel = level;
        needsVPotRedraw = true; // Marca que el vPot necesita ser redibujado
        // log_v("VPot level changed to: %d", currentVPotLevel); // Descomentar si usas ESP_LOG
    }
}

// ******************************************************
// initDisplay()
// ******************************************************
void initDisplay() {
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BG_COLOR);
  
  // Configurar PWM para el backlight
  ledcAttach(TFT_BL, 5000, 8); // Canal 0, frecuencia 5kHz, resolución de 8 bits
  
  // ENCENDER la pantalla - este es el paso crucial que falta
  ledcWrite(TFT_BL, screenBrightness); // Valor entre 0-255
  
  // También puedes usar esto si screenBrightness es 0:
  // ledcWrite(TFT_BL, 128); // 50% de brillo
  
  setScreenBrightness(screenBrightness); // Esta función probablemente ya hace ledcWrite
  mainArea.createSprite(MAINAREA_WIDTH, MAINAREA_HEIGHT);
  header.createSprite(TFT_WIDTH, HEADER_HEIGHT);
  vuSprite.createSprite(TFT_WIDTH-MAINAREA_WIDTH, MAINAREA_HEIGHT);
  vPotSprite.createSprite(TFT_WIDTH, VPOT_HEIGHT);
  Serial.println("[SETUP] Módulo de Display iniciado.");
  
  // Establecer el vPot a su valor por defecto al inicializar.
  setVPotLevel(VPOT_DEFAULT_LEVEL); 
  needsTOTALRedraw = true; // Forzar un redibujo completo al inicio
}

// setScreenBrightness (tu implementación)
void setScreenBrightness(uint8_t brightness) {
  screenBrightness = brightness;
  ledcWrite(TFT_BL_CHANNEL, screenBrightness);
}

// drawOfflineScreen (tu implementación)
void drawOfflineScreen() {
  //tft.fillScreen(TFT_BLACK); // Limpiar toda la pantalla
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString("iMakie Control", tft.width() / 2, tft.height() / 2 - 20);
  
  String status_text = "Conectando...";
  uint16_t color = TFT_ORANGE;
  
  tft.setTextColor(color);
  tft.drawString(status_text, tft.width() / 2, tft.height() / 2 + 10);
}

// drawInitializingScreen (tu implementación)
void drawInitializingScreen() {
    Serial.println("drawInitializingScreen(): Dibujando pantalla de inicialización del DAW.");
    
    tft.fillScreen(TFT_BLUE); 
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString("MIDI Handshake OK!", tft.width() / 2, tft.height() / 2 - 30);
    
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("Esperando datos de proyecto del DAW...", tft.width() / 2, tft.height() / 2 + 10);
    
    Serial.println("drawInitializingScreen(): Pantalla de inicialización dibujada.");
}


// =========================================================================
//  IMPLEMENTACIÓN DE updateDisplay() (PÚBLICA)
// =========================================================================
void updateDisplay() {
    setScreenBrightness(screenBrightness);
    switch (logicConnectionState) {
        case ConnectionState::DISCONNECTED:
            static bool offline_screen_drawn = false;
            if (!offline_screen_drawn || needsTOTALRedraw) {
                drawOfflineScreen();
                offline_screen_drawn = true;
                needsTOTALRedraw = false;
            }
            return;

        case ConnectionState::INITIALIZING:
            static bool initializing_screen_drawn = false;
            if (!initializing_screen_drawn || needsTOTALRedraw) {
                drawInitializingScreen();
                initializing_screen_drawn = true;
                needsTOTALRedraw = false;
            }
            return;

        case ConnectionState::CONNECTED:
            static bool connected_screen_init = false;
            if (!connected_screen_init || needsTOTALRedraw) {
                drawHeaderSprite();
                drawMainArea();
                drawVUMeters();
                drawVPotDisplay(); // <<< Correcto: Ahora visible y sin parámetro
                
                needsTOTALRedraw = false;
                needsHeaderRedraw = false;
                needsMainAreaRedraw = false;
                needsVUMetersRedraw = false;
                needsVPotRedraw = false; 
                connected_screen_init = true;
            }

            if (needsHeaderRedraw) { drawHeaderSprite(); needsHeaderRedraw = false; }
            if (needsMainAreaRedraw) { drawMainArea(); needsMainAreaRedraw = false; }
            if (needsVUMetersRedraw) { drawVUMeters(); needsVUMetersRedraw = false; }
            if (needsVPotRedraw) { // Si la bandera de redibujo del vPot está activa
                drawVPotDisplay(); // <<< Correcto: Ahora visible y sin parámetro
                needsVPotRedraw = false;
            }
            break;
    }
}

// drawHeaderSprite (tu implementación)
void drawHeaderSprite() {
    // Limpiamos el sprite de la cabecera con el color de fondo
    header.fillSprite(TFT_MCU_DARKGRAY);
    int rectWidth = TFT_WIDTH - 60;  // Ancho casi completo con márgenes
    int rectHeight = 10;              // 7px de alto
    int rectX = (TFT_WIDTH - rectWidth) / 2;  // Centrado horizontal
    int rectY = (HEADER_HEIGHT - rectHeight) / 2;  // Centrado vertical

    // Dibujar fillRoundRect si selectStates es TRUE
    if (selectStates) {
        
        // Dibujar el rectángulo redondeado
        header.fillRoundRect(rectX, rectY, rectWidth, rectHeight, 3, TFT_MCU_BLUE);
        screenBrightness = 255;
        
        //header.drawRoundRect(rectX, rectY, rectWidth, rectHeight, 3, TFT_WHITE);
        
    }
    else {
        // Si no está seleccionado, dibujar un rectángulo normal
        header.fillRoundRect(rectX, rectY, rectWidth, rectHeight, 3, TFT_MCU_GRAY);
        screenBrightness = 070;
        
    }

    //header.drawString("Track 1", TFT_WIDTH/2, HEADER_HEIGHT / 2 -2);

    // Empujar el sprite actualizado a la pantalla TFT
    header.pushSprite(0, 0); // En la posición Y = 0 (arriba del todo)
}

// *************************************************************************
// Dibuja un botón con estado activo/inactivo
// *************************************************************************
void drawButton(TFT_eSprite &sprite, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char* label, bool active, uint16_t activeColor) {
  uint16_t btnColor = active ? activeColor : TFT_MCU_DARKGRAY;
  sprite.fillRoundRect(x, y, w, h, 5, btnColor);

  // --- CAMBIO AQUÍ: Usar setTextFont(2) ---
  sprite.setTextFont(2); // Selecciona la fuente de sistema número 2 (generalmente 5x7px)
  sprite.setTextSize(1); // Con esta fuente, setTextSize(1) es el tamaño base.
  // --- FIN DE CAMBIO ---

  // --- NUEVA LÓGICA: Texto negro para activo, blanco para inactivo ---
  uint16_t textColor = active ? TFT_BLACK : TFT_WHITE;

  sprite.setTextColor(textColor, btnColor);
  sprite.setTextDatum(MC_DATUM);
  sprite.drawString(label, x + w / 2, y + h / 2);
}

// drawMainArea (tu implementación)
void drawMainArea() {
    Serial.println("drawMainArea(): Dibujando área principal (botones y nombres de pista).");
    mainArea.fillSprite(TFT_BG_COLOR); // Ok, limpia el sprite
    //mainArea.fillSprite(TFT_RED); // Ok, limpia el sprite

    drawButton(mainArea, 10, 7, BUTTON_WIDTH, BUTTON_HEIGHT, "REC", recStates, TFT_REC_COLOR);
    drawButton(mainArea, 10+BUTTON_WIDTH+2, 7, BUTTON_WIDTH, BUTTON_HEIGHT, "SOLO", soloStates, TFT_SOLO_COLOR);
    drawButton(mainArea, 10+BUTTON_WIDTH+2+BUTTON_WIDTH+2,7, BUTTON_WIDTH, BUTTON_HEIGHT, "MUTE", muteStates, TFT_MUTE_COLOR);
        
    
    #ifdef LOAD_GFXFF
        // "Audio1" con fuente grande de ~40px
        mainArea.setTextColor(TFT_WHITE);
        mainArea.setFreeFont(&FreeSans24pt7b); // CAMBIADO A mainArea
        mainArea.setCursor(7, 115);           // Posición relativa al sprite mainArea, NO GLOBAL
        //mainArea.print("Audio1");
        mainArea.print(trackName);
        
        // dB con fuente mediana (~32px)
        mainArea.setFreeFont(&FreeSans18pt7b);
        mainArea.setCursor(7, 155);
        mainArea.print("-60 dB");
    #endif
    

    // Esto se mantendría al final
    mainArea.pushSprite(0, HEADER_HEIGHT);
    }

// drawMeter - Versión con picos como bordes
// Esta función dibuja un medidor de VU vertical (Volume Unit) en un sprite TFT.
// Muestra el nivel actual, un nivel de pico (como borde), y un indicador de clipping.
void drawMeter(TFT_eSprite &sprite, uint16_t x, uint16_t y, uint16_t w, uint16_t h, float level, float peakLevel, bool isClipping) {
    log_d("  drawMeter(): Invocado para VU en sprite_x=%d, sprite_y=%d, w=%d, h=%d. Level=%.3f, Peak=%.3f, Clipping=%d.", x, y, w, h, level, peakLevel, isClipping);
    
    // --- Configuración BÁSICA del MEDIDOR ---
    const int numSegments = 12; // Número total de segmentos en el medidor VU.
    const int padding = 2;      // Espacio (en píxeles) entre cada segmento del medidor.
    const int peakBorderWidth = 2; // Grosor del borde para indicar picos
    
    // --- VALIDACIÓN de PARÁMETROS ---
    if (numSegments <= 1 || h <= (padding * (numSegments - 1))) {
        log_e("  drawMeter(): ERROR: Parámetros de altura o segmentos inválidos. h=%d, numSegments=%d, padding=%d.", h, numSegments, padding);
        return;
    }
    
    // --- CÁLCULO de DIMENSIONES de SEGMENTOS ---
    const int segmentHeight = (h - padding * (numSegments - 1)) / numSegments;
    
    // --- CÁLCULO del RADIO para ESQUINAS REDONDEADAS ---
    const int cornerRadius = 5;

    // --- CONVERSIÓN de NIVELES a SEGMENTOS ACTIVOS ---
    size_t activeSegments = round(level * numSegments);
    size_t peakSegment_idx = round(peakLevel * numSegments);

    // --- AJUSTE del ÍNDICE del SEGMENTO de PICO ---
    if (peakSegment_idx > 0) peakSegment_idx--;
    if (activeSegments > 0 && peakSegment_idx < activeSegments - 1) {
        peakSegment_idx = activeSegments - 1;
    } else if (activeSegments == 0 && peakSegment_idx != (size_t)-1) {
        peakSegment_idx = (size_t)-1;
    }
    
    // --- DEFINICIÓN de COLORES ---
    uint16_t green_off  = tft.color565(0, 20, 0);     // Verde oscuro (apagado)
    uint16_t green_on   = TFT_GREEN;                  // Verde brillante (encendido)
    uint16_t yellow_off = tft.color565(20, 20, 0);    // Amarillo oscuro (apagado)
    uint16_t yellow_on  = TFT_YELLOW;                 // Amarillo brillante (encendido)
    uint16_t red_off    = tft.color565(20, 0, 0);     // Rojo oscuro (apagado)
    uint16_t red_on     = TFT_RED;                    // Rojo brillante (encendido)
    uint16_t peak_color = TFT_MCU_GRAY; // Color del borde de pico (amarillo claro)
    
    // --- DIBUJO de los SEGMENTOS del MEDIDOR ---
    for (int i = 0; i < numSegments; i++) {
        uint16_t segY = y + h - (static_cast<uint16_t>(i) + 1) * (segmentHeight + padding);
        uint16_t fillColor, borderColor = 0;
        bool hasPeakBorder = false;
        
        // --- LÓGICA para DETERMINAR COLOR de RELLENO ---
        if (static_cast<size_t>(i) < activeSegments) {
            if (i < 8) { 
                fillColor = green_on;   
            } else if (i < 10) { 
                fillColor = yellow_on;  
            } else { 
                fillColor = red_on;     
            }
        } else {
            if (i < 8) { 
                fillColor = green_off;  
            } else if (i < 10) { 
                fillColor = yellow_off; 
            } else { 
                fillColor = red_off;    
            }
        }
        
        // --- LÓGICA para BORDE de PICO ---
        if (static_cast<size_t>(i) == peakSegment_idx && peakLevel > level + 0.001f) {
            borderColor = peak_color;
            hasPeakBorder = true;
        }
        
        // --- LÓGICA para INDICADOR de CLIPPING ---
        if (static_cast<size_t>(i) == (numSegments - 1) && isClipping) {
            fillColor = red_on;
            borderColor = peak_color; // Borde blanco para destacar clipping
            hasPeakBorder = true;
            log_i("  drawMeter(): Track CLIP activo para Segmento %d (y=%d), forzando ROJO con borde.", i, segY);
        }

        // --- DIBUJO del SEGMENTO con o sin BORDE de PICO ---
        if (hasPeakBorder) {
            // Dibujar segmento con borde de pico
            sprite.fillRoundRect(x, segY, w, segmentHeight, cornerRadius, fillColor);
            sprite.drawRoundRect(x, segY, w, segmentHeight, cornerRadius, borderColor);
            // Dibujar borde interior más grueso
            sprite.drawRoundRect(x+1, segY+1, w-2, segmentHeight-2, cornerRadius-1, borderColor);
            log_d("  drawMeter(): Segmento %d con borde de pico en %dx%d", i, x, segY);
        } else {
            // Dibujar segmento normal sin borde
            sprite.fillRoundRect(x, segY, w, segmentHeight, cornerRadius, fillColor);
            log_d("  drawMeter(): Segmento %d normal en %dx%d", i, x, segY);
        }
    }
    log_d("  drawMeter(): Dibujo de VU en %dx%d completado.", x, y);
}

// drawVUMeters (tu implementación)
void drawVUMeters() {
    //Serial.println("drawVUMeters() llamado. Limpiando vuSprite.");
    //vuSprite.fillSprite(TFT_BG_COLOR); 
    vuSprite.fillSprite(TFT_MCU_DARKGRAY); 
    int track = 0;
        uint16_t baseX = track * MAINAREA_HEIGHT; 
        uint16_t meter_x = 3; // X relativo al sprite del VU
        uint16_t meter_y = 4;                              // Y relativo al sprite del VU
        uint16_t meter_w = 42;
        uint16_t meter_h = MAINAREA_HEIGHT-10;
        drawMeter(
            vuSprite,           // Argumento 1: el sprite
            meter_x,            // Argumento 2: posición X
            meter_y,            // Argumento 3: posición Y
            meter_w,            // Argumento 4: ancho (width)
            meter_h,            // Argumento 5: alto (height)
            vuLevels,           // Argumento 6: nivel instantáneo
            vuPeakLevels,       // Argumento 7: nivel de pico
            vuClipState         // Argumento 8: estado de clip
        );
    
    vuSprite.pushSprite(MAINAREA_WIDTH, HEADER_HEIGHT);
    //Serial.println("drawVUMeters FInished");
}
// ****************************************************************************
// Lógica de decaimiento de los vúmetros y retención de picos (PARA UN SOLO CANAL)
// ****************************************************************************
void handleVUMeterDecay() {
    // log_v("handleVUMeterDecay() llamado."); // Comentado temporalmente para no sobrecargar el serial.
    const unsigned long DECAY_INTERVAL_MS = 150;    // Frecuencia de decaimiento del nivel normal
    const unsigned long PEAK_HOLD_TIME_MS = 1500;   // Tiempo que el pico se mantiene visible
    const float DECAY_AMOUNT = 1.0f / 12.0f;        // Cantidad de decaimiento (aproximadamente 1 segmento)
    
    unsigned long currentTime = millis(); // Obtener el tiempo actual una sola vez
    bool anyVUMeterChanged = false; // Flag para saber si hubo cambio por decaimiento
    // --- 1. Decaimiento del Nivel Normal (vuLevels) ---
    if (vuLevels > 0) {
        if (currentTime - vuLastUpdateTime > DECAY_INTERVAL_MS) {
            float oldLevel = vuLevels;
            vuLevels -= DECAY_AMOUNT;
            if (vuLevels < 0.0f) { // Asegura que no baje de 0
                vuLevels = 0.0f;
            }
            vuLastUpdateTime = currentTime;
            anyVUMeterChanged = true;
            // log_v("VU Level decayed from %.3f to %.3f.", oldLevel, vuLevels);
        }
    }
    
    // --- 2. Decaimiento/Reinicia de Nivel de Pico (vuPeakLevels) ---
    if (vuPeakLevels > 0) {
        if (currentTime - vuPeakLastUpdateTime > PEAK_HOLD_TIME_MS) {
            // El pico "salta" hacia abajo hasta el nivel normal actual.
            if (vuPeakLevels > vuLevels) { 
                float oldPeakLevel = vuPeakLevels;
                vuPeakLevels = vuLevels; // Igualar el pico al nivel actual
                anyVUMeterChanged = true;
                // log_v("VU Peak decayed from %.3f to %.3f (jump to current level).", oldPeakLevel, vuPeakLevels);
            }
        }
    }
    // --- 3. Lógica de Seguridad para el Pico ---
    // Asegurarse de que el pico nunca esté por debajo del nivel actual.
    if (vuPeakLevels < vuLevels) {
        // log_w("VU Peak (%.3f) era menor que VU Level (%.3f). Corrigiendo.", vuPeakLevels, vuLevels);
        vuPeakLevels = vuLevels;
        vuPeakLastUpdateTime = currentTime; // Reiniciar el temporizador de retención del pico
        anyVUMeterChanged = true;
    }
    // Si cualquier vúmetro cambió, activar el flag de redibujo específico para vúmetros
    if (anyVUMeterChanged) {
        needsVUMetersRedraw = true;
        // log_v("handleVUMeterDecay(): anyVUMeterChanged es TRUE. needsVUMetersRedraw = true.");
    }
}


// =========================================================================
//  IMPLEMENTACIÓN DE drawVPotDisplay() (INTERNA)
//  Ahora usa la variable interna `currentVPotLevel`
// =========================================================================
void drawVPotDisplay() {
    // 1. Limpiar el sprite
    vPotSprite.fillSprite(TFT_MCU_DARKGRAY);
    int currentVPotLevel = Encoder::currentVPotLevel;

    // 2. Configuración de segmentos
    const int totalSegments = 7;     
    const int centerWidth = 30;      
    const int gapVPot = 2;           
    const int cornerRadius = 5; 
    
    int availableWidthForSegments = vPotSprite.width() - centerWidth - (2 * totalSegments * gapVPot);
    int segmentWidthVPot = availableWidthForSegments / (2 * totalSegments); 
    int actualDrawingWidth = (2 * totalSegments * segmentWidthVPot) + (2 * totalSegments * gapVPot) + centerWidth;
    int startX_in_sprite = (vPotSprite.width() - actualDrawingWidth) / 2;
    int segment_y = 3; 
    int segment_height = vPotSprite.height() - 6;

    // 3. Dibujar segmentos negativos (izquierda)
    for (int i = 0; i < totalSegments; i++) {
        int x = startX_in_sprite + (i * (segmentWidthVPot + gapVPot));
        bool active = (currentVPotLevel < 0 && i >= (totalSegments + currentVPotLevel));
        uint16_t color = active ? TFT_MCU_BLUE : TFT_MCU_GRAY;
        vPotSprite.fillRoundRect(x, segment_y, segmentWidthVPot, segment_height, cornerRadius, color);
    }

    // 4. Dibujar segmento central
    int centerX_in_sprite = startX_in_sprite + (totalSegments * (segmentWidthVPot + gapVPot));
    vPotSprite.fillRoundRect(centerX_in_sprite, segment_y, centerWidth, segment_height, cornerRadius, TFT_MCU_BLUE); 

    // 5. Dibujar segmentos positivos (derecha)
    for (int i = 0; i < totalSegments; i++) {
        int x = centerX_in_sprite + centerWidth + gapVPot + (i * (segmentWidthVPot + gapVPot));
        bool active = (currentVPotLevel > 0 && i < currentVPotLevel);
        uint16_t color = active ? TFT_MCU_BLUE : TFT_MCU_GRAY;
        vPotSprite.fillRoundRect(x, segment_y, segmentWidthVPot, segment_height, cornerRadius, color);
    }

    // 6. Empujar sprite a pantalla principal
    vPotSprite.pushSprite(0, MAINAREA_HEIGHT + HEADER_HEIGHT);

    // log opcional
    Serial.print("vPot horizontal bar rendered with level: ");
    Serial.println(currentVPotLevel);
    Serial.println(Encoder::currentVPotLevel);
}