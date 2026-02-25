// src/midi/MIDIProcessor.cpp
#include "MIDIProcessor.h"
#include "../config.h"
#include "../comUA/UARTHandler.h" // ¡INCLUSIÓN CRUCIAL!

// --- Variables internas del parser MIDI (CON MÁQUINA DE ESTADOS) ---
namespace {
    byte midi_buffer[256];
    int midi_idx = 0;
    bool in_sysex = false;
    byte last_status_byte = 0;

    // Máquina de estados para "pescar" el desafío
    enum class HandshakeState {
        IDLE,
        AWAITING_CHALLENGE_BYTES 
    };
    HandshakeState handshakeState = HandshakeState::IDLE;
    byte challenge_buffer[7];
    int challenge_idx = 0;

    // Cooldown para la respuesta de versión
    unsigned long lastVersionReplyTime = 0;
    const unsigned long VERSION_REPLY_COOLDOWN_MS = 2000; // 2 segundos
}



// ****************************************************************************
// --- Prototipos de funciones especialistas "privadas" ---
// ****************************************************************************

void processMidiByte(byte b);
void onHostQueryDetected(); 

void processMackieSysEx(byte* payload, int len);
void processNote(byte status, byte note, byte velocity);
void handleMcuHandshake(byte* challenge_code);
void processChannelPressure(byte channel, byte value);
void processControlChange(byte channel, byte controller, byte value);
void processPitchBend(byte channel, int bendValue);
void processMackieFader(byte channel, int value);

float masterMeterLevel = 0.0f;
float masterPeakLevel = 0.0f;
bool masterClip = false;
unsigned long masterMeterDecayTimer = 0;



// ****************************************************************************
// Procesar byte MIDI entrante
// ****************************************************************************
void processMidiByte(byte b) {
    if (b >= 0xF8) return; // RealTime
    
    // --- PROTECCIÓN NUEVA: Fuga de SysEx ---
    // Si estamos esperando datos de SysEx, pero llega un Status Byte nuevo (ej: E0 para Fader)
    // significa que nos perdimos el F7 anterior. Hay que abortar el SysEx para no comerse el Fader.
    if (in_sysex && (b & 0x80) && b != 0xF7) {
        log_w("¡SysEx interrumpido! Recibido Status 0x%02X dentro de SysEx. Reseteando.", b);
        in_sysex = false;
        midi_idx = 0;
        // NO hacemos return, dejamos que este byte (b) se procese abajo como mensaje normal (Fader, Nota, etc)
    }
    // ----- LÓGICA DE HANDSHAKE (MÁXIMA PRIORIDAD) -----
    if (handshakeState == HandshakeState::AWAITING_CHALLENGE_BYTES) {
        // En este estado, los data bytes son parte del desafío MIDI
        if (b < 0x80) { // ¿Es un byte de datos?
            if (challenge_idx < 7) {
                // log_d("[HANDSHAKE-PESCA] Pescado byte de desafío #%d: 0x%02X", challenge_idx, b); // Desactivado para menos verbosidad
                challenge_buffer[challenge_idx++] = b;
                if (challenge_idx == 7) {
                    log_i("[HANDSHAKE-PESCA] ¡7 bytes pescados! Completando handshake. Desafío: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X", 
                        challenge_buffer[0], challenge_buffer[1], challenge_buffer[2], challenge_buffer[3], 
                        challenge_buffer[4], challenge_buffer[5], challenge_buffer[6]);
                    handleMcuHandshake(challenge_buffer);
                    handshakeState = HandshakeState::IDLE; // ¡Misión cumplida!
                }
            } else {
                log_e("[HANDSHAKE-PESCA] Advertencia: challenge_idx fuera de límites (%d) durante pesca de desafío. Byte 0x%02X.", challenge_idx, b);
            }
            return; // No pasar este byte de datos a la lógica normal.
        } else {
            // Es un byte de estado (F0, 90, D0, etc). Lo dejamos pasar a la lógica normal.
        }
    }
    // ----- LÓGICA MIDI NORMAL -----
    if (b == 0xF0) { // Inicio de SysEx
        log_v("processMidiByte: Detectado 0xF0 (SysEx Start). midi_idx=0.");
        in_sysex = true;
        midi_idx = 0;
        return;
    }
    if (b == 0xF7) { // Fin de SysEx
        log_v("processMidiByte: Detectado 0xF7 (SysEx End).");
        if (in_sysex) {
            in_sysex = false;
            log_d("processMidiByte: Procesando SysEx con %d bytes.", midi_idx); // Log antes de llamar a processMackieSysEx
            processMackieSysEx(midi_buffer, midi_idx);
        } else {
            log_w("processMidiByte: 0xF7 recibido sin iniciar SysEx. Ignorando.");
        }
        return;
    }
    if (in_sysex) { // Byte de datos SysEx
        if (midi_idx < sizeof(midi_buffer)) {
            midi_buffer[midi_idx++] = b;
        } else {
            log_e("processMidiByte: ERROR: Buffer SysEx desbordado al añadir byte 0x%02X. Descartando SysEx malformado.", b);
            in_sysex = false; // Descartar el SysEx malformado
            midi_idx = 0;
        }
        return;
    }   
    
    // --- LÓGICA DE MENSAJES DE CANAL (Status + Data Bytes) ---
    if (b & 0x80) { // Si el bit más significativo es 1, es un Status Byte
        last_status_byte = b;
        midi_idx = 0; // Se espera que los bytes de datos sigan
    } else if (last_status_byte != 0) { // Si NO es un Status Byte, es un Data Byte. SÓLO si tenemos un Status previo.

        // --- MANEJO DE ERRORES: Comprobación de desbordamiento de `midi_buffer` ---
        if (midi_idx >= sizeof(midi_buffer)) {
            log_e("processMidiByte: ERROR: midi_buffer desbordado al añadir Data Byte 0x%02X (capacidad %d). Descartando el mensaje actual.", b, sizeof(midi_buffer));
            midi_idx = 0; // Resetear para descartar el mensaje malformado.
            last_status_byte = 0; // También reseteamos el status para evitar Running Status erróneo.
            return; // No procesar este byte.
        }
        // --- FIN MANEJO ERRORES (BUFFER) ---

        midi_buffer[midi_idx++] = b; // Añadimos el byte de datos al buffer
        byte cmd_type = last_status_byte & 0xF0; // Extraemos el tipo de comando (ej. 0x90, 0xD0, 0xE0, 0xB0, 0xC0)
        
        int msg_len_expected;
        if (cmd_type == 0xC0 || cmd_type == 0xD0) { // Program Change o Channel Pressure -> 1 byte de datos
            msg_len_expected = 2; // Status + 1 Data Byte
        } else if (cmd_type == 0x80 || cmd_type == 0x90 || cmd_type == 0xB0 || cmd_type == 0xE0) { // Note On/Off, Control Change, Pitch Bend -> 2 bytes de datos
            msg_len_expected = 3; // Status + 2 Data Bytes
        } else {
            log_w("processMidiByte: Tipo de comando 0x%02X no reconocido. Descartando mensaje.", cmd_type);
            midi_idx = 0; // Descartar bytes si el Status no es reconocido.
            last_status_byte = 0;
            return;
        }

        log_v("processMidiByte: cmd_type=0x%02X. Longitud de mensaje esperada (Total): %d bytes. Bytes actuales: %d.", cmd_type, msg_len_expected, midi_idx + 1);

        // Si hemos recibido todos los bytes de datos esperados para el mensaje actual
        if (midi_idx == (msg_len_expected - 1)) {
            log_d("processMidiByte: Mensaje MIDI completo capturado para Status 0x%02X.", last_status_byte);
            // Loguear los datos para depuración
            if (msg_len_expected == 2) {
                log_d("  Datos: Data1=0x%02X", midi_buffer[0]);
            } else { // msg_len_expected == 3
                log_d("  Datos: Data1=0x%02X, Data2=0x%02X", midi_buffer[0], midi_buffer[1]);
            }

            // --- Dispatch a las funciones de procesamiento específicas ---
            if (cmd_type == 0x90 || cmd_type == 0x80) { // Note On/Off
                log_d("  Dispatching a processNote (S=0x%02X, N=0x%02X, V=0x%02X).", last_status_byte, midi_buffer[0], midi_buffer[1]);
                processNote(last_status_byte, midi_buffer[0], midi_buffer[1]);
            } else if (cmd_type == 0xD0) { // Channel Pressure (Vúmetros)
                log_d("  Dispatching a processChannelPressure (Chan=%d, Value=0x%02X).", last_status_byte & 0x0F, midi_buffer[0]);
                processChannelPressure(last_status_byte & 0x0F, midi_buffer[0]);
            } else if (cmd_type == 0xB0) { // Control Change
                log_d("  Dispatching a processControlChange (Chan=%d, Ctr=0x%02X, Val=0x%02X).", last_status_byte & 0x0F, midi_buffer[0], midi_buffer[1]);
                processControlChange(last_status_byte & 0x0F, midi_buffer[0], midi_buffer[1]);
            } else if (cmd_type == 0xE0) { // Pitch Bend (Faders)
                int bendValue = (midi_buffer[1] << 7) | midi_buffer[0]; 
                log_d("  Dispatching a processPitchBend (Chan=%d, Value=%d [LSB:0x%02X, MSB:0x%02X]).", last_status_byte & 0x0F, bendValue, midi_buffer[0], midi_buffer[1]);
                processPitchBend(last_status_byte & 0x0F, bendValue);
            } else if (cmd_type == 0xC0) { // Program Change (no tiene un manejador específico aún, pero podemos loguearlo)
                 log_v("  Mensaje Program Change (0x%02X) detectado (Chan:%d, Program:0x%02X). No hay manejador específico.", cmd_type, last_status_byte & 0x0F, midi_buffer[0]);
            } else {
                 log_w("  Tipo de comando 0x%02X (Channel %d) no manejado por una función específica.", cmd_type, last_status_byte & 0x0F);
            }
            midi_idx = 0; // *** CRÍTICO: Resetear el índice DESPUÉS de despachar el mensaje. ***
            log_v("processMidiByte: Mensaje completado y procesado. midi_idx reseteado.");

        } 
        // --- MANEJO DE MENSAJES MALFORMADOS (LONGITUD EXCEDIDA) ---
        else if (midi_idx >= msg_len_expected) { // Si hemos recibido más bytes de datos de los esperados
            log_e("processMidiByte: ERROR: Mensaje MIDI malformado. Más bytes de datos (%d) de los esperados (%d para Status 0x%02X). Descartando el mensaje actual.", midi_idx, msg_len_expected - 1, last_status_byte);
            midi_idx = 0; // Resetear para descartar el mensaje malformado.
            last_status_byte = 0; // Resetear el status para evitar Running Status erróneo.
        }
        // --- FIN MANEJO MENSAJES MALFORMADOS (LONGITUD) ---

    } else { // Si NO es un Status Byte y last_status_byte es 0 (Data Byte huérfano)
        log_w("processMidiByte: Data Byte huérfano (0x%02X) recibido sin Status Byte previo. Ignorando.", b);
    }
}


// *****************************************************************
// Respuesta al Handshake MCU ---
// *****************************************************************
void handleMcuHandshake(byte* challenge_code) {
    log_v("handleMcuHandshake(): Iniciando respuesta al Handshake MCU.");

    // Guardamos el estado previo de la conexión para tomar decisiones sobre redibujo.
    ConnectionState previousState = logicConnectionState; 

    // 1. Construir la respuesta con el código de desafío
    byte response[16] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x01, 0x00,
                          challenge_code[0], challenge_code[1], challenge_code[2], challenge_code[3],
                          challenge_code[4], challenge_buffer[5], challenge_buffer[6], 0xF7 };
    sendToPico(response, sizeof(response));
    log_i(">>> MCU Handshake: Enviando respuesta con el código de desafío.");
    
    // 2. Enviar confirmación "Online"
    byte online_msg[] = {0xF0, 0x00, 0x00, 0x66, 0x14, 0x02, 0xF7};
    sendToPico(online_msg, sizeof(online_msg));
    log_i(">>> MCU Handshake: Enviando confirmación 'Online'.");
    
    // 3. Actualizar estado de la aplicación
    // Si la conexión ya estaba establecida (durante el uso normal), no forzamos un reset visual.
    if (previousState == ConnectionState::CONNECTED) {
        log_i("handleMcuHandshake(): DAW re-confirmó conexión mientras ya estábamos CONNECTED. No se fuerza redibujo total.");
        // El estado logicConnectionState sigue siendo CONNECTED.
        // No necesitamos needsTOTALRedraw ni re-inicializar el UI.
    } else {
        // Esto cubre DISCONNECTED, AWAITING_SESSION, MIDI_HANDSHAKE_COMPLETE.
        // Si el DA se reconecta, logicConnectionState podría ser cualquier valor anterior a CONNECTED
        // La lógica del handshake siempre lleva al estado intermedio MIDI_HANDSHAKE_COMPLETE
        logicConnectionState = ConnectionState::MIDI_HANDSHAKE_COMPLETE; 
        
        // Forzar un redibujo TOTAL solo si no estábamos ya en un estado de reconocimiento de DAW.
        // Esto evita redibujar toda la pantalla de inicialización si por alguna razón
        // ya habíamos pasado este estado y se reintegra la conexión.
        if (previousState == ConnectionState::DISCONNECTED || 
            previousState == ConnectionState::AWAITING_SESSION) {
            needsTOTALRedraw = true; 
            log_i("Handshake MCU completado. Transicionando a MIDI_HANDSHAKE_COMPLETE desde estado DISCONNECTED/AWAITING_SESSION.");
            log_v("Pantalla marcada para redibujo total (needsTOTALRedraw = true).");
        } else {
            // Si ya estábamos en MIDI_HANDSHAKE_COMPLETE (ej. reconfirmación durante la espera),
            // no forzamos otro needsTOTALRedraw, ya que la pantalla de inicialización ya debería estar mostrándose.
            log_i("Handshake MCU completado. Transicionando a MIDI_HANDSHAKE_COMPLETE (sin redibujo total forzado).");
        }
    }
}


// *****************************************************************
// * @brief Procesa los mensajes MIDI de Control Change (0xB0).
// *****************************************************************
//* Se especializa en decodificar los mensajes utilizados por el protocolo Mackie
//* para actualizar el display de 12 dígitos de Timecode/Beats.
//*
//* @param channel Canal MIDI del mensaje (0-15).
//* @param controller Número del controlador CC (0-127).

void processControlChange(byte channel, byte controller, byte value) {
    log_d("processControlChange: Recibido CC, channel=%d, controller=%d, value=%d", channel, controller, value);

    // Filtrar mensajes que no son para el Mackie Display
    if ((channel != 0 && channel != 15) || (controller < 64 || controller > 73)) {
        log_w("processControlChange: CC ignorado. Channel=%d, Controller=%d.", channel, controller);
        return;
    }
    
    // Calcular índice del dígito (0-9)
    int digit_index = 73 - controller;
    
    // Verificación de límites
    if (digit_index < 0 || digit_index >= 10) { 
        log_e("processControlChange: ERROR: Índice de dígito fuera de rango: %d", digit_index);
        return;
    }
    
    // Extraer código del carácter Mackie (bits 0-5)
    byte char_code = value & 0x3F;
    char display_char;
    
    // Buscar carácter ASCII correspondiente
    if (char_code < 64) { 
        display_char = MACKIE_CHAR_MAP[char_code];
    } else {
        display_char = '?';
        log_e("processControlChange: char_code 0x%02X fuera de rango.", char_code);
    }
    
    // Preparar carácter con bit de punto
    byte char_to_store = (byte)display_char;
    if ((value & 0x40) != 0) { // Bit 6 activo = punto
        char_to_store |= 0x80;
    }
    
    // Actualizar array correspondiente según modo
    if (currentTimecodeMode == MODE_BEATS) {
        beatsChars_clean[digit_index] = char_to_store;
        log_w("processControlChange: beatsChars_clean[%d] = 0x%02X ('%c')", 
              digit_index, char_to_store, display_char);
    } else {
        timeCodeChars_clean[digit_index] = char_to_store;
        log_w("processControlChange: timeCodeChars_clean[%d] = 0x%02X ('%c')", 
              digit_index, char_to_store, display_char);
    }
    
    // Marcar que el header necesita redibujarse
    needsHeaderRedraw = true;
    
    // FORZAR redibujado inmediato (opcional)
    // drawHeaderSprite();
    
    log_i("processControlChange: Digit[%d], Char:'%c', Dot:%s, Mode:%s",
          digit_index, display_char, 
          ((value & 0x40) != 0) ? "YES" : "NO",
          (currentTimecodeMode == MODE_BEATS) ? "BEATS" : "TIMECODE");
}

// Función para formatear el beat
String formatBeatString() {
    char formatted[14]; // 12 caracteres + puntos extra + null
    int pos = 0;
    
    for (int i = 0; i < 12; i++) {
        byte char_with_dot = beatsChars_clean[i];
        char ascii_char = char_with_dot & 0x7F;  // Extraer ASCII
        
        // Si es 0 (vacío) o carácter no imprimible, usar espacio
        if (ascii_char == 0 || ascii_char < 32) {
            ascii_char = ' ';
        }
        
        formatted[pos++] = ascii_char;
        
        // Si tiene punto, agregarlo después (formato musical usa puntos)
        if ((char_with_dot & 0x80) != 0) {
            formatted[pos++] = '.';
        }
    }
    
    formatted[pos] = '\0';
    
    String result = String(formatted);
    result.trim();  // Eliminar espacios al inicio/final
    
    // Si está vacío, retornar placeholder
    if (result.length() == 0) {
        return "---.---";
    }
    
    return result;
}

// Función para formatear timecode
String formatTimecodeString() {
    char formatted[14];
    int pos = 0;
    
    for (int i = 0; i < 12; i++) {
        byte char_with_dot = timeCodeChars_clean[i];
        char ascii_char = char_with_dot & 0x7F;
        
        if (ascii_char == 0 || ascii_char < 32) {
            ascii_char = ' ';
        }
        
        formatted[pos++] = ascii_char;
        
        // Para timecode, los puntos se convierten en ':'
        if ((char_with_dot & 0x80) != 0) {
            formatted[pos++] = ':';
        }
    }
    
    formatted[pos] = '\0';
    
    String result = String(formatted);
    result.trim();
    
    if (result.length() == 0) {
        return "--:--:--:--";
    }
    
    return result;
}



// ****************************************************************************
// --- NUEVA FUNCIÓN: Manejador de Vúmetros ---
// ****************************************************************************

void processChannelPressure(byte channel, byte value) {
    // [DEBUG 1] Entrada Cruda: Aquí vemos TODO lo que entra
    log_e(">> CP IN: Ch=%d, Val=%d", channel, value);

    float normalizedLevel = 0.0f;
    int targetChannel = -1;
    bool newClipState = false;
    bool clearClip = false;

    // --- Detección de formato (sin cambios lógicos) ---
    if (channel == 0) {
        // FORMATO "PURO" MCU (Nibbles en Canal 0)
        targetChannel = (value >> 4) & 0x0F;
        byte mcu_level = value & 0x0F;
        
        log_e("   -> Modo MCU. Target: %d, Nivel Raw: %d", targetChannel, mcu_level);

        if (targetChannel >= 8) {
            log_e("   -> RECHAZADO MCU: Target %d es >= 8", targetChannel);
            return;
        }

        switch (mcu_level) {
            case 0x0F: // Clear overload
                clearClip = true;
                normalizedLevel = vuLevels[targetChannel]; 
                break;
            case 0x0E: // Set overload
                newClipState = true;
                normalizedLevel = 1.0f;
                break;
            case 0x0D: // 100% (>0dB)
                normalizedLevel = 1.0f;
                break;
            case 0x0C: // Nivel 12 de 12
                normalizedLevel = 1.0f; 
                break;
            default: // Niveles de 0x00 a 0x0B
                if (mcu_level <= 11) {
                    normalizedLevel = (float)mcu_level / 11.0f;
                } else {
                    normalizedLevel = 0.0f;
                }
                break;
        }
    } 
    else if (channel >= 1 && channel <= 7) {
        // FORMATO "LOGIC PRO" (Canales directos)
        // OJO: Logic suele mandar Master en Channel 8 (0-based) = MIDI Ch 9
        // Tu condición 'channel <= 7' lo está bloqueando.
        
        targetChannel = channel;
        log_e("   -> Modo Logic. Target: %d", targetChannel);

        normalizedLevel = (float)value / 127.0f;
        if (value >= 127) {
            newClipState = true;
        }
    } 
    else {
        // [DEBUG 2] Aquí caerá el Master si viene en CH 8
        log_e("   -> RECHAZADO GLOBAL: Canal %d no está entre 1 y 7 ni es 0", channel);
        return; // Canal no válido
    }

    // --- Actualización de estado global (MODIFICADO) ---
    if (targetChannel != -1) {
        // [DEBUG 3] Confirmación de actualización
        // log_e("   -> Actualizando vuLevels[%d] = %.2f", targetChannel, normalizedLevel);

        bool stateChanged = false;
        // Actualizar el timer para el decay del nivel normal
        if (normalizedLevel >= vuLevels[targetChannel] || normalizedLevel == 0.0f) {
            vuLastUpdateTime[targetChannel] = millis();
        }
        // Actualizar el estado de clipping (sin cambios)
        if (clearClip) {
            if (vuClipState[targetChannel]) {
                vuClipState[targetChannel] = false;
                stateChanged = true;
            }
        } else if (newClipState) {
            if (!vuClipState[targetChannel]) {
                vuClipState[targetChannel] = true;
                stateChanged = true;
            }
        }
        // Actualizar el nivel del vúmetro y el PICO
        if (normalizedLevel > vuLevels[targetChannel]) {
            // El nivel instantáneo sube
            vuLevels[targetChannel] = normalizedLevel;
            
            // --- LÓGICA DE PICO (PEAK HOLD) AÑADIDA ---
            // Si el nuevo nivel también supera el pico guardado, actualizamos el pico
            if (normalizedLevel > vuPeakLevels[targetChannel]) {
                vuPeakLevels[targetChannel] = normalizedLevel;
                // Y reseteamos el timer de 'hold' del pico
                vuPeakLastUpdateTime[targetChannel] = millis();
            }
            
            stateChanged = true;
        } else if (normalizedLevel == 0.0f && vuLevels[targetChannel] != 0.0f) {
            // Si el DAW manda un cero explícito
            vuLevels[targetChannel] = 0.0f;
            stateChanged = true;
        }
        
        // Si algo cambió, activar redibujado
        if (stateChanged) {
            needsVUMetersRedraw = true;
        }
    }
}

// *****************************************************************
// --- SysEx Mackie ---
// *****************************************************************

void processMackieSysEx(byte* payload, int len) {
    if (len < 5) return;
    
    byte device_family = payload[3];
    byte command = payload[4];

    if (device_family != 0x14) return;
    // ----- DETECCIÓN DE INICIO DE HANDSHAKE -----
    if (command == 0x00 && len == 5) {
        log_d("[HANDSHAKE] 'Connection Query' (0x00) recibida. Empezando a 'pescar' 7 bytes de desafío...");
        handshakeState = HandshakeState::AWAITING_CHALLENGE_BYTES;
        challenge_idx = 0; // Preparamos el buffer para la pesca
        return; // El mensaje 0x00 solo sirve para activar el estado.
    }
    
    // ----- PROCESAMIENTO DE SYSEX NORMAL -----
    switch(command) {
        case 0x0E: { // Version Request
            unsigned long currentTime = millis();
            if (currentTime - lastVersionReplyTime > VERSION_REPLY_COOLDOWN_MS) {
                log_d("<<< Version Request recibido. Enviando UNA respuesta.");
                byte version_reply[] = {0xF0, 0x00, 0x00, 0x66, 0x14, 0x0F, '1', '.', '2', '.', '0', 0xF7};
                sendToPico(version_reply, sizeof(version_reply));
                lastVersionReplyTime = currentTime;
            }
            break;
        }
        
        case 0x12: { // Nombres de Pista y texto LCD
            if (len < 6) break;
            byte startOffset = payload[5];
            int text_len = len - 6;
            if (text_len <= 0) break;
            for (int i = 0; i < text_len; i++) {
                byte currentOffset = startOffset + i;
                if (currentOffset % 7 != 0) continue;
                bool isFirstRow = (currentOffset < 56);
                if (isFirstRow) {
                    int track_idx = currentOffset / 7;
                    if (track_idx < 8) {
                        int charsToCopy = min((size_t)6, (size_t)(text_len - i));
                        char name_buf[7];
                        strncpy(name_buf, (const char*)&payload[6 + i], charsToCopy);
                        name_buf[charsToCopy] = '\0';
                        for (int j = strlen(name_buf) - 1; j >= 0; j--) {
                            if (name_buf[j] == ' ') name_buf[j] = '\0'; else break;
                        }
                        if (trackNames[track_idx] != name_buf) {
                            trackNames[track_idx] = String(name_buf);
                            needsMainAreaRedraw = true; 
                        }
                    }
                }
            }
            break;
        }
        case 0x14: { // Time Code / BBT Display
            if (len >= 15) { 
                char time_buf[11];
                memcpy(time_buf, &payload[5], 10);
                time_buf[10] = '\0';
                if (timeCodeString != time_buf) timeCodeString = String(time_buf);
            }
            break;
        }
        case 0x11: { // Assignment Display
            if (len >= 7) {
            // 1. Ver qué demonios está llegando realmente
                byte b1 = payload[5];
                byte b2 = payload[6];
                log_i("SysEx 0x11 RAW Values: 0x%02X 0x%02X", b1, b2);

                // 2. Limpieza de seguridad (Sanitizing)
                // Si el caracter es menor que espacio (32) o mayor que ~ (126), lo forzamos a un espacio o '?'
                char c1 = (b1 >= 32 && b1 <= 126) ? (char)b1 : '?';
                char c2 = (b2 >= 32 && b2 <= 126) ? (char)b2 : '?';

                char assign_buf[3];
                assign_buf[0] = c1;
                assign_buf[1] = c2;
                assign_buf[2] = '\0'; // Importante: Terminador nulo

                // 3. Actualizar y dibujar
                if (assignmentString != assign_buf) {
                    log_e("Actualizando Assignment de '%s' a '%s'", assignmentString.c_str(), assign_buf);
                    assignmentString = String(assign_buf);
                    needsHeaderRedraw = true;
                }
            }
            break;
        }
        
        default:
            break;
    }
}


// ****************************************************************************
// Procesar Note On/Off (Botones)
// ****************************************************************************
void processNote(byte status, byte note, byte velocity) {
    // Limita a las notas de botones REC/SOLO/MUTE/SELECT por ahora (0-31).
    // Si manejas otros botones con NoteOn/Off (ej. Fader Touch G#7 a E8, vPot Clicks G#1 a D#2),
    // necesitarías expandir este filtro y su lógica.
    if (note > 31) { 
        log_v("processNote: Nota (0x%02X) fuera del rango de botones principales (0-31). Ignorando.", note);
        return; 
    }

    int group = note / 8;        // 0=REC (0-7), 1=SOLO (8-15), 2=MUTE (16-23), 3=SELECT (24-31)
    int track_idx = note % 8;    // Pista 0-7

    bool is_on = ((status & 0xF0) == 0x90 && velocity > 0); // Detecta Note On (velocity > 0)
    bool stateChanged = false; // Flag para saber si el estado del botón cambió
    String currentReason = ""; // Usada para el log, no para variable global 'redrawReason'.

    // Actualiza el estado del botón y establece 'stateChanged' si hubo un cambio
    switch(group) {
        case 0: // REC (Notas 0-7)
            if (recStates[track_idx] != is_on) {
                recStates[track_idx] = is_on;
                stateChanged = true;
                currentReason = "Feedback REC Pista " + String(track_idx + 1);
            }
            break;
        case 1: // SOLO (Notas 8-15)
            if (soloStates[track_idx] != is_on) {
                soloStates[track_idx] = is_on;
                stateChanged = true;
                currentReason = "Feedback SOLO Pista " + String(track_idx + 1);
            }
            break;
        case 2: // MUTE (Notas 16-23)
            if (muteStates[track_idx] != is_on) {
                muteStates[track_idx] = is_on;
                stateChanged = true;
                currentReason = "Feedback MUTE Pista " + String(track_idx + 1);
            }
            break;
        case 3: // SELECT (Notas 24-31)
            if (selectStates[track_idx] != is_on) {
                selectStates[track_idx] = is_on;
                stateChanged = true;
                currentReason = "Feedback SELECT Pista " + String(track_idx + 1);
            }
            break;
    }
    
    // Si el estado de algún botón cambió, activamos la bandera needsMainAreaRedraw
    if(stateChanged) {
        needsMainAreaRedraw = true; // <--- Usamos el flag específico para MainArea.
        // La String 'currentReason' se utiliza solo para el log local, no para una variable global.
        const char* group_name = (group==0)?"REC":(group==1)?"SOLO":(group==2)?"MUTE":"SELECT";
        log_i("<<< FEEDBACK DE BOTÓN: Pista %d - %s ahora está %s", 
                      track_idx + 1, 
                      group_name, 
                      is_on ? "ON" : "OFF");
    }
}



// ****************************************************************************
// --- Manejador de Pitch Bend (Faders) ---
// ****************************************************************************
void processPitchBend(byte channel, int bendValue) {
    log_d("processPitchBend: Recibido Pitch Bend para Channel %d, Value %d.", channel, bendValue);

    // Los faders Mackie Control ocupan los canales MIDI 0-7 para los faders 1-8.
    // Si manejas un fader maestro en el canal 8, tendrías que ajustar el rango a '< 9'.
    if (channel >= 0 && channel < 9) { // Cambiado a < 8 dado tu implementación para faders 0-7
        // Normalizar el valor de Pitch Bend (14 bits: -8192 a 8191) a un rango flotante de 0.0 a 1.0
        float faderPositionNormalized = (float)(bendValue + 8192) / 16383.0f; 
        log_i("  Fader Track %d: Posición normalizada: %.3f (Valor MIDI Raw: %d)", channel + 1, faderPositionNormalized, bendValue);
        
        // --- Lógica de Transición a CONNECTED (si aplica) ---
        // Se activa la conexión completa al recibir el primer Pitch Bend válido.
        if (logicConnectionState == ConnectionState::MIDI_HANDSHAKE_COMPLETE) {
            logicConnectionState = ConnectionState::CONNECTED;
            needsTOTALRedraw = true; // <--- Activamos redibujo TOTAL al pasar a CONNECTED
            log_i("DAW conectado: Primer Pitch Bend recibido para Track %d. Transicionando a CONNECTED.", channel + 1);
        }
        
        // Actualizar la posición global del fader si es un cambio significativo
        // Necesitarías una forma de dibujar los faders en la pantalla para ver esto.
        // Asumo que faderPositions[] se usaría en drawMainArea() o en una función de dibujo de faders independiente.
        if (abs(faderPositions[channel] - faderPositionNormalized) > 0.001f) { // Umbral para evitar redibujados por ruido mínimo
            faderPositions[channel] = faderPositionNormalized;
            needsMainAreaRedraw = true; // <--- Usamos el flag específico para MainArea (si los faders se dibujan ahí)
            log_v("  Fader Track %d posición actualizada a %.3f. needsMainAreaRedraw=true.", channel + 1, faderPositionNormalized);
        }
    } else {
        log_v("  processPitchBend: Ignorando Pitch Bend para canal no fader principal (%d).", channel);
    }
}



//Fin del Codigo