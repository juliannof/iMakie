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
TrellisCallback blink(keyEvent evt);
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
    trellis.registerCallback(i, blink);
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
    
    for (uint8_t i = 0; i < X_DIM; i++) {
        // Fila 0: REC (Rojo) - Sin cambios
        trellis.setPixelColor(i, recStates[i] ? applyBrightness(baseColors[0]) : 0);
        
        // Fila 1: SOLO (Amarillo) - Sin cambios
        trellis.setPixelColor(i + X_DIM, soloStates[i] ? applyBrightness(baseColors[1]) : 0);
        
        // --- SECCIÓN DE MUTE MODIFICADA ---
        trellis.setPixelColor(i + 2 * X_DIM, muteStates[i] ? applyBrightness(baseColors[2]) : 0);
        
        // Fila 3: SELECT (Blanco) - Sin cambios
        trellis.setPixelColor(i + 3 * X_DIM, selectStates[i] ? applyBrightness(baseColors[3]) : 0);
    }
    
    // Actualizar el hardware una sola vez - Sin cambios
    trellis.show();
}

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
TrellisCallback blink(keyEvent evt) {
    uint8_t row = evt.bit.NUM / X_DIM;
    uint8_t col = evt.bit.NUM % X_DIM;
    byte note_base;

    switch(row) {
      case 0: note_base = 0;  break;  // REC
      case 1: note_base = 8;  break;  // SOLO
      case 2: note_base = 16; break;  // MUTE
      case 3: note_base = 24; break;  // SELECT
      default: return 0;
    }
    
    if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
        // AL PULSAR -> NOTE ON
        byte midi_msg[3] = {0x90, (byte)(note_base + col), 127};
        sendToPico(midi_msg, 3);
        
        // La lógica de exclusión de SELECT va aquí
        if (row == 3) {
            for(int i = 0; i < 8; ++i) {
                if(selectStates[i] && i != col) {
                    // Envía un Note Off explícito para los otros botones de selección
                    byte off_msg[] = {0x80, (byte)(24 + i), 0};
                    sendToPico(off_msg, 3);
                }
            }
        }
        
    } else if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_FALLING) {
        // AL SOLTAR -> NOTE OFF
        // El protocolo Mackie es momentáneo para la mayoría de los botones.
        // El DAW se encarga de 'enganchar' (latch) el estado.
        byte midi_msg[3] = {0x80, (byte)(note_base + col), 0};
        sendToPico(midi_msg, 3);
    }
    
    return 0;
}

// Fin del codigo