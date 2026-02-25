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

  mainArea.createSprite(SCREEN_WIDTH, 140);
  header.createSprite(SCREEN_WIDTH, HEADER_HEIGHT);
  vuSprite.createSprite(SCREEN_WIDTH, VU_METER_HEIGHT);
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

void drawButton(TFT_eSprite &sprite, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char* label, bool active, uint16_t activeColor) {
  uint16_t btnColor = active ? activeColor : TFT_BUTTON_COLOR;
  sprite.fillRoundRect(x, y, w, h, 5, btnColor);
  sprite.setTextColor(TFT_WHITE, btnColor);
  sprite.setTextSize(1);
  sprite.setTextDatum(MC_DATUM);
  sprite.drawString(label, x + w / 2, y + h / 2);
}




// *************************************************************************
// Dibuja un VU Meter con indicador de pico
// *************************************************************************
void drawMeter(TFT_eSprite &sprite, uint16_t x, uint16_t y, uint16_t w, uint16_t h, float level, float peakLevel, bool isClipping) {
    log_v("  drawMeter(): Invocado para VU en sprite_x=%d, sprite_y=%d, w=%d, h=%d. Level=%.3f, Peak=%.3f, Clipping=%d.",
          x, y, w, h, level, peakLevel, isClipping);

    const int numSegments = 12; // Número total de segmentos del medidor
    const int padding = 1;      // Espacio (en píxeles) entre cada segmento
    
    // Calcular la altura de cada segmento, distribuyendo el espacio restante uniformemente
    if (numSegments <= 1 || h <= (padding * (numSegments - 1))) { // Evitar división por cero o segmentos negativos
        log_e("  drawMeter(): ERROR: Parámetros de altura o segmentos inválidos. h=%d, numSegments=%d, padding=%d.", h, numSegments, padding);
        return; 
    }
    const int segmentHeight = (h - padding * (numSegments - 1)) / numSegments;
    log_v("  drawMeter(): numSegments=%d, padding=%d, segmentHeight=%d.", numSegments, padding, segmentHeight);

    // Calcular la cantidad de segmentos que deben estar "activos" según el nivel instantáneo
    size_t activeSegments = round(level * numSegments); // Convertir nivel flotante a número de segmentos activos
    // Calcular el índice del segmento de pico (0-11)
    size_t peakSegment_idx = round(peakLevel * numSegments); // Convertido a número de segmento (1-12)
    
    log_v("  drawMeter(): Calculado: activeSegments=%d (from level %.3f), peakSegment_idx=%d (from peak %.3f).",
          activeSegments, level, peakSegment_idx, peakLevel);

    // Por seguridad, el pico no puede estar por debajo del nivel actual.
    // Ajustamos peakSegment_idx para que sea un índice, y para que nunca sea < activeSegments
    if (peakSegment_idx > 0) peakSegment_idx--; // Convertir cantidad (1-12) a índice (0-11)
    
    if (activeSegments > 0 && peakSegment_idx < activeSegments - 1) { // Si hay nivel y el pico es menor que el último segmento activo
        peakSegment_idx = activeSegments - 1; // El pico se ajusta al último segmento activo
        log_v("  drawMeter(): Ajustando peakSegment_idx -> %u (debajo del nivel activo).", (unsigned int)peakSegment_idx);
    } else if (activeSegments == 0 && peakSegment_idx != (size_t)-1) { // Si no hay nivel, no debería haber pico
        peakSegment_idx = (size_t)-1; // -1 como marcador para no visible
        log_v("  drawMeter(): Ajustando peakSegment_idx -> -1 (no hay nivel activo).");
    }
    log_v("  drawMeter(): Final: activeSegments=%u, peakSegment_idx=%u.", (unsigned int)activeSegments, (unsigned int)peakSegment_idx);


    // Colores para los segmentos del vúmetro
    uint16_t green_off = tft.color565(0, 40, 0);       // Verde oscuro para segmentos inactivos
    uint16_t green_on  = TFT_GREEN;                    // Verde brillante para segmentos activos
    uint16_t yellow_off = tft.color565(40, 40, 0);     // Amarillo oscuro
    uint16_t yellow_on  = TFT_YELLOW;                   // Amarillo brillante
    uint16_t red_off   = tft.color565(40, 0, 0);       // Rojo oscuro
    uint16_t red_on    = TFT_RED;                      // Rojo brillante
    uint16_t peak_color = tft.color565(150, 150, 150); // Color para el indicador de pico

    // Bucle para dibujar cada segmento de abajo hacia arriba
    for (int i = 0; i < numSegments; i++) {
        // Calcular la coordenada Y del segmento actual (de abajo hacia arriba), relativo al sprite
        // Se usa static_cast<uint16_t>(i) para asegurar la compatibilidad de tipo.
        uint16_t segY = y + h - (static_cast<uint16_t>(i) + 1) * (segmentHeight + padding);
        uint16_t selectedColor; // Color que se usará para el segmento actual
        
        // Determinar el color base del segmento (activo, inactivo, o pico)
        if (static_cast<size_t>(i) == peakSegment_idx && peakLevel > level + 0.001f) { // Si es el segmento de pico y el pico está por encima del nivel actual
            selectedColor = peak_color;
            log_v("    drawMeter(): Segment %d (y=%d): Color PICO.", i, segY);
        } else if (static_cast<size_t>(i) < activeSegments) { // Si este segmento está por debajo (o en) el nivel actual
            if (i < 8) { selectedColor = green_on;}           // Primeros 8 segmentos (verde)
            else if (i < 10) { selectedColor = yellow_on;}    // Siguientes 2 segmentos (amarillo)
            else { selectedColor = red_on;}                  // Últimos 2 segmentos (rojo)
            log_v("    drawMeter(): Segment %d (y=%d): Color ACTIVO (%04X).", i, segY, selectedColor);
        } else { // Si este segmento está por encima del nivel activo (fondo)
            if (i < 8) { selectedColor = green_off;}         // Primeros 8 segmentos (verde oscuro)
            else if (i < 10) { selectedColor = yellow_off;}   // Siguientes 2 segmentos (amarillo oscuro)
            else { selectedColor = red_off;}                 // Últimos 2 segmentos (rojo oscuro)
            log_v("    drawMeter(): Segment %d (y=%d): Color INACTIVO (%04X).", i, segY, selectedColor);
        }

        // --- LÓGICA DE CLIPPING (sobrescribe cualquier otro color para el último segmento) ---
        // Si el estado de clip está activo, el segmento más alto SIEMPRE es rojo brillante.
        if (static_cast<size_t>(i) == (numSegments - 1) && isClipping) {
            selectedColor = red_on;
            log_i("  drawMeter(): Track CLIP activo para Segmento %d (y=%d), forzando ROJO.", i, segY);
        }
        
        // Dibujar el rectángulo del segmento
        sprite.fillRect(x, segY, w, segmentHeight, selectedColor);
        log_v("  drawMeter(): Dibujado rect para Segment %d: x=%d, y=%d, w=%d, h=%d, color=%04X.", x, segY, w, segmentHeight, selectedColor);
    }
}



// src/display/Display.cpp (dentro de drawVUMeters)

// *************************************************************************
// VU Meters (Dibuja todos los medidores en el sprite `vuSprite`)
// *************************************************************************
void drawVUMeters() {
    log_v("drawVUMeters() llamado. Limpiando vuSprite.");
    // Usamos TFT_BG_COLOR (negro) para que el fondo se vea cuando los VUs de reposo se dibujen oscuros.
    vuSprite.fillSprite(TFT_BG_COLOR); 

    for (int track = 0; track < 8; track++) {
        // --- NO HAY OPTIMIZACIÓN AQUÍ. drawMeter() se llama SIEMPRE para TODOS los tracks. ---
        // Esto asegura que el vúmetro esté siempre visible, mostrando su estado de reposo o activo.

        uint16_t baseX = track * TRACK_WIDTH;
        uint16_t meter_x = baseX + (TRACK_WIDTH - 20) / 2; // X relativo al sprite del VU
        uint16_t meter_y = 0;                              // Y relativo al sprite del VU
        uint16_t meter_w = 20;
        uint16_t meter_h = VU_METER_HEIGHT;

        // Logs que muestran los niveles para cada track antes de dibujar, independientemente de la actividad.
        log_v("  drawVUMeters(): VU Meter para Track %d: dibujando en sprite_x=%d, sprite_y=%d, w=%d, h=%d.", track, meter_x, meter_y, meter_w, meter_h);
        log_v("    drawVUMeters(): Niveles para Track %d: vuLevels=%.3f, vuPeakLevels=%.3f, vuClipState=%d.",
              track, vuLevels[track], vuPeakLevels[track], vuClipState[track]);
        
        // --- LLAMADA a drawMeter (SIEMPRE para TODOS los tracks) ---
        drawMeter(
            vuSprite,           // Argumento 1: el sprite
            meter_x,            // Argumento 2: posición X
            meter_y,            // Argumento 3: posición Y
            meter_w,            // Argumento 4: ancho (width)
            meter_h,            // Argumento 5: alto (height)
            vuLevels[track],    // Argumento 6: nivel instantáneo
            vuPeakLevels[track],// Argumento 7: nivel de pico
            vuClipState[track]  // Argumento 8: estado de clip
        );
    }
    
    // Empuja el sprite de vúmetros a la pantalla física.
    // Esto asegura que toda el área de los VUs se actualiza en la pantalla física.
    vuSprite.pushSprite(0, VU_METER_AREA_Y);
    log_v("drawVUMeters(): vuSprite empujado a la pantalla en (screen_x=0, screen_y=%d).", VU_METER_AREA_Y);
}



// *************************************************************************
// Área Principal con los botones y nombres de pista
// *************************************************************************

void drawMainArea() {
    log_v("drawMainArea(): Dibujando área principal (botones y nombres de pista)."); // Log de inicio de la función
    mainArea.fillSprite(TFT_BG_COLOR); // Limpia el sprite del área principal

    // Dibuja líneas divisorias verticales entre las pistas para separar visualmente los canales
    for (int i=1; i<8; ++i) {
        log_v("  drawMainArea(): Dibujando línea vertical en X=%d.", i * TRACK_WIDTH);
        mainArea.drawFastVLine(i * TRACK_WIDTH, 0, mainArea.height(), TFT_DARKGREY);
    }
    
    // Iterar por cada pista para dibujar sus botones y nombre
    for (int track = 0; track < 8; ++track) {
        uint16_t x = track * TRACK_WIDTH + BUTTON_SPACING / 2; // Posición X base para los elementos de la pista
        
        // Determinar el color de fondo del nombre de pista si está seleccionado
        uint16_t textBgColor = selectStates[track] ? TFT_SELECT_BG_COLOR : TFT_BG_COLOR;
        
        log_v("  drawMainArea(): Pista %d: X base=%d, textBgColor=%04X (Selected: %d).", track, x, textBgColor, selectStates[track]);

        // ======================== DIBUJO DE BOTONES ========================
        log_v("    drawMainArea(): Pista %d: Dibujando botones REC, SOLO, MUTE.", track);
        drawButton(mainArea, x, 5, BUTTON_WIDTH, BUTTON_HEIGHT, "REC", recStates[track], TFT_REC_COLOR);
        drawButton(mainArea, x, 5+BUTTON_HEIGHT+BUTTON_SPACING, BUTTON_WIDTH, BUTTON_HEIGHT, "SOLO", soloStates[track], TFT_SOLO_COLOR);
        drawButton(mainArea, x, 5+2*(BUTTON_HEIGHT+BUTTON_SPACING), BUTTON_WIDTH, BUTTON_HEIGHT, "MUTE", muteStates[track], TFT_MUTE_COLOR);
        
        // ======================== DIBUJO DE NOMBRE DE PISTA ========================
        mainArea.setTextColor(TFT_TEXT_COLOR, textBgColor);
        mainArea.setTextDatum(MC_DATUM); // Centrar el texto en su posición X e Y
        
        // La posición Y para el nombre de pista es justo debajo de los botones
        uint16_t nameY = 5 + 3 * (BUTTON_HEIGHT + BUTTON_SPACING) + 10;
        log_d("    drawMainArea(): Pista %d: Nombre '%s' en X=%d, Y=%d. rec:%d, solo:%d, mute:%d, sel:%d",
              track, trackNames[track].c_str(), track * TRACK_WIDTH + TRACK_WIDTH / 2, nameY, recStates[track], soloStates[track], muteStates[track], selectStates[track]);
        mainArea.drawString(trackNames[track].c_str(), track * TRACK_WIDTH + TRACK_WIDTH / 2, nameY);
    }
    
    // Empuja el sprite del área principal a la pantalla física, justo debajo del encabezado
    mainArea.pushSprite(0, HEADER_HEIGHT);
    log_v("drawMainArea(): mainArea sprite empujado a la pantalla en (0, %d).", HEADER_HEIGHT);

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
