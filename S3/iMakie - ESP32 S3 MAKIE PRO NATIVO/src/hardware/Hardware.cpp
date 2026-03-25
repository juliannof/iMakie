// src/hardware/Hardware.cpp

#include "Hardware.h"                      // Incluye su propia cabecera
#include "../config.h"                     // Para acceder a los objetos (trellis) y estados (recStates, etc.)
#include "../midi/MIDIProcessor.h"     // ← AÑADIR


// --- VARIABLES "PRIVADAS" DE ESTE MÓDULO ---
namespace {
    uint8_t trellisBrightness = 5; // Valor de brillo por defecto
}

// --- PROTOTIPO DE LA FUNCIÓN DE CALLBACK (necesario porque initHardware la usa) ---
TrellisCallback onTrellisEvent(keyEvent evt); 
uint32_t applyBrightness(uint32_t color);
uint32_t applyMidBrightness(uint32_t color);  // ← añadir

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
  
  uint32_t colorAzulClaro = seesaw_NeoPixel::Color(0, 0, 200);
  
  for (int i = 0; i < X_DIM * Y_DIM; i++) {
    trellis.setPixelColor(i, colorAzulClaro);
  }
  
  trellis.show();

  digitalWrite(LED_BUILTIN, LOW);
  
  log_d("[SETUP] Módulo de Hardware iniciado y verificado.");
}

void handleHardwareInputs() {
  // Esta función se llama en cada loop para leer el estado del Trellis.
  //log_e("procesando inputs de neotrellis...");

  trellis.read();
}


static CalibrateRequestCallback _onCalibrateRequest = nullptr;

void registerCalibrateRequestCallback(CalibrateRequestCallback cb) {
    _onCalibrateRequest = cb;
}



// ****************************************************************************
// Update Leds
// ****************************************************************************

void updateLeds() {
    // --- STANDBY: todos los LEDs apagados excepto indicador de página ---
    if (!isLogicConnected()) {
        for (int i = 0; i < 32; i++) {
            trellis.setPixelColor(i, 0x000000); // apagado
        }
        // Solo botón de página activo, tenue, para orientar al usuario
        if (31 < 32) {
            uint32_t pageColor;
            if      (currentPage == 1) pageColor = applyBrightness(C_BLUE);
            else if (currentPage == 2) pageColor = applyBrightness(C_GREEN);
            else                       pageColor = applyBrightness(C_RED);
            trellis.setPixelColor(31, pageColor);
        }
        trellis.show();
        return; // ← salir sin procesar estados
    }
    const byte* colorIndexMap;
    if      (currentPage == 1) colorIndexMap = LED_COLORS_PG1;
    else if (currentPage == 2) colorIndexMap = LED_COLORS_PG2;
    else                       colorIndexMap = LED_COLORS_PG3;

    for (int i = 0; i < 32; i++) {
        uint32_t baseColor = PALETTE[colorIndexMap[i]].rgb888;
        bool active = false;

        if (currentPage == 3) {
            byte note = MIDI_NOTES_PG1[i];
            if (note <= 31) {
                int group     = note / 8;
                int track_idx = note % 8;
                switch (group) {
                    case 0: active = recStates[track_idx];    break;
                    case 1: active = soloStates[track_idx];   break;
                    case 2: active = muteStates[track_idx];   break;
                    case 3: active = selectStates[track_idx]; break;
                }
            }
        } else if (currentPage == 1) {
            active = btnStatePG1[i];
        } else {
            active = btnStatePG2[i];
        }

        uint32_t colorFinal;
        if (active) {
            colorFinal = applyMidBrightness(baseColor);
        } else {
            colorFinal = applyBrightness(baseColor);
        }

        if (i == 26 && globalShiftPressed) colorFinal = C_YELLOW;
        if (i == 31) {
            if      (currentPage == 1) colorFinal = applyBrightness(C_BLUE);
            else if (currentPage == 2) colorFinal = applyBrightness(C_GREEN);
            else                       colorFinal = applyBrightness(C_RED);
        }

        trellis.setPixelColor(i, colorFinal);
    }
    trellis.show();
}

// ****************************************************************************
// Set Trellis Brightness
// ****************************************************************************




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

void setTrellisBrightness(uint8_t newBrightness) {
  trellisBrightness = newBrightness;
  log_d("[HARDWARE] Brillo de Trellis ajustado a: %d\n", newBrightness);
  // Al cambiar el brillo, forzamos una actualización de los LEDs para verlo al instante
  updateLeds();
}

uint32_t applyBrightness(uint32_t color) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >>  8) & 0xFF;
    uint8_t b =  color        & 0xFF;
    r = (r * trellisBrightness) / 255;
    g = (g * trellisBrightness) / 255;
    b = (b * trellisBrightness) / 255;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// Activo: más brillante que inactivo, sin deslumbrar
// Multiplica trellisBrightness x6, clampeado a 255
uint32_t applyMidBrightness(uint32_t color) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >>  8) & 0xFF;
    uint8_t b =  color        & 0xFF;
    uint16_t mid = min((uint16_t)(trellisBrightness * 8), (uint16_t)255);
    r = (r * mid) / 255;
    g = (g * mid) / 255;
    b = (b * mid) / 255;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}



// ---------------------------------------------------------
// NUEVO CALLBACK MAESTRO (Soporta Páginas y Mapas)
// ---------------------------------------------------------
TrellisCallback onTrellisEvent(keyEvent evt) {
    int key      = evt.bit.NUM;
    bool isPress = (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING);

    static unsigned long pg3Key31PressTime = 0;
    static constexpr unsigned long PG3_LONG_PRESS_MS = 600;

    if (isPress) log_e("TRELLIS key=%d note=0x%02X", key,
                       (currentPage==1 ? MIDI_NOTES_PG1 :
                        currentPage==2 ? MIDI_NOTES_PG2 :
                                         MIDI_NOTES_PG3)[key]);

    // ── Botón de página (PG1/PG2: press corto key 31) ────────────
    if (key == 31 && currentPage != 3) {
        if (isPress) {
            currentPage = (currentPage % 3) + 1;
            if (currentPage == 3) pg3Key31PressTime = millis();
            updateLeds();
            needsMainAreaRedraw = true;
        }
        return 0;
    }

    // ── Botón key 31 en PG3: press corto = SEL8, largo = página ──
    if (key == 31 && currentPage == 3) {
        if (isPress) {
            pg3Key31PressTime = millis();
        } else {
            unsigned long held = millis() - pg3Key31PressTime;
            if (held < PG3_LONG_PRESS_MS) {
                byte noteOn[3]  = { 0x90, 0x1F, 0x7F };
                byte noteOff[3] = { 0x90, 0x1F, 0x00 };
                sendMIDIBytes(noteOn,  3);
                sendMIDIBytes(noteOff, 3);
            } else {
                currentPage = (currentPage % 3) + 1;
                updateLeds();
                needsMainAreaRedraw = true;
            }
        }
        return 0;
    }

    // ── Botón SHIFT (key 26, solo PG1/PG2) ───────────────────────
    if (key == 26 && currentPage != 3) {
        globalShiftPressed = isPress;
        needsMainAreaRedraw = true;
        updateLeds();
    }

    // ── Botón SMPTE/BEATS (key 15, solo PG1) ─────────────────────
    if (key == 15 && currentPage == 1 && isPress) {
        byte midiMsg[3] = { 0x90, 0x35, 0x7F };
        sendMIDIBytes(midiMsg, 3);
        return 0;
    }

    // ── CALIB (key 16, solo PG1) ──────────────────────────────────
    if (key == 16 && currentPage == 1 && isPress) {
        if (_onCalibrateRequest) {
            for (int i = 0; i < 8; i++) {
                if (selectStates[i]) {
                    _onCalibrateRequest(i + 1);
                    break;
                }
            }
        }
        return 0;
    }

    // ── Enviar MIDI ───────────────────────────────────────────────
    const byte* currentMap = (currentPage == 1) ? MIDI_NOTES_PG1 :
                             (currentPage == 2) ? MIDI_NOTES_PG2 :
                                                  MIDI_NOTES_PG3;
    byte noteToSend = currentMap[key];

    if (noteToSend != 0x00) {
        byte midiMsg[3];
        midiMsg[0] = 0x90;
        midiMsg[1] = noteToSend;
        midiMsg[2] = isPress ? 127 : 0;
        sendMIDIBytes(midiMsg, 3);
    }

    return 0;
}
// Fin del codigo