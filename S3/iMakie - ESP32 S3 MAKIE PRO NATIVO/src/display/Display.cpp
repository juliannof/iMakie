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
  //delay(2000); // Espera para que te dé tiempo a abrir el monitor

  // Usamos log_v (Error) para forzar que se vea en consola con DEBUG_LEVEL=1
  log_v("=== 🔎 DIAGNÓSTICO DE MEMORIA ESP32-S3 ===");

  if (psramFound()) {
    // Usamos printf style que soportan los macros de log automáticamente
    log_v("✅ ÉXITO: PSRAM Detectada. Total: %u bytes (%.2f MB) | Libre: %u bytes", 
          ESP.getPsramSize(), 
          (float)ESP.getPsramSize()/1024.0f/1024.0f, 
          ESP.getFreePsram());
  } else {
    log_v("❌ ERROR CRÍTICO: PSRAM NO DETECTADA.");
    log_v("👉 Causas probables:");
    log_v("   1. build_flags incorrectos (falta -DBOARD_HAS_PSRAM)");
    log_v("   2. memory_type incorrecto (debe ser qio_opi para N16R8)");
    log_v("   3. Hardware dañado.");
  }
  
  tft.init();
  tft.initDMA(); // <--- OBLIGATORIO: Inicializa el canal DMA
  tft.setRotation(3  );
  tft.fillScreen(TFT_BG_COLOR);
  
  

  ledcAttach(TFT_BL,5000, 8);
  setScreenBrightness(screenBrightness);
  ledcWrite(TFT_BL, screenBrightness); // Máximo brillo
  

  mainArea.createSprite(MAIN_AREA_WIDTH, SCREEN_HEIGHT - HEADER_HEIGHT);
  header.createSprite(SCREEN_WIDTH, HEADER_HEIGHT);
  vuSprite.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT - HEADER_HEIGHT);  // 480 × 270

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


void updateDisplay() {
    
    log_v("updateDisplay() llamado. Estado de conexión: %d, needsTOTALRedraw: %d.", (int)logicConnectionState, needsTOTALRedraw);

    
    // --- LÓGICA DE REDIBUJO TOTAL (prioridad máxima) ---
    if (needsTOTALRedraw) {
        log_i("updateDisplay(): needsTOTALRedraw es TRUE. Realizando redibujo completo para el estado actual.");
        if (logicConnectionState == ConnectionState::CONNECTED) {
            drawHeaderSprite();
            drawMainArea();
            //drawVUMeters(); // <--- Llamada a VUMeters en redibujo total
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
            if (currentPage == 3) drawVUMeters(); // En PG3 los VUs son parte de la vista principal
            needsVUMetersRedraw = false;
            log_d("updateDisplay(): Redibujando VUMeters. currentPage=%d", currentPage);
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

void drawButton(TFT_eSprite &sprite,
                int x, int y, int w, int h,
                const char* label, bool active,
                uint16_t activeColor,
                uint16_t textColor,
                uint16_t inactiveColor)   // ← sin valor por defecto aquí
{
    uint16_t btnColor = active ? activeColor : inactiveColor;
    sprite.fillRoundRect(x, y, w, h, 8, btnColor);
    sprite.setTextColor(textColor, btnColor);
    sprite.setTextSize(1);
    sprite.setTextDatum(MC_DATUM);
    sprite.drawString(label, x + (w / 2), y + (h / 2));
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
        log_v("  drawMeter(): ERROR: Parámetros de altura o segmentos inválidos. h=%d, numSegments=%d, padding=%d.", h, numSegments, padding);
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


    uint16_t green_off  = VU_GREEN_OFF;
    uint16_t green_on   = VU_GREEN_ON;
    uint16_t yellow_off = VU_YELLOW_OFF;
    uint16_t yellow_on  = VU_YELLOW_ON;
    uint16_t red_off    = VU_RED_OFF;
    uint16_t red_on     = VU_RED_ON;
    uint16_t peak_color = VU_PEAK_COLOR;
    
    
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
    //vuSprite.fillSprite(TFT_BG_COLOR); 
    //vuSprite.fillSprite(TFT_WHITE); 

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
//  drawMainArea — Arquitectura con tabla de despacho
//  Añadir una página nueva = (1) escribir su función drawPageXxx()
//                            (2) añadir una línea en pageDrawTable[]
//  El resto del código NO se toca.
// *************************************************************************


// ─────────────────────────────────────────────────────────────────────────────
// PÁGINA 1 y 2 — Vista de Pads  (misma lógica, distintos datos)
// ─────────────────────────────────────────────────────────────────────────────

// Función auxiliar — atenúa un color RGB565 a ~25% de brillo
static uint16_t dimColor(uint16_t color) {
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5)  & 0x3F;
    uint8_t b =  color        & 0x1F;

    r = r / 4;  // 25% brillo
    g = g / 4;
    b = b / 4;

    return (r << 11) | (g << 5) | b;
}

// Función interna compartida por las dos páginas de pads
static void _drawPads(const char** labels, const byte* colors) {
    int cellW   = MAIN_AREA_WIDTH / 8;
    int cellH   = (SCREEN_HEIGHT - HEADER_HEIGHT) / 4;
    int btnSize = ((cellW < cellH) ? cellW : cellH) - 4;

    for (int i = 0; i < 32; i++) {
        int fila = i / 8;
        int col  = i % 8;

        // Centrar botón cuadrado en su celda
        int x = (col  * cellW) + (cellW  - btnSize) / 2;
        int y = (fila * cellH) + (cellH  - btnSize) / 2;

        // Paleta LED → color de pantalla
        uint16_t activeColor;
        switch (colors[i]) {
            case 1:  activeColor = TFT_RED;       break;
            case 2:  activeColor = TFT_GREEN;     break;
            case 3:  activeColor = 0x03FF;        break; // Azul vivo
            case 4:  activeColor = TFT_YELLOW;    break;
            case 5:  activeColor = TFT_CYAN;      break;
            case 6:  activeColor = TFT_MAGENTA;   break;
            case 7:  activeColor = TFT_WHITE;     break;
            default: activeColor = TFT_LIGHTGREY; break;
        }

        // Texto: amarillo en SHIFT; negro si el fondo activo es muy claro
        uint16_t txtColor = globalShiftPressed ? TFT_YELLOW : TFT_WHITE;
        if (btnState[i] && (activeColor == TFT_WHITE  ||
                            activeColor == TFT_YELLOW ||
                            activeColor == TFT_CYAN)) {
            txtColor = TFT_BLACK;
        }

        uint16_t inactiveColor = dimColor(activeColor); // Versión tenue del color del pad


        log_v("    _drawPads(): pad %d col=%d fila=%d x=%d y=%d state=%d", 
              i, col, fila, x, y, btnState[i]);

        drawButton(mainArea,
                   x, y,
                   btnSize, btnSize,
                   labels[i],
                   btnState[i],
                   activeColor,
                   txtColor,
                   inactiveColor);
    }
}

void drawPage1() {
    log_d("drawPage1(): PG1 shift=%d", globalShiftPressed);
    const char** labels = globalShiftPressed ? labels_PG1_SHIFT : labels_PG1;
    _drawPads(labels, LED_COLORS_PG1);
}

void drawPage2() {
    log_d("drawPage2(): PG2");
    _drawPads(labels_PG2, LED_COLORS_PG2);
}


// ─────────────────────────────────────────────────────────────────────────────
// PÁGINA 3 — Vista Mixer  (REC / SOLO / MUTE + nombre de pista)
// ─────────────────────────────────────────────────────────────────────────────

// ── drawPage3: cambiar uint16_t → int en variables locales ───────────────
void drawPage3() {
    log_v("drawPage3(): Mixer");

    //for (int i = 1; i < 8; ++i)
    //    mainArea.drawFastVLine(i * TRACK_WIDTH, 0, mainArea.height(), TFT_DARKGREY);

    for (int track = 0; track < 8; ++track) {
        int x            = track * TRACK_WIDTH + BUTTON_SPACING / 2;  // ← int
        uint16_t textBgColor = selectStates[track] ? TFT_SELECT_BG_COLOR : TFT_BG_COLOR;

        drawButton(mainArea, x, 5,
                   BUTTON_WIDTH, BUTTON_HEIGHT,
                   "REC",  recStates[track], TFT_REC_COLOR);

        drawButton(mainArea, x, 5 + BUTTON_HEIGHT + BUTTON_SPACING,
                   BUTTON_WIDTH, BUTTON_HEIGHT,
                   "SOLO", soloStates[track], TFT_SOLO_COLOR);

        drawButton(mainArea, x, 5 + 2 * (BUTTON_HEIGHT + BUTTON_SPACING),
                   BUTTON_WIDTH, BUTTON_HEIGHT,
                   "MUTE", muteStates[track], TFT_MUTE_COLOR);

        int nameY = 5 + 3 * (BUTTON_HEIGHT + BUTTON_SPACING) + 10;  // ← int
        mainArea.setTextColor(TFT_TEXT_COLOR, textBgColor);
        mainArea.setTextDatum(MC_DATUM);
        mainArea.drawString(trackNames[track].c_str(),
                            track * TRACK_WIDTH + TRACK_WIDTH / 2, nameY);
    }
    // VU Meters tienen su propio sprite y pushSprite — se empujan independientemente
    //drawVUMeters();
}


// ─────────────────────────────────────────────────────────────────────────────
// TABLA DE DESPACHO
// Para añadir una página nueva:
//   1. Escribe su función  void drawPageN() { ... }
//   2. Añade un puntero al final de pageDrawTable[]
//   NUM_PAGES se recalcula solo, la navegación se adapta automáticamente.
// ─────────────────────────────────────────────────────────────────────────────

typedef void (*PageDrawFn)();

static const PageDrawFn pageDrawTable[] = {
    nullptr,    // índice 0 — no se usa (las páginas empiezan en 1)
    drawPage1,  // currentPage == 1
    drawPage2,  // currentPage == 2
    drawPage3,  // currentPage == 3
    // drawPage4,  // ← futura página: solo añade la línea y su función
};

static const int NUM_PAGES = (sizeof(pageDrawTable) / sizeof(pageDrawTable[0])) - 1;


// ─────────────────────────────────────────────────────────────────────────────
// DISPATCHER PRINCIPAL — no necesita modificarse nunca más
// ─────────────────────────────────────────────────────────────────────────────

void drawMainArea() {
    log_v("drawMainArea(): currentPage=%d / NUM_PAGES=%d", currentPage, NUM_PAGES);

    if (currentPage < 1 || currentPage > NUM_PAGES) {
        log_v("drawMainArea(): currentPage=%d fuera de rango [1..%d]", currentPage, NUM_PAGES);
        return;
    }

    mainArea.fillSprite(TFT_BG_COLOR);
    pageDrawTable[currentPage]();
    mainArea.pushSprite(0, HEADER_HEIGHT);  // ← primero mainArea

    // VU Meters se empujan DESPUÉS, quedando encima de mainArea
    //if (currentPage == 3) drawVUMeters();

    log_v("drawMainArea(): sprite empujado en (0, %d).", HEADER_HEIGHT);
}

// ─────────────────────────────────────────────────────────────────────────────
// NAVEGACIÓN — tampoco necesita tocarse al añadir páginas
// ─────────────────────────────────────────────────────────────────────────────

void nextPage() {
    currentPage = (currentPage % NUM_PAGES) + 1;
    log_d("nextPage(): currentPage ahora = %d", currentPage);
    drawMainArea();
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
