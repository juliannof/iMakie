// src/hardware/Hardware.cpp

#include "Hardware.h"                      // Incluye su propia cabecera
#include "../config.h"                     // Para acceder a los objetos (trellis) y estados (recStates, etc.)
#include "../hardware/Hardware.h"  // Para poder llamar a sendToPico




// ====================================================================
// --- IMPLEMENTACIÓN DE FUNCIONES PÚBLICAS ---
// ====================================================================

void initHardware() {
 
  
  Serial.println("[SETUP] Módulo de Hardware iniciado y verificado.");
}

void handleHardwareInputs() {
  // Esta función se llama en cada loop para leer el estado del Trellis.
}



// ****************************************************************************
// Update Leds
// ****************************************************************************

void updateLeds() {
    
    
}



// Fin del codigo