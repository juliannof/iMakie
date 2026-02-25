// src/display/Display.cpp

#include "Display.h"
#include "../config.h" // Necesario para acceder a variables de estado y objetos TFT

// =================================================================================
//  ACCESO A tft Y SPRITES (DEFINIDOS GLOBALMENTE EN main.cpp)
//  Estas son las declaraciones 'extern' para que este archivo pueda usarlos.
// =================================================================================
extern TFT_eSPI tft;
extern TFT_eSprite header;
extern TFT_eSprite mainArea;
extern TFT_eSprite vuSprite;
extern TFT_eSprite vPotSprite;


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
void drawVPotDisplay(int8_t vpot_level); // <-- Prototipo CORRECTO



bool needsTOTALRedraw = true; // Forzar un redibujo completo al inicio
bool needsMainAreaRedraw = false;
bool needsHeaderRedraw = false;
bool needsVUMetersRedraw = false;
bool needsVPotRedraw = false;
// --- VARIABLES DE ESTADO GENERAL ---
volatile ConnectionState logicConnectionState = ConnectionState::CONNECTED; // CORREGIDO con ConnectionState::

String assignmentString = "CH-01 "; // <<<--- DEFINICIÓN DE String

// --- VARIABLES DE ESTADO DE CANAL ---
// Definir estos array aquí
String trackName = "Track99"; // <<<--- DEFINICIÓN

bool recStates = false;      // Para el estado de grabación de LA pista
bool soloStates = false;     // Para el estado de solo de LA pista
bool muteStates = false;     // Para el estado de mute de LA pista
bool selectStates = false;   // Para el estado de selección de LA pista
bool vuClipState = false;    // Para el estado de clip del VU de LA pista
float vuPeakLevels = 0.0f;   // Para el nivel de pico del VU de LA pista
float faderPositions = 0.0f; // Para la posición del fader de LA pista

float vuLevels = 0.0f; 

unsigned long vuLastUpdateTime = 0;      // Para el último tiempo de actualización del VU de LA pista
unsigned long vuPeakLastUpdateTime = 0;

static int8_t currentVPotValue = 3; // Valor inicial del vPot (simulado)



// ******************************************************
// iniciar
// ******************************************************

void initDisplay() {
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BG_COLOR);
  
  ledcSetup(TFT_BL_CHANNEL, 5000, 8);
  ledcAttachPin(TFT_BL, TFT_BL_CHANNEL);
  setScreenBrightness(screenBrightness);

  mainArea.createSprite(MAINAREA_WIDTH, MAINAREA_HEIGHT); // <<<-- USA LAS NUEVAS DIMENSIONES
  header.createSprite(TFT_WIDTH, HEADER_HEIGHT);
  vuSprite.createSprite(TFT_WIDTH-MAINAREA_WIDTH, MAINAREA_HEIGHT);
  vPotSprite.createSprite(TFT_WIDTH,  V_POT_HEIGHT );
  Serial.println("[SETUP] Módulo de Display iniciado.");
}

void setScreenBrightness(uint8_t brightness) {
  screenBrightness = brightness;
  ledcWrite(TFT_BL_CHANNEL, screenBrightness);
}

// *************************************************************************
// Pantalla de Espera cuando no hay conexión
// *************************************************************************

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



// *************************************************************************
// NUEVA FUNCIÓN: Dibuja una pantalla de "Sincronizando MIDI" para el estado MIDI_HANDSHAKE_COMPLETE
// Se usa para indicar que el handshake MIDI ha finalizado pero se espera información vital del DAW.
// *************************************************************************
void drawInitializingScreen() {
    Serial.println("drawInitializingScreen(): Dibujando pantalla de inicialización del DAW.");
    
    // Limpia el fondo (o dibuja un fondo parcial si quieres mantener algo de la MainArea visible)
    tft.fillScreen(TFT_BLUE); 

    // Mensaje de estado principal
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString("MIDI Handshake OK!", tft.width() / 2, tft.height() / 2 - 30);
    
    // Mensaje adicional de espera o progreso
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("Esperando datos de proyecto del DAW...", tft.width() / 2, tft.height() / 2 + 10);
    
    Serial.println("drawInitializingScreen(): Pantalla de inicialización dibujada.");
}

// =========================================================================
//  IMPLEMENTACIÓN DE updateDisplay()
//  (Esta función se encargará de TUS necesidades de actualización inteligente de pantalla)
// =========================================================================
void updateDisplay() {
    // Primero, llamamos a handleDisplaySynchronization() si hay lógica de tiempo/sincronización
    //handleDisplaySynchronization(); // Asegúrate de que esta función está implementada en Display.cpp

    // Lógica para el manejo de estados de conexión (DISCONNECTED, INITIALIZING, CONNECTED)
    // Esto se usa para mostrar pantallas de estado o la interfaz principal.
    switch (logicConnectionState) {
        case ConnectionState::DISCONNECTED:
            // Solo dibuja la pantalla de desconexión una vez por estado (o si se fuerza redraw)
            static bool offline_screen_drawn = false;
            if (!offline_screen_drawn || needsTOTALRedraw) {
                drawOfflineScreen(); // Asumo que esta función existe en Display.cpp. Hacer pushSprite si es a tft
                offline_screen_drawn = true;
                needsTOTALRedraw = false; // El TOTALRedraw ya se aplicó aquí.
            }
            return; // No procesar más si está desconectado.


        case ConnectionState::CONNECTED:
            // Si acabamos de entrar en el estado CONNECTED, o si se ha pedido un redibujo total
            // Vuelve a dibujar todos los elementos estáticos y dinámicos para asegurar la coherencia.
            // La variable `connected_screen_init` asegura que esto pase solo una vez al entrar en CONNECTED.
            static bool connected_screen_init = false;
            if (!connected_screen_init || needsTOTALRedraw) {
                drawHeaderSprite(); // Tu función de dibujo para el encabezado (hace su propio pushSprite)
                drawMainArea();     // Tu función de dibujo para el área principal (hace su propio pushSprite)
                drawVUMeters();  // Llamar a esta función si quieres un dibujo inicial de los VUs (hace su propio pushSprite)
                drawVPotDisplay(currentVPotValue); // <-- Corregido para pasar el valor global del vPot

                // Después de dibujar todo el contenido inicial, reseteamos las banderas.
                needsTOTALRedraw = false;
                needsHeaderRedraw = false;
                needsMainAreaRedraw = false;
                needsVUMetersRedraw = false;
                needsVPotRedraw = false;
                connected_screen_init = true;
                // NOTA: Las funciones drawXXSprite() y drawVPotDisplay() deben hacer su propio pushSprite().
            }


            // =====================================================================
            //     Procesamiento de Banderas de Redibujo (Incremental)
            //     Solo empuja los sprites que realmente cambiaron.
            // =====================================================================
            if (needsHeaderRedraw) {
                drawHeaderSprite(); // Esta función debe hacer su propio pushSprite()
                needsHeaderRedraw = false;
            }
            if (needsMainAreaRedraw) {
                drawMainArea(); // Esta función debe hacer su propio pushSprite()
                needsMainAreaRedraw = false;
            }
            if (needsVUMetersRedraw) {
                drawVUMeters();
                Serial.println("drawVUMeters");
                // updateMeter ya lo hace si se llama. Si no, debería llamarse aquí.
                // updateMeter(currentPeak_sim, clipIndicator_sim && clipBlinkState_sim);
                // vuSprite.pushSprite(VU_METERS_X_POS, VU_METERS_Y_POS);
                needsVUMetersRedraw = false;
            }
            if (needsVPotRedraw) {
                // updateVPot ya lo hace si se llama. Si no, debería llamarse aquí.
                // updateVPotDisplay(vPotValue_sim);
                // vPotSprite.pushSprite(VPOT_READOUT_X_POS, VPOT_READOUT_Y_POS);
                needsVPotRedraw = false;
            }
            break; // Salir del switch 'CONNECTED'
    } // Fin del switch (logicConnectionState)
}





// *************************************************************************
// Cabecera con Timecode y Asignación
// *************************************************************************

void drawHeaderSprite() {
    // Limpiamos el sprite de la cabecera con el color de fondo
    header.fillSprite(TFT_BLUE);

    // --- DIBUJAR CÓDIGO DE TIEMPO (10 dígitos) ---
    header.setTextDatum(MC_DATUM); // Alinear el texto en el centro (horizontal y vertical)
    header.setTextColor(TFT_WHITE); // Color blanco estándar
    
    // Podemos usar una fuente monoespaciada para que los dígitos no "bailen"
    // Asegúrate de haber cargado esta fuente o tenerla disponible en tu user_setup.
    header.setFreeFont(&FreeMonoBold12pt7b); // Ejemplo de fuente, cámbiala por la tuya



    header.drawString("Track 1", TFT_WIDTH/2, HEADER_HEIGHT / 2);

    // Empujar el sprite actualizado a la pantalla TFT
    header.pushSprite(0, 0); // En la posición Y = 0 (arriba del todo)
}

// *************************************************************************
// Área Principal con los botones y nombres de pista
// *************************************************************************

void drawMainArea() {
    Serial.println("drawMainArea(): Dibujando área principal (botones y nombres de pista).");
    mainArea.fillSprite(TFT_BG_COLOR); // Ok, limpia el sprite
    
    #ifdef LOAD_GFXFF
        // "Audio1" con fuente grande de ~40px
        mainArea.setFreeFont(&FreeSans24pt7b); // CAMBIADO A mainArea
        mainArea.setCursor(3, 115);           // Posición relativa al sprite mainArea, NO GLOBAL
        //mainArea.print("Audio1");
        mainArea.print(trackName);
        
        // dB con fuente mediana (~32px)
        mainArea.setFreeFont(&FreeSans18pt7b);
        mainArea.setCursor(3, 150);
        mainArea.print("-60 dB");
    #endif
    

    // Esto se mantendría al final
    mainArea.pushSprite(0, HEADER_HEIGHT);
    }




// *************************************************************************
// Dibuja un VU Meter con indicador de pico
// *************************************************************************
void drawMeter(TFT_eSprite &sprite, uint16_t x, uint16_t y, uint16_t w, uint16_t h, float level, float peakLevel, bool isClipping) {
    log_d("  drawMeter(): Invocado para VU en sprite_x=%d, sprite_y=%d, w=%d, h=%d. Level=%.3f, Peak=%.3f, Clipping=%d.",
          x, y, w, h, level, peakLevel, isClipping);

    const int numSegments = 12; // Número total de segmentos del medidor
    const int padding = 1;      // Espacio (en píxeles) entre cada segmento
    
    // Calcular la altura de cada segmento, distribuyendo el espacio restante uniformemente
    if (numSegments <= 1 || h <= (padding * (numSegments - 1))) { // Evitar división por cero o segmentos negativos
        log_e("  drawMeter(): ERROR: Parámetros de altura o segmentos inválidos. h=%d, numSegments=%d, padding=%d.", h, numSegments, padding);
        return; 
    }
    const int segmentHeight = (h - padding * (numSegments - 1)) / numSegments;
    

    // Calcular la cantidad de segmentos que deben estar "activos" según el nivel instantáneo
    size_t activeSegments = round(level * numSegments); // Convertir nivel flotante a número de segmentos activos
    // Calcular el índice del segmento de pico (0-11)
    size_t peakSegment_idx = round(peakLevel * numSegments); // Convertido a número de segmento (1-12)
    

    // Por seguridad, el pico no puede estar por debajo del nivel actual.
    // Ajustamos peakSegment_idx para que sea un índice, y para que nunca sea < activeSegments
    if (peakSegment_idx > 0) peakSegment_idx--; // Convertir cantidad (1-12) a índice (0-11)
    
    if (activeSegments > 0 && peakSegment_idx < activeSegments - 1) { // Si hay nivel y el pico es menor que el último segmento activo
        peakSegment_idx = activeSegments - 1; // El pico se ajusta al último segmento activo
    } else if (activeSegments == 0 && peakSegment_idx != (size_t)-1) { // Si no hay nivel, no debería haber pico
        peakSegment_idx = (size_t)-1; // -1 como marcador para no visible
    }


    // Colores para los segmentos del vúmetro
    uint16_t green_off = tft.color565(0, 20, 0);       // Verde oscuro para segmentos inactivos
    uint16_t green_on  = TFT_GREEN;                    // Verde brillante para segmentos activos
    uint16_t yellow_off = tft.color565(20, 20, 0);     // Amarillo oscuro
    uint16_t yellow_on  = TFT_YELLOW;                   // Amarillo brillante
    uint16_t red_off   = tft.color565(20, 0, 0);       // Rojo oscuro
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
        } else if (static_cast<size_t>(i) < activeSegments) { // Si este segmento está por debajo (o en) el nivel actual
            if (i < 8) { selectedColor = green_on;}           // Primeros 8 segmentos (verde)
            else if (i < 10) { selectedColor = yellow_on;}    // Siguientes 2 segmentos (amarillo)
            else { selectedColor = red_on;}                  // Últimos 2 segmentos (rojo)
        } else { // Si este segmento está por encima del nivel activo (fondo)
            if (i < 8) { selectedColor = green_off;}         // Primeros 8 segmentos (verde oscuro)
            else if (i < 10) { selectedColor = yellow_off;}   // Siguientes 2 segmentos (amarillo oscuro)
            else { selectedColor = red_off;}                 // Últimos 2 segmentos (rojo oscuro)
        }

        // --- LÓGICA DE CLIPPING (sobrescribe cualquier otro color para el último segmento) ---
        // Si el estado de clip está activo, el segmento más alto SIEMPRE es rojo brillante.
        if (static_cast<size_t>(i) == (numSegments - 1) && isClipping) {
            selectedColor = red_on;
            log_i("  drawMeter(): Track CLIP activo para Segmento %d (y=%d), forzando ROJO.", i, segY);
        }
        
        // Dibujar el rectángulo del segmento
        sprite.fillRect(x, segY, w, segmentHeight, selectedColor);
        Serial.println("Dibujo terminado");
    }
}



// *************************************************************************
// VU Meters (Dibuja todos los medidores en el sprite `vuSprite`)
// *************************************************************************
void drawVUMeters() {
    Serial.println("drawVUMeters() llamado. Limpiando vuSprite.");
    // Usamos TFT_BG_COLOR (negro) para que el fondo se vea cuando los VUs de reposo se dibujen oscuros.
    vuSprite.fillSprite(TFT_BG_COLOR); 
   //vuSprite.fillSprite(TFT_GREEN); 

    int track = 0;
        // --- NO HAY OPTIMIZACIÓN AQUÍ. drawMeter() se llama SIEMPRE para TODOS los tracks. ---
        // Esto asegura que el vúmetro esté siempre visible, mostrando su estado de reposo o activo.

        uint16_t baseX = track * MAINAREA_HEIGHT;
        uint16_t meter_x = 0; // X relativo al sprite del VU
        uint16_t meter_y = 3;                              // Y relativo al sprite del VU
        uint16_t meter_w = 40;
        uint16_t meter_h = MAINAREA_HEIGHT-3;

        // Logs que muestran los niveles para cada track antes de dibujar, independientemente de la actividad.
        
        // --- LLAMADA a drawMeter (SIEMPRE para TODOS los tracks) ---
        drawMeter(
            vuSprite,           // Argumento 1: el sprite
            meter_x,            // Argumento 2: posición X
            meter_y,            // Argumento 3: posición Y
            meter_w,            // Argumento 4: ancho (width)
            meter_h,            // Argumento 5: alto (height)
            vuLevels,    // Argumento 6: nivel instantáneo
            vuPeakLevels,// Argumento 7: nivel de pico
            vuClipState  // Argumento 8: estado de clip
        );
    
    
    // Empuja el sprite de vúmetros a la pantalla física.
    // Esto asegura que toda el área de los VUs se actualiza en la pantalla física.
    vuSprite.pushSprite(MAINAREA_WIDTH, HEADER_HEIGHT);
    Serial.println("drawVUMeters");
}


// --- Definición de la función drawVPotDisplay ---
// IMPORTANTE: Si esta función va a ser una función GLOBAL, tienes que asegurarte
// de que las variables globales tft y vPotSprite estén accesibles y correctamente inicializadas.
// Si vPotSprite es global, DEBE haber sido creado (.createSprite(...)) antes de ser usado.
void drawVPotDisplay(int8_t vpot_level) { // ¡Aquí 'int8_t' era lo que faltaba!
    // 1. Limpiar el sprite del vPot para el nuevo dibujo
    // Usamos TFT_BLACK como color de fondo para limpiar sin dejar artefactos.
    vPotSprite.fillSprite(TFT_BG_COLOR); 
    
    // 2. Definir las propiedades visuales de los segmentos de la barra
    const int totalSegments = 7;     
    const int centerWidth = 20;      
    const int gapVPot = 2;           

    // 3. Calcular dimensiones y posiciones para centrar la barra de segmentos dentro del sprite
    //    Usamos vPotSprite.width() y vPotSprite.height() para los cálculos.
    int availableWidthForSegments = vPotSprite.width() - centerWidth - (2 * totalSegments * gapVPot);
    int segmentWidthVPot = availableWidthForSegments / (2 * totalSegments); 
    int actualDrawingWidth = (2 * totalSegments * segmentWidthVPot) + (2 * totalSegments * gapVPot) + centerWidth;
    int startX_in_sprite = (vPotSprite.width() - actualDrawingWidth) / 2;

    int segment_y = 2; 
    int segment_height = vPotSprite.height() - 4; 

    // 4. Dibujar los SEGMENTOS NEGATIVOS (lado izquierdo de la barra)
    for (int i = 0; i < totalSegments; i++) {
        int x = startX_in_sprite + (i * (segmentWidthVPot + gapVPot));
        bool active = (vpot_level < 0 && i >= (totalSegments + vpot_level));
        
        uint16_t color = active ? TFT_MCU_BLUE : TFT_MCU_DARKGRAY;
        vPotSprite.fillRect(x, segment_y, segmentWidthVPot, segment_height, color);
    }

    // 5. Dibujar el SEGMENTO CENTRAL (posición 0), que siempre está activo.
    int centerX_in_sprite = startX_in_sprite + (totalSegments * (segmentWidthVPot + gapVPot));
    vPotSprite.fillRect(centerX_in_sprite, segment_y, centerWidth, segment_height, TFT_MCU_BLUE); 

    // 6. Dibujar los SEGMENTOS POSITIVOS (lado derecho de la barra)
    for (int i = 0; i < totalSegments; i++) {
        int x = centerX_in_sprite + centerWidth + gapVPot + (i * (segmentWidthVPot + gapVPot));
        bool active = (vpot_level > 0 && i < vpot_level);
        
        uint16_t color = active ? TFT_MCU_BLUE : TFT_MCU_DARKGRAY;
        vPotSprite.fillRect(x, segment_y, segmentWidthVPot, segment_height, color);
    }
    
    // 7. Empujar el sprite del vPot (con la barra dibujada) a la pantalla principal.
    // Usamos las constantes V_POT_DRAW_X y V_POT_DRAW_Y que definiste para la posición final.
    vPotSprite.pushSprite(0, MAINAREA_HEIGHT+HEADER_HEIGHT); 
    
    log_v("vPot horizontal bar rendered with level: %d", vpot_level);
}
// Fin del codigo
