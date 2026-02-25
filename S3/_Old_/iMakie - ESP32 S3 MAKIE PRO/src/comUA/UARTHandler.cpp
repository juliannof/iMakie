// src/comUA/UARTHandler.cpp

#include "UARTHandler.h"
#include "../config.h"               // Para variables globales y defines
#include "../midi/MIDIProcessor.h"   // Para llamar a processMidiByte

// --- Variables "Privadas" y Estado de este Módulo ---
namespace {
    // Para el parser de frames UART
    byte uart_buffer[256];
    int uart_idx = 0;
    bool in_frame = false;

    // Para el handshake
    enum class HandshakeStatus { IDLE, AWAITING_CHALLENGE, READY_TO_RESPOND };
    HandshakeStatus handshake_status = HandshakeStatus::IDLE;
    byte challenge_code[7];
    int challenge_byte_count = 0;
}

// Prototipos de funciones internas de este módulo
void processMidiByteWithHandshake(byte b);

// *********************************************************************
// Inicialización y Manejo de la Comunicación UART con la Pico
// *********************************************************************

void initUART() {
    Serial2.begin(UART_BAUDRATE, SERIAL_8N1, PICO_RX_PIN, PICO_TX_PIN);
    Serial2.setRxBufferSize(4096);
    log_i("[SETUP] Módulo UART iniciado.");
}

// *************************************************************
// Funciones Públicas para Manejo de la Comunicación UART
// *************************************************************

void handleUART() {
    // 1. Procesar todos los bytes entrantes de la Pico
    while (Serial2.available()) {
        byte b = Serial2.read();
        if (b == USB_TO_UART_START) {
            in_frame = true;
            uart_idx = 0;
        } else if (b == USB_TO_UART_END) {
            if (in_frame) {
                // Frame completo recibido. Pasa su contenido, byte a byte,
                // a la lógica de procesamiento que gestiona el handshake.
                for (int i = 0; i < uart_idx; i++) {
                    processMidiByteWithHandshake(uart_buffer[i]);
                }
            }
            in_frame = false;
        } else if (in_frame && uart_idx < sizeof(uart_buffer)) {
            uart_buffer[uart_idx++] = b;
        }
    }

}

// *****************************************************
// Envío de Mensajes MIDI a la Pico y Estado de Conexión
// *****************************************************

void sendToPico(byte* midi_message, byte len) {
    if (len == 0) return;
    byte frame[len + 2];
    frame[0] = ESP_TO_USB_START;
    memcpy(&frame[1], midi_message, len);
    frame[len + 1] = ESP_TO_USB_END;
    Serial2.write(frame, sizeof(frame));
}


// *********************************************************************
// Función para consultar si el estado de conexión lógica es 'CONNECTED'
// *********************************************************************

bool isLogicConnected() {
    return (logicConnectionState == ConnectionState::CONNECTED);
}

// *********************************************************************
// Función de notificación que el módulo UART llamará cuando detecte el "Host Query" MIDI
// Esta función inicia el proceso de handshake MIDI con el DAW.
// *********************************************************************
void onHostQueryReceived() {
    log_d("onHostQueryReceived(): Función llamada. Estado actual: %d.", (int)logicConnectionState);

    // Si ya estamos conectados, ignorar nuevas consultas de host para evitar reinicializaciones indeseadas.
    if (logicConnectionState == ConnectionState::CONNECTED) {
        log_v("onHostQueryReceived(): Ya en estado CONNECTED. Ignorando consulta de host.");
        return;
    }
    
    // Loguear que la consulta de host ha sido detectada y que se inicia el proceso de handshake.
    log_i("<- [HANDSHAKE] Consulta de Host (DAW) detectada. Iniciando proceso de handshake MIDI.");

    // Establecer el estado de handshake para empezar a "pescar" los bytes del desafío.
    handshake_status = HandshakeStatus::AWAITING_CHALLENGE;
    challenge_byte_count = 0; // Resetear el contador de bytes del desafío.
    log_v("onHostQueryReceived(): Estado de handshake establecido a AWAITING_CHALLENGE.");
    
    // Si el sistema estaba completamente desconectado, es una nueva sesión.
    // Transicionar al estado AWAITING_SESSION.
    if (logicConnectionState == ConnectionState::DISCONNECTED) {
        logicConnectionState = ConnectionState::AWAITING_SESSION;
        // needsTOTALRedraw se activa para forzar el dibujo de la pantalla de "Conectando..." o "Esperando DAW..."
        needsTOTALRedraw = true; // <--- Bandera para un redibujo completo de la pantalla.
        log_i("onHostQueryReceived(): Transicionando a AWAITING_SESSION. needsTOTALRedraw = true.");
    } else {
        log_v("onHostQueryReceived(): Ya en estado AWAITING_SESSION o MIDI_HANDSHAKE_COMPLETE. No se cambia el estado principal.");
    }
}

// *********************************************************************
// --- Implementación de Funciones Internas ---
// *********************************************************************

void processMidiByteWithHandshake(byte b) {
    // Si estamos en el proceso de handshake, capturamos los bytes del desafío.
    if (handshake_status == HandshakeStatus::AWAITING_CHALLENGE) {
        if (b < 0x80) { // Un byte de desafío es siempre un byte de datos
          if (challenge_byte_count < 7) {
            challenge_code[challenge_byte_count++] = b;
          }
          if (challenge_byte_count == 7) {
            handshake_status = HandshakeStatus::READY_TO_RESPOND;
            log_i("[HANDSHAKE] Código de desafío completo capturado.");
          }
        }
        // Importante: No pasamos estos bytes al parser MIDI normal
        return;
    }
    
    // Si no estamos en handshake, el byte se pasa al procesador MIDI principal.
    processMidiByte(b);
}


// Fin del codigo