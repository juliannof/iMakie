// src/hardware/Hardware.cpp

#include "Hardware.h"                      // Incluye su propia cabecera
#include "../config.h"                     // Para acceder a los objetos (trellis) y estados (recStates, etc.)
#include "../comUA/UARTHandler.h"  // Para poder llamar a sendToPico

// --- VARIABLES "PRIVADAS" DE ESTE MÓDULO ---
namespace {
    uint8_t trellisBrightness = 15; // Valor de brillo por defecto
    const uint32_t baseColors[4] = {0xFF0000, 0xFFFF00, 0xFF0000, 0xFFFFFF};
}

// --- PROTOTIPO DE LA FUNCIÓN DE CALLBACK (necesario porque initHardware la usa) ---
TrellisCallback onTrellisEvent(keyEvent evt); 
uint32_t applyBrightness(uint32_t color);

// ====================================================================
// --- IMPLEMENTACIÓN DE FUNCIONES PÚBLICAS ---
// ====================================================================

void initHardware() {
  log_d("Iniciando NeoTrellis...");
  if (!trellis.begin()) {
    log_e("!!! ERROR FATAL: NeoTrellis no encontrado. Comprueba I2C.");
    pinMode(LED_BUILTIN, OUTPUT);
    while (1) { digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); delay(100); }
  }
  log_d("        -> Trellis.begin() exitoso.");

  log_d("[SETUP] Registrando callbacks...");
  for (int i = 0; i < X_DIM * Y_DIM; i++) {
    trellis.activateKey(i, SEESAW_KEYPAD_EDGE_RISING);
    trellis.activateKey(i, SEESAW_KEYPAD_EDGE_FALLING); // Asegúrate de escuchar ambos flancos
    trellis.registerCallback(i, onTrellisEvent); 
  }
  log_d("        -> Callbacks registrados.");
  
  log_i("Encendiendo LEDs en Azul Claro...");

  // Color R, G, B (Ice Blue / Cyan claro)
  // R=50, G=180, B=255
  // Ajusta valores (0-255) para afinar el tono
  // Manual way (R, G, B)
  //uint32_t colorAzulClaro = (50 << 16) | (180 << 8) | 255;
  
  uint32_t colorAzulClaro = seesaw_NeoPixel::Color(0, 0, 1);
  
  for (int i = 0; i < X_DIM * Y_DIM; i++) {
    trellis.setPixelColor(i, colorAzulClaro);
  }
  
  trellis.show();
  
  log_d("[SETUP] Módulo de Hardware iniciado y verificado.");
}

void handleHardwareInputs() {
  // Esta función se llama en cada loop para leer el estado del Trellis.
  //log_e("procesando inputs de neotrellis...");

  trellis.read();
}

// src/hardware/Hardware.cpp (ubicada en su archivo .cpp)

// ****************************************************************************
// Lógica de decaimiento de los vúmetros y retención de picos
// ****************************************************************************
void handleVUMeterDecay() {
    log_v("handleVUMeterDecay() llamado.");

    const unsigned long DECAY_INTERVAL_MS = 100;    // Frecuencia de decaimiento del nivel normal
    const unsigned long PEAK_HOLD_TIME_MS = 2000;   // Tiempo que el pico se mantiene visible
    const float DECAY_AMOUNT = 1.0f / 12.0f;        // Cantidad de decaimiento (aproximadamente 1 segmento)
    
    unsigned long currentTime = millis(); // Obtener el tiempo actual una sola vez
    bool anyVUMeterChanged = false; // Flag para saber si algún VU cambió por decaimiento

    for (int i = 0; i < 8; i++) {
        // --- 1. Decaimiento del Nivel Normal (RMS/Instantáneo) ---
        if (vuLevels[i] > 0) {
            // Si el tiempo desde la última actualización excede el intervalo de decaimiento
            if (currentTime - vuLastUpdateTime[i] > DECAY_INTERVAL_MS) {
                float oldLevel = vuLevels[i];
                vuLevels[i] -= DECAY_AMOUNT; // Reducir el nivel
                if (vuLevels[i] < 0.01f) { // Evitar números negativos y asegurar que llega a cero
                    vuLevels[i] = 0.0f;
                }
                vuLastUpdateTime[i] = currentTime; // Actualizar el tiempo de la última caída
                anyVUMeterChanged = true; // Indicar que hubo un cambio
                log_v("  Track %d: VU Level decayed from %.3f to %.3f.", i, oldLevel, vuLevels[i]);
            }
        }
        
        // --- 2. Decaimiento/Reinicia de Nivel de Pico (Peak Hold) ---
        if (vuPeakLevels[i] > 0) {
            // Si ha pasado el tiempo de retención del pico
            if (currentTime - vuPeakLastUpdateTime[i] > PEAK_HOLD_TIME_MS) {
                // El pico "salta" hacia abajo hasta el nivel normal actual.
                if (vuPeakLevels[i] > vuLevels[i]) { // Solo si el pico aún está por encima del nivel actual
                    float oldPeakLevel = vuPeakLevels[i];
                    vuPeakLevels[i] = vuLevels[i]; // Igualar el pico al nivel actual
                    anyVUMeterChanged = true; // Indicar que hubo un cambio
                    log_v("  Track %d: VU Peak decayed from %.3f to %.3f (jump to current level).", i, oldPeakLevel, vuPeakLevels[i]);
                }
            }
        }

        // --- 3. Lógica de Seguridad para el Pico ---
        // Asegurarse de que el pico nunca esté por debajo del nivel actual.
        // Si el nivel instantáneo sube por encima del pico, el pico debe igualarlo.
        if (vuPeakLevels[i] < vuLevels[i]) {
            log_w("  Track %d: VU Peak (%.3f) era menor que VU Level (%.3f). Corrigiendo.", i, vuPeakLevels[i], vuLevels[i]);
            vuPeakLevels[i] = vuLevels[i];
            vuPeakLastUpdateTime[i] = currentTime; // Reiniciar el temporizador de retención del pico
            anyVUMeterChanged = true; // Indicar que hubo un cambio
        }
    }

    // Si cualquier vúmetro cambió, activar el flag de redibujo específico para vúmetros
    if (anyVUMeterChanged) {
        needsVUMetersRedraw = true; // <--- Usamos el nuevo flag específico para VUMeters
        log_v("handleVUMeterDecay(): anyVUMeterChanged es TRUE. needsVUMetersRedraw = true.");
    }
}

// ****************************************************************************
// Update Leds
// ****************************************************************************

void updateLeds() {
    // 1. Elegir mapa de colores según página
    const byte* colorIndexMap = (currentPage == 1) ? LED_COLORS_PG1 : LED_COLORS_PG2;

    for (int i = 0; i < 32; i++) {
        uint32_t colorFinal = 0;
        
        // Obtener el color base de la paleta
        uint32_t baseColor = PALETTE[colorIndexMap[i]];

        // A. ESTADO ACTIVO (Si el DAW dice ON)
        if (btnState[i]) {
            if (baseColor == C_OFF) baseColor = C_WHITE; 
            colorFinal = baseColor; // Brillo máximo
        }
        // B. ESTADO REPOSO
        else {
            colorFinal = applyBrightness(baseColor); // Atenuado
        }

        // C. OVERRIDES LOCALES (Shift y Pagina)
        if (i == 26 && globalShiftPressed) colorFinal = C_YELLOW; 
        if (i == 31) colorFinal = (currentPage == 1) ? applyBrightness(C_BLUE) : C_RED;

        trellis.setPixelColor(i, colorFinal);
    }
    
    trellis.show();
}


// ****************************************************************************
// Set Trellis Brightness
// ****************************************************************************

void setTrellisBrightness(uint8_t newBrightness) {
  trellisBrightness = newBrightness;
  log_d("[HARDWARE] Brillo de Trellis ajustado a: %d\n", newBrightness);
  // Al cambiar el brillo, forzamos una actualización de los LEDs para verlo al instante
  updateLeds();
}


// ****************************************************************************
// Reset to StandBy: Reinicia el sistema a un estado de espera (Standby)
// Limpia todos los estados visuales y de control del dispositivo.
// ****************************************************************************
void resetToStandbyState() {
    log_i("resetToStandbyState(): Reseteando al estado de espera (Standby)...");

    bool stateChanged = false; // Usamos este flag local para ver si hubo cambios en los estados.

    // 1. Resetear el estado lógico interno de todas las pistas y vúmetros
    for (int i = 0; i < 8; i++) {
        // Vúmetros
        if (vuLevels[i] != 0.0f) { vuLevels[i] = 0.0f; stateChanged = true; }
        if (vuPeakLevels[i] != 0.0f) { vuPeakLevels[i] = 0.0f; stateChanged = true; }
        if (vuClipState[i]) { vuClipState[i] = false; stateChanged = true; }
        
        // Botones (REC, SOLO, MUTE, SELECT)
        if (recStates[i]) { recStates[i] = false; stateChanged = true; }
        if (soloStates[i]) { soloStates[i] = false; stateChanged = true; }
        if (muteStates[i]) { muteStates[i] = false; stateChanged = true; }
        if (selectStates[i]) { selectStates[i] = false; stateChanged = true; }
        
        // Nombres de pista
        if (trackNames[i] != "") { trackNames[i] = ""; stateChanged = true; }

        // Faders (si ya tenemos faderPositions)
        if (faderPositions[i] != 0.0f) { faderPositions[i] = 0.0f; stateChanged = true; }
    }

    // Resetear también el string de asignación y el tiempo/tempo si no se resetean automáticamente
    if (assignmentString != "--") { assignmentString = "--"; stateChanged = true; }
    // Asumiendo que tempoString, timeCodeChars_clean, beatsChars_clean se actualizan por MIDI de DAW.
    // Si queremos limpiarlos, lo haríamos aquí:
    // snprintf(tempoString, sizeof(tempoString), "0.00 BPM"); stateChanged = true;
    // memset(timeCodeChars_clean, ' ', 12); timeCodeChars_clean[12] = '\0'; stateChanged = true;
    // memset(beatsChars_clean, ' ', 12); beatsChars_clean[12] = '\0'; stateChanged = true;

    // 2. Si hubo algún cambio en el estado, forzamos una actualización de los LEDs y la pantalla.
    if (stateChanged) {
        log_d("resetToStandbyState(): Se detectaron cambios de estado. Actualizando interfaz.");
        updateLeds(); // Apaga los LEDs o los pone en un estado base (asumiendo que updateLeds usa los estados (recStates etc.))
        
        // Activar banderas de redibujo específicas y total
        needsTOTALRedraw = true;       // Forzar redibujo completo (pantalla offline / de inicio)
        needsMainAreaRedraw = true;    // MainArea (botones, nombres vacíos)
        needsHeaderRedraw = true;      // Header (tiempo, tempo, asignación vacíos)
        needsVUMetersRedraw = true;    // VUMeters (a cero)
    } else {
        log_d("resetToStandbyState(): No se detectaron cambios de estado. Asegurando LEDs y redibujo total.");
        updateLeds(); // Asegurarse de que los LEDs estén en estado base.
        needsTOTALRedraw = true; // Forzar redibujo de la pantalla de espera, por si acaso.
    }
}

// ====================================================================
// --- Manejo de respuesta de Neotrellis ---
// ====================================================================

uint32_t applyBrightness(uint32_t color) {
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  r = (r * trellisBrightness) / 255;
  g = (g * trellisBrightness) / 255;
  b = (b * trellisBrightness) / 255;
  return (uint32_t)(r << 16) | (uint32_t)(g << 8) | b;
}



// ---------------------------------------------------------
// NUEVO CALLBACK MAESTRO (Soporta Páginas y Mapas)
// ---------------------------------------------------------
TrellisCallback onTrellisEvent(keyEvent evt) {
    int key = evt.bit.NUM; // Índice del botón (0 a 31)
    bool isPress = (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING);

    // ============================================================
    // 1. GESTIÓN LOCAL (Botones que cambian el comportamiento)
    // ============================================================

    // BOTÓN DE PÁGINA (Índice 31: Esquina inferior derecha)
    // Cambia entre "Mixer Mode" y "Function Mode"
    if (key == 31 && isPress) {
        currentPage = (currentPage == 1) ? 2 : 1;
        
        log_i("Hardware: Cambio a Página %d", currentPage);
        updateLeds();          // Cambia los colores de la rejilla física
        needsMainAreaRedraw = true; // Avisa al loop para repintar la pantalla TFT
        
        return 0; // Este botón NO envía MIDI a la Pico
    }

    // BOTÓN SHIFT (Índice 26: Fila 4, Columna 3)
    if (key == 26) {
        globalShiftPressed = isPress; // True mientras mantengas pulsado
        
        // Si el estado cambia, redibujamos la pantalla (textos amarillos)
        if (evt.bit.EDGE != SEESAW_KEYPAD_EDGE_HIGH && evt.bit.EDGE != SEESAW_KEYPAD_EDGE_LOW) {
             needsMainAreaRedraw = true;
             updateLeds(); // Para poner el botón en Amarillo
        }
        
        // OJO: Además de ser local, SHIFT suele enviar nota MIDI (0x46)
        // Dejamos que el código de abajo envíe la nota también.
    }

    // ============================================================
    // 2. BUSCAR QUÉ NOTA MIDI TOCA ENVIAR
    // ============================================================
    
    // Elegimos el mapa de notas según la página en la que estemos
    const byte* currentMap = (currentPage == 1) ? MIDI_NOTES_PG1 : MIDI_NOTES_PG2;
    byte noteToSend = currentMap[key];

    // ============================================================
    // 3. ENVIAR A LA PICO (SI HAY NOTA ASIGNADA)
    // ============================================================
    if (noteToSend != 0x00) {
        
        byte midiMsg[3];
        midiMsg[0] = isPress ? 0x90 : 0x90; // Mackie usa Note On (90) para pulsar y soltar
        midiMsg[1] = noteToSend;
        midiMsg[2] = isPress ? 127 : 0;     // Vel 127 = Pulsar, Vel 0 = Soltar

        sendToPico(midiMsg, 3);

        // --- FEEDBACK VISUAL INSTANTÁNEO ---
        if (isPress) {
            // Flash Blanco al pulsar para sentir que has tocado
            trellis.setPixelColor(key, 0xFFFFFF); 
            trellis.show();
        } else {
            // Al soltar, volvemos a calcular el color correcto
            // (Si Logic ya contestó, se quedará encendido. Si no, se apagará/atenuará)
            updateLeds(); 
        }
    }

    return 0;
}


// Fin del codigo