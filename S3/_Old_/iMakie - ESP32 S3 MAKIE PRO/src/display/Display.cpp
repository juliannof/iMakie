// src/display/Display.cpp

#include "Display.h"
#include "../midi/MIDIProcessor.h"  // ¡AÑADE ESTA LÍNEA!
#include "../config.h" // Necesario para acceder a variables de estado y objetos TFT

// --- Variables "Privadas" de este Módulo ---
namespace {
    const int TFT_BL_CHANNEL = 0;
    uint8_t screenBrightness = 255;
}



// --- Prototipos de las funciones de dibujo internas ---
void drawMainArea();
void drawVUMeters();
void drawHeaderSprite();
void drawOfflineScreen();
void drawButton(TFT_eSprite &sprite, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char* label, bool active, uint16_t activeColor);
void drawMeter(TFT_eSprite &sprite, uint16_t x, uint16_t y, uint16_t w, uint16_t h, float level);

// ******************************************************
// iniciar
// ******************************************************

void initDisplay() {
  delay(2000); // Espera para que te dé tiempo a abrir el monitor

  // Usamos log_e (Error) para forzar que se vea en consola con DEBUG_LEVEL=1
  log_e("=== 🔎 DIAGNÓSTICO DE MEMORIA ESP32-S3 ===");

  if (psramFound()) {
    // Usamos printf style que soportan los macros de log automáticamente
    log_e("✅ ÉXITO: PSRAM Detectada. Total: %u bytes (%.2f MB) | Libre: %u bytes", 
          ESP.getPsramSize(), 
          (float)ESP.getPsramSize()/1024.0f/1024.0f, 
          ESP.getFreePsram());
  } else {
    log_e("❌ ERROR CRÍTICO: PSRAM NO DETECTADA.");
    log_e("👉 Causas probables:");
    log_e("   1. build_flags incorrectos (falta -DBOARD_HAS_PSRAM)");
    log_e("   2. memory_type incorrecto (debe ser qio_opi para N16R8)");
    log_e("   3. Hardware dañado.");
  }
  
  tft.init();
  tft.initDMA(); // <--- OBLIGATORIO: Inicializa el canal DMA
  tft.setRotation(3  );
  tft.fillScreen(TFT_BG_COLOR);
  
  ledcSetup(TFT_BL_CHANNEL, 12000, 8);
  ledcAttachPin(TFT_BL, TFT_BL_CHANNEL);
  setScreenBrightness(screenBrightness);

  mainArea.createSprite(MAIN_AREA_WIDTH, SCREEN_HEIGHT - HEADER_HEIGHT);
  header.createSprite(SCREEN_WIDTH, HEADER_HEIGHT);
  vuSprite.createSprite(VU_METER_wIDTH, SCREEN_HEIGHT - HEADER_HEIGHT);
  log_i("[SETUP] Módulo de Display iniciado.");
}

void setScreenBrightness(uint8_t brightness) {
  screenBrightness = brightness;
  ledcWrite(TFT_BL_CHANNEL, screenBrightness);
}


// *************************************************************************
// NUEVA FUNCIÓN: Dibuja una pantalla de "Sincronizando MIDI" para el estado MIDI_HANDSHAKE_COMPLETE
// Se usa para indicar que el handshake MIDI ha finalizado pero se espera información vital del DAW.
// *************************************************************************
void drawInitializingScreen() {
    log_v("drawInitializingScreen(): Dibujando pantalla de inicialización del DAW.");
    
    // Limpia el fondo (o dibuja un fondo parcial si quieres mantener algo de la MainArea visible)
    tft.fillScreen(TFT_DARKGREY); 

    // Mensaje de estado principal
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("MIDI Handshake OK!", tft.width() / 2, tft.height() / 2 - 30);
    
    // Mensaje adicional de espera o progreso
    tft.setTextColor(TFT_SKYBLUE);
    tft.drawString("Esperando datos de proyecto del DAW...", tft.width() / 2, tft.height() / 2 + 10);
    
    log_v("drawInitializingScreen(): Pantalla de inicialización dibujada.");
}

// *************************************************************************
// Cabecera con Timecode y Asignación
// *************************************************************************

void drawHeaderSprite() {
    if (!needsHeaderRedraw) {
        return; // No necesita redibujarse
    }
    
    // Limpiamos el sprite de la cabecera con el color de fondo
    header.fillSprite(TFT_BLUE);

    // Formatear el texto según el modo actual
    String displayText;
    if (currentTimecodeMode == MODE_BEATS) {
        displayText = formatBeatString();
    } else {
        displayText = formatTimecodeString();
    }

    // --- DIBUJAR DISPLAY DE ASIGNACIÓN (2 dígitos) ---
    // Podrías añadir esto si necesitas mostrar el canal o asignación
    // header.setTextDatum(ML_DATUM);
    // header.setTextColor(TFT_CYAN);
    // header.setFreeFont(&FreeMonoBold9pt7b);
    // header.drawString("CH1", 5, HEADER_HEIGHT / 2);

    // --- DIBUJAR CÓDIGO DE TIEMPO/BEATS (10 dígitos) ---
    header.setTextDatum(MC_DATUM); // Alinear el texto en el centro
    header.setFreeFont(&FreeMonoBold12pt7b);
    header.setTextSize(1);

    // Elegir color según el modo
    if (currentTimecodeMode == MODE_BEATS) {
        // BEATS - color verde/cian
        header.setTextColor(TFT_CYAN);
    } else {
        // TIMECODE - color naranja
        header.setTextColor(TFT_ORANGE);
    }
    
    // Posición: en el centro de la pantalla
    log_d("drawHeaderSprite: %s: '%s' (Len: %d)", 
          (currentTimecodeMode == MODE_BEATS) ? "BEATS" : "TIMECODE",
          displayText.c_str(), 
          displayText.length());
    
    header.drawString(displayText.c_str(), SCREEN_WIDTH / 2, HEADER_HEIGHT / 2);

    // --- DIBUJAR INDICADOR DE MODO ---
    header.setTextDatum(MR_DATUM); // Alinear a la derecha
    header.setFreeFont(&FreeMono9pt7b);
    header.setTextSize(1);
    
    if (currentTimecodeMode == MODE_BEATS) {
        header.setTextColor(TFT_GREEN);
        header.drawString("BEATS", SCREEN_WIDTH - 10, HEADER_HEIGHT / 2);
    } else {
        header.setTextColor(TFT_YELLOW);
        header.drawString("SMPTE", SCREEN_WIDTH - 10, HEADER_HEIGHT / 2);
    }

    // --- DIBUJAR INDICADOR DE ACTIVIDAD MIDI ---
    static unsigned long lastMIDIActivity = 0;
    if (millis() - lastMIDIActivity < 200) { // Parpadeo por 200ms
        header.fillCircle(SCREEN_WIDTH - 30, 10, 3, TFT_GREEN);
    }

    // Empujar el sprite actualizado a la pantalla TFT
    header.pushSprite(0, 0); // En la posición Y = 0 (arriba del todo)
    
    needsHeaderRedraw = false; // Resetear flag
}

// *************************************************************************
// Actualiza la pantalla según el estado de conexión y la necesidad de redibujar
// *************************************************************************
// Tengo que asumir que tu updateDisplay tiene la siguiente estructura general
// Esta es la corrección que asume que tu updateDisplay() original terminaba después del primer 'if'
void updateDisplay() {
    
    log_v("updateDisplay() llamado. Estado de conexión: %d, needsTOTALRedraw: %d.", (int)logicConnectionState, needsTOTALRedraw);

    
    // --- LÓGICA DE REDIBUJO TOTAL (prioridad máxima) ---
    if (needsTOTALRedraw) {
        log_i("updateDisplay(): needsTOTALRedraw es TRUE. Realizando redibujo completo para el estado actual.");
        if (logicConnectionState == ConnectionState::CONNECTED) {
            drawHeaderSprite();
            drawMainArea();
            drawVUMeters(); // <--- Llamada a VUMeters en redibujo total
            log_v("updateDisplay(): Redibujo total en CONNECTED: Header, MainArea y VUMeters dibujados.");
        } else if (logicConnectionState == ConnectionState::MIDI_HANDSHAKE_COMPLETE) {
            drawInitializingScreen();
            log_v("updateDisplay(): Redibujo total en MIDI_HANDSHAKE_COMPLETE: Pantalla de inicialización dibujada.");
        } else { // DISCONNECTED o AWAITING_SESSION
            drawOfflineScreen();
            log_v("updateDisplay(): Redibujo total en DISCONNECTED/AWAITING_SESSION: Pantalla offline dibujada.");
        }
        needsTOTALRedraw = false; // Resetear la bandera total después de un redibujo completo.
        needsMainAreaRedraw = false; // Resetear todas las banderas específicas también
        needsHeaderRedraw = false;
        needsVUMetersRedraw = false;
        // NO HAY RETURN AQUI. Si se hace un redibujo total, el resto de las banderas ya están false,
        // o si una se puso TRUE, ya se ha gestionado.
    }

    // --- LÓGICA DE REDIBUJO PARCIAL (solo si no se hizo un redibujo total en este ciclo) ---
    // Estas llamadas solo se ejecutarán si needsTOTALRedraw fue false al inicio del updateDisplay,
    // O si acaba de volverse false y estamos conectados.
    
    // ** SOLO EJECUTAR ESTO SI ESTAMOS CONECTADOS A LA DAW **
    if (logicConnectionState == ConnectionState::CONNECTED) {
        // Log para depuración, si estas banderas son las que están activas
        log_v("updateDisplay(): Chequeando banderas parciales: Header=%d, Main=%d, VU=%d.", needsHeaderRedraw, needsMainAreaRedraw, needsVUMetersRedraw);
        

        if (needsHeaderRedraw) {
            drawHeaderSprite();
            needsHeaderRedraw = false; // Resetear bandera
            log_v("updateDisplay(): Redibujando Header.");
        }
        if (needsMainAreaRedraw) {
            drawMainArea();
            needsMainAreaRedraw = false; // Resetear bandera
            log_v("updateDisplay(): Redibujando Main Area.");
        }
        if (needsVUMetersRedraw) {
            drawVUMeters(); // <--- LLAMADA CRUCIAL PARA LOS VUMETROS
            needsVUMetersRedraw = false; // Resetear bandera
            log_v("updateDisplay(): Redibujando VUMeters.");
        }
    } else {
        // Si no estamos conectados y no hubo un redibujo total (ej. si needsTOTALRedraw se acaba de resetear)
        // Podrías tener aquí lógica para otros estados offline
        log_v("updateDisplay(): No conectado y no necesita redibujo total.");
    }
    // No hay mas código. La función termina aquí.
}


// *************************************************************************
// Dibuja un botón con estado activo/inactivo
// *************************************************************************

// Añadimos 'inactiveColor' al final
void drawButton(TFT_eSprite &sprite, int x, int y, int w, int h, const char* label, bool active, uint16_t activeColor, uint16_t textColor, uint16_t inactiveColor = 0x222222) {
    
    // Usamos el color inactivo que le pasamos
    uint16_t btnColor = active ? activeColor : inactiveColor;
    
    // Dibujar Fondo (Radio 8 queda muy "NeoTrellis")
    sprite.fillRoundRect(x, y, w, h, 8, btnColor); 
    
    // Borde sutil para darle volumen (Opcional)
    // sprite.drawRoundRect(x, y, w, h, 8, active ? TFT_WHITE : 0x444444);

    sprite.setTextColor(textColor, btnColor);
    sprite.setTextSize(1);
    sprite.setTextDatum(MC_DATUM);
    sprite.drawString(label, x + (w / 2), y + (h / 2));
}




// *************************************************************************
// Dibuja un VU Meter con indicador de pico
// *************************************************************************
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
    uint16_t green_off  = tft.color565(0, 50, 0);     // Verde oscuro (apagado)
    uint16_t green_on   = TFT_GREEN;                  // Verde brillante (encendido)
    uint16_t yellow_off = tft.color565(50, 50, 0);    // Amarillo oscuro (apagado)
    uint16_t yellow_on  = TFT_YELLOW;                 // Amarillo brillante (encendido)
    uint16_t red_off    = tft.color565(50, 0, 0);     // Rojo oscuro (apagado)
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
    log_e("  drawMeter(): Dibujo de VU en %dx%d completado.", x, y);
}

// drawVUMeters
void drawVUMeters() {
    // 1. Limpiar Sprite
    vuSprite.fillSprite(TFT_MCU_DARKGRAY); 
    
    // 2. Definir coordenadas relativas al sprite
    uint16_t meter_x = 3;
    uint16_t meter_y = 3;
    uint16_t meter_w = 34;
    uint16_t meter_h = SCREEN_HEIGHT - HEADER_HEIGHT - 6;

    // --- CORRECCIÓN: Declarar y Asignar variables LOCALES ---
    // Leemos de las variables globales que actualiza MIDIProcessor
    float nivelNormalizado = masterMeterLevel; 
    float picoNormalizado = masterPeakLevel;
    bool clip = masterClip;

    // 3. Llamar a la función de dibujado con las variables que acabamos de crear
    drawMeter(
        vuSprite, 
        meter_x, 
        meter_y, 
        meter_w, 
        meter_h, 
        nivelNormalizado, // Ahora sí existen
        picoNormalizado, 
        clip
    );
    
    // 4. Volcar a pantalla
    vuSprite.pushSprite(MAIN_AREA_WIDTH, HEADER_HEIGHT);
}


// *************************************************************************
// Área Principal con los botones y nombres de pista
// *************************************************************************

void drawMainArea() {
    mainArea.fillSprite(TFT_BLACK);

    // 1. Calcular rejilla base
    int cellW = MAIN_AREA_WIDTH / 8; 
    int cellH = (SCREEN_HEIGHT - HEADER_HEIGHT) / 4; 
    
    // 2. Definir TAMAÑO CUADRADO para el botón
    // Cogemos el lado más pequeño y le quitamos margen para que "respiren"
    int btnSize = ((cellW < cellH) ? cellW : cellH) - 8; 

    // Seleccionamos etiquetas y mapa de colores según página
    const char** labels;
    const byte* colors; // Mapa de colores (indices 0-7)

    if (currentPage == 1) {
        labels = globalShiftPressed ? labels_PG1_SHIFT : labels_PG1;
        colors = LED_COLORS_PG1; // Usamos el array de colores de config.h
    } else {
        labels = labels_PG2;
        colors = LED_COLORS_PG2;
    }

    for (int i = 0; i < 32; i++) {
        int fila = i / 8;
        int col = i % 8;
        
        // 3. CENTRAR EL CUADRADO EN LA CELDA
        int x = (col * cellW) + (cellW - btnSize) / 2;
        int y = (fila * cellH) + (cellH - btnSize) / 2;

        // 4. TRADUCIR COLOR DE LA PALETA A COLOR PANTALLA
        // (Esto hace que coincida visualmente con el LED físico)
        uint16_t activeColor = TFT_WHITE;
        switch(colors[i]) {
            case 1: activeColor = TFT_RED; break;
            case 2: activeColor = TFT_GREEN; break;
            case 3: activeColor = 0x03FF; /*Azul Vivo*/ break; 
            case 4: activeColor = TFT_YELLOW; break;
            case 5: activeColor = TFT_CYAN; break;
            case 6: activeColor = TFT_MAGENTA; break;
            case 7: activeColor = TFT_WHITE; break;
            default: activeColor = TFT_LIGHTGREY; break; 
        }

        // Color del texto:
        // Si pulsamos SHIFT, texto Amarillo. Si no, Blanco o Negro según contraste.
        uint16_t txtColor = globalShiftPressed ? TFT_YELLOW : TFT_WHITE;
        if (btnState[i] && (activeColor == TFT_WHITE || activeColor == TFT_YELLOW || activeColor == TFT_CYAN)) {
             txtColor = TFT_BLACK; // Texto negro si el botón encendido es muy claro
        }

        // Color INACTIVO:
        // En lugar de gris plano, usamos un gris muy oscuro o el color atenuado
        // para simular la goma del botón apagado.
        uint16_t inactiveColor = 0x18E3; // Gris oscuro neutro (simula silicona)

        // 5. DIBUJAR
        drawButton(
            mainArea,
            x, y,
            btnSize, btnSize,       // ¡Ancho y Alto IGUALES!
            labels[i],
            btnState[i],            // Estado
            activeColor,            // Color vivo si ON
            txtColor,
            inactiveColor           // Color goma si OFF (Nuevo parámetro, ver abajo)
        );
    }

    mainArea.pushSprite(0, HEADER_HEIGHT);
}

// *************************************************************************
// Pantalla de Espera cuando no hay conexión
// *************************************************************************

void drawOfflineScreen() {
  //tft.fillScreen(TFT_BLACK); // Limpiar toda la pantalla
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString("iMakie Control", tft.width() / 2, tft.height() / 2);
  
  //String status_text = "Conectando...";
  //uint16_t color = TFT_ORANGE;
  
  //tft.setTextColor(color);
  //tft.drawString(status_text, tft.width() / 2, tft.height() / 2 + 10);
}



// Fin del codigo
