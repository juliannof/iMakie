// src/midi/MIDIProcessor.cpp
#include "MIDIProcessor.h"
#include "../config.h"
#include <USBMIDI.h>  // ✅ NUEVO: para sendMIDIBytes()

/* =========================================================
   ACCESO AL OBJETO MIDI (definido en main.cpp)
   ========================================================= */
extern USBMIDI MIDI;

/* =========================================================
   sendMIDIBytes() — Reemplaza sendToPico() en todo el archivo
   Envía un array de bytes MIDI crudos por USB.
   Para SysEx: envía byte a byte (único método válido en Core 3.1.1)
   ========================================================= */
void sendMIDIBytes(const byte* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        MIDI.write(data[i]);
    }
}

// --- Variables internas del parser MIDI ---
namespace {
    byte midi_buffer[256];
    int midi_idx = 0;
    bool in_sysex = false;
    byte last_status_byte = 0;

    enum class HandshakeState {
        IDLE,
        AWAITING_CHALLENGE_BYTES
    };
    HandshakeState handshakeState = HandshakeState::IDLE;
    byte challenge_buffer[7];
    int challenge_idx = 0;

    unsigned long lastVersionReplyTime = 0;
    const unsigned long VERSION_REPLY_COOLDOWN_MS = 200; // reducido: Logic reintenta rápido durante init
}

// --- Prototipos privados ---
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

    if (in_sysex && (b & 0x80) && b != 0xF7) {
        log_w("¡SysEx interrumpido! Recibido Status 0x%02X dentro de SysEx. Reseteando.", b);
        in_sysex = false;
        midi_idx = 0;
    }

    // ----- LÓGICA DE HANDSHAKE -----
    if (handshakeState == HandshakeState::AWAITING_CHALLENGE_BYTES) {
        if (b < 0x80) {
            if (challenge_idx < 7) {
                challenge_buffer[challenge_idx++] = b;
                if (challenge_idx == 7) {
                    log_i("[HANDSHAKE] ¡7 bytes pescados! Completando handshake.");
                    handleMcuHandshake(challenge_buffer);
                    handshakeState = HandshakeState::IDLE;
                }
            } else {
                log_e("[HANDSHAKE] challenge_idx fuera de límites (%d). Byte 0x%02X.", challenge_idx, b);
            }
            return;
        }
    }

    // ----- LÓGICA MIDI NORMAL -----
    if (b == 0xF0) {
        in_sysex = true;
        midi_idx = 0;
        return;
    }
    if (b == 0xF7) {
        if (in_sysex) {
            in_sysex = false;
            processMackieSysEx(midi_buffer, midi_idx);
        } else {
            log_w("processMidiByte: 0xF7 sin SysEx activo. Ignorando.");
        }
        return;
    }
    if (in_sysex) {
        if (midi_idx < sizeof(midi_buffer)) {
            midi_buffer[midi_idx++] = b;
        } else {
            log_e("processMidiByte: Buffer SysEx desbordado. Descartando.");
            in_sysex = false;
            midi_idx = 0;
        }
        return;
    }

    // --- Mensajes de canal ---
    if (b & 0x80) {
        last_status_byte = b;
        midi_idx = 0;
    } else if (last_status_byte != 0) {
        if (midi_idx >= sizeof(midi_buffer)) {
            log_e("processMidiByte: midi_buffer desbordado. Descartando.");
            midi_idx = 0;
            last_status_byte = 0;
            return;
        }
        midi_buffer[midi_idx++] = b;
        byte cmd_type = last_status_byte & 0xF0;

        int msg_len_expected;
        if (cmd_type == 0xC0 || cmd_type == 0xD0) {
            msg_len_expected = 2;
        } else if (cmd_type == 0x80 || cmd_type == 0x90 || cmd_type == 0xB0 || cmd_type == 0xE0) {
            msg_len_expected = 3;
        } else {
            log_w("processMidiByte: Tipo de comando 0x%02X no reconocido.", cmd_type);
            midi_idx = 0;
            last_status_byte = 0;
            return;
        }

        if (midi_idx == (msg_len_expected - 1)) {
            if (cmd_type == 0x90 || cmd_type == 0x80) {
                processNote(last_status_byte, midi_buffer[0], midi_buffer[1]);
            } else if (cmd_type == 0xD0) {
                processChannelPressure(last_status_byte & 0x0F, midi_buffer[0]);
            } else if (cmd_type == 0xB0) {
                processControlChange(last_status_byte & 0x0F, midi_buffer[0], midi_buffer[1]);
            } else if (cmd_type == 0xE0) {
                int bendValue = (midi_buffer[1] << 7) | midi_buffer[0];
                processPitchBend(last_status_byte & 0x0F, bendValue);
            }
            midi_idx = 0;
        } else if (midi_idx >= msg_len_expected) {
            log_e("processMidiByte: Mensaje malformado. Descartando.");
            midi_idx = 0;
            last_status_byte = 0;
        }
    } else {
        log_w("processMidiByte: Data Byte huérfano (0x%02X). Ignorando.", b);
    }
}


// *****************************************************************
// Respuesta al Handshake MCU
// *****************************************************************
void handleMcuHandshake(byte* challenge_code) {
    log_v("handleMcuHandshake(): Respondiendo al Handshake MCU.");

    ConnectionState previousState = logicConnectionState;

    // 1. Respuesta con el código de desafío
    // ⚠️ Array de 15 bytes exactos — el byte extra causaba "Invalid 1 bytes"
    byte response[15] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x01, 0x00,
                          challenge_code[0], challenge_code[1], challenge_code[2], challenge_code[3],
                          challenge_code[4], challenge_buffer[5], challenge_buffer[6], 0xF7 };
    sendMIDIBytes(response, sizeof(response));
    log_i(">>> MCU Handshake: Respuesta enviada (%d bytes).", sizeof(response));

    // 2. Confirmación "Online"
    byte online_msg[] = {0xF0, 0x00, 0x00, 0x66, 0x14, 0x02, 0xF7};
    sendMIDIBytes(online_msg, sizeof(online_msg)); // ✅ reemplaza sendToPico()
    log_i(">>> MCU Handshake: Confirmación 'Online' enviada.");

    // 3. Actualizar estado
    if (previousState == ConnectionState::CONNECTED) {
        log_i("handleMcuHandshake(): Re-confirmación mientras ya estábamos CONNECTED.");
    } else {
        logicConnectionState = ConnectionState::MIDI_HANDSHAKE_COMPLETE;
        if (previousState == ConnectionState::DISCONNECTED ||
            previousState == ConnectionState::AWAITING_SESSION) {
            needsTOTALRedraw = true;
            log_i("Handshake completado. Transicionando a MIDI_HANDSHAKE_COMPLETE.");
        }
    }
}


// *****************************************************************
// Control Change — Display de Timecode/Beats
// *****************************************************************
void processControlChange(byte channel, byte controller, byte value) {
    log_d("processControlChange: CH=%d, CC=%d, Val=%d", channel, controller, value);

    if ((channel != 0 && channel != 15) || (controller < 64 || controller > 73)) {
        log_w("processControlChange: CC ignorado. Channel=%d, Controller=%d.", channel, controller);
        return;
    }

    int digit_index = 73 - controller;
    if (digit_index < 0 || digit_index >= 10) {
        log_e("processControlChange: Índice de dígito fuera de rango: %d", digit_index);
        return;
    }

    byte char_code = value & 0x3F;
    char display_char = (char_code < 64) ? MACKIE_CHAR_MAP[char_code] : '?';

    byte char_to_store = (byte)display_char;
    if ((value & 0x40) != 0) {
        char_to_store |= 0x80;
    }

    if (currentTimecodeMode == MODE_BEATS) {
        beatsChars_clean[digit_index] = char_to_store;
    } else {
        timeCodeChars_clean[digit_index] = char_to_store;
    }

    needsHeaderRedraw = true;
}

// Formatear beat string
String formatBeatString() {
    char formatted[14];
    int pos = 0;
    for (int i = 0; i < 12; i++) {
        byte char_with_dot = beatsChars_clean[i];
        char ascii_char = char_with_dot & 0x7F;
        if (ascii_char == 0 || ascii_char < 32) ascii_char = ' ';
        formatted[pos++] = ascii_char;
        if ((char_with_dot & 0x80) != 0) formatted[pos++] = '.';
    }
    formatted[pos] = '\0';
    String result = String(formatted);
    result.trim();
    return (result.length() == 0) ? "---.---" : result;
}

// Formatear timecode string
String formatTimecodeString() {
    char formatted[14];
    int pos = 0;
    for (int i = 0; i < 12; i++) {
        byte char_with_dot = timeCodeChars_clean[i];
        char ascii_char = char_with_dot & 0x7F;
        if (ascii_char == 0 || ascii_char < 32) ascii_char = ' ';
        formatted[pos++] = ascii_char;
        if ((char_with_dot & 0x80) != 0) formatted[pos++] = ':';
    }
    formatted[pos] = '\0';
    String result = String(formatted);
    result.trim();
    return (result.length() == 0) ? "--:--:--:--" : result;
}


// ****************************************************************************
// Vúmetros — Channel Pressure
// ****************************************************************************
void processChannelPressure(byte channel, byte value) {
    log_e(">> CP IN: Ch=%d, Val=%d", channel, value);

    float normalizedLevel = 0.0f;
    int targetChannel = -1;
    bool newClipState = false;
    bool clearClip = false;

    if (channel == 0) {
        targetChannel = (value >> 4) & 0x0F;
        byte mcu_level = value & 0x0F;

        if (targetChannel >= 8) return;

        switch (mcu_level) {
            case 0x0F: clearClip = true; normalizedLevel = vuLevels[targetChannel]; break;
            case 0x0E: newClipState = true; normalizedLevel = 1.0f; break;
            case 0x0D: case 0x0C: normalizedLevel = 1.0f; break;
            default:
                normalizedLevel = (mcu_level <= 11) ? (float)mcu_level / 11.0f : 0.0f;
                break;
        }
    } else if (channel >= 1 && channel <= 7) {
        targetChannel = channel;
        normalizedLevel = (float)value / 127.0f;
        if (value >= 127) newClipState = true;
    } else {
        return;
    }

    if (targetChannel != -1) {
        bool stateChanged = false;
        if (normalizedLevel >= vuLevels[targetChannel] || normalizedLevel == 0.0f) {
            vuLastUpdateTime[targetChannel] = millis();
        }
        if (clearClip) {
            if (vuClipState[targetChannel]) { vuClipState[targetChannel] = false; stateChanged = true; }
        } else if (newClipState) {
            if (!vuClipState[targetChannel]) { vuClipState[targetChannel] = true; stateChanged = true; }
        }
        if (normalizedLevel > vuLevels[targetChannel]) {
            vuLevels[targetChannel] = normalizedLevel;
            if (normalizedLevel > vuPeakLevels[targetChannel]) {
                vuPeakLevels[targetChannel] = normalizedLevel;
                vuPeakLastUpdateTime[targetChannel] = millis();
            }
            stateChanged = true;
        } else if (normalizedLevel == 0.0f && vuLevels[targetChannel] != 0.0f) {
            vuLevels[targetChannel] = 0.0f;
            stateChanged = true;
        }
        if (stateChanged) needsVUMetersRedraw = true;
    }
}


// *****************************************************************
// SysEx Mackie
// *****************************************************************
void processMackieSysEx(byte* payload, int len) {
    if (len < 5) return;

    byte device_family = payload[3];
    byte command = payload[4];

    // Acepta tanto Mackie Control (0x14) como ControlXT (0x15)
    if (device_family != 0x14 && device_family != 0x15) return;

    // Guard: si ya estamos en handshake activo, ignorar queries duplicadas
    if (command == 0x00 && len == 5) {
        if (handshakeState == HandshakeState::AWAITING_CHALLENGE_BYTES) {
            log_w("[HANDSHAKE] Query duplicada ignorada (ya esperando desafío).");
            return;
        }

        // Si ya estamos CONNECTED: solo confirmamos con 02 (ya online).
        // Enviar 01 de nuevo provocaría que Logic repitiese el ciclo completo.
        if (logicConnectionState == ConnectionState::CONNECTED) {
            log_i("[HANDSHAKE] Re-query en CONNECTED. Solo 02.");
            byte online_msg[] = {0xF0, 0x00, 0x00, 0x66, 0x14, 0x02, 0xF7};
            sendMIDIBytes(online_msg, sizeof(online_msg));
            return;
        }

        // HANDSHAKE_COMPLETE: responder 01+02 con bytes fijos sin consumir el burst
        if (logicConnectionState == ConnectionState::MIDI_HANDSHAKE_COMPLETE) {
            log_i("[HANDSHAKE] Re-query en HANDSHAKE_COMPLETE. Respondiendo 01+02 inmediato.");
            byte quick_response[15] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x01, 0x00,
                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF7 };
            sendMIDIBytes(quick_response, sizeof(quick_response));
            byte online_msg[] = {0xF0, 0x00, 0x00, 0x66, 0x14, 0x02, 0xF7};
            sendMIDIBytes(online_msg, sizeof(online_msg));
            return;
        }

        log_d("[HANDSHAKE] 'Connection Query' recibida (device=0x%02X). Pescando 7 bytes...", device_family);
        handshakeState = HandshakeState::AWAITING_CHALLENGE_BYTES;
        challenge_idx = 0;
        return;
    }

    switch(command) {
        case 0x13: { // Version Request (decimal 19 = 0x13, según protocolo Mackie)
            unsigned long currentTime = millis();
            if (currentTime - lastVersionReplyTime > VERSION_REPLY_COOLDOWN_MS) {
                log_d("<<< Version Request (0x13). Enviando respuesta.");
                // Version Reply = command 0x14 (decimal 20)
                byte version_reply[] = {0xF0, 0x00, 0x00, 0x66, 0x14, 0x14, '1', '.', '2', '.', '0', 0xF7};
                sendMIDIBytes(version_reply, sizeof(version_reply));
                lastVersionReplyTime = currentTime;
            }
            break;
        }

        case 0x12: { // LCD / Nombres de pista
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

        case 0x14: { // Time Code / BBT
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
                byte b1 = payload[5];
                byte b2 = payload[6];
                char c1 = (b1 >= 32 && b1 <= 126) ? (char)b1 : '?';
                char c2 = (b2 >= 32 && b2 <= 126) ? (char)b2 : '?';
                char assign_buf[3] = {c1, c2, '\0'};
                if (assignmentString != assign_buf) {
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
// Note On/Off — Botones REC/SOLO/MUTE/SELECT
// ****************************************************************************
void processNote(byte status, byte note, byte velocity) {
    if (note > 31) return;

    int group = note / 8;
    int track_idx = note % 8;
    bool is_on = ((status & 0xF0) == 0x90 && velocity > 0);
    bool stateChanged = false;

    switch(group) {
        case 0: if (recStates[track_idx]    != is_on) { recStates[track_idx]    = is_on; stateChanged = true; } break;
        case 1: if (soloStates[track_idx]   != is_on) { soloStates[track_idx]   = is_on; stateChanged = true; } break;
        case 2: if (muteStates[track_idx]   != is_on) { muteStates[track_idx]   = is_on; stateChanged = true; } break;
        case 3: if (selectStates[track_idx] != is_on) { selectStates[track_idx] = is_on; stateChanged = true; } break;
    }

    if (stateChanged) {
        needsMainAreaRedraw = true;
        const char* group_name = (group==0)?"REC":(group==1)?"SOLO":(group==2)?"MUTE":"SELECT";
        log_i("<<< BOTÓN: Pista %d - %s -> %s", track_idx + 1, group_name, is_on ? "ON" : "OFF");
    }
}


// ****************************************************************************
// Pitch Bend — Faders
// ****************************************************************************
void processPitchBend(byte channel, int bendValue) {
    if (channel >= 0 && channel < 9) {
        float faderPositionNormalized = (float)(bendValue + 8192) / 16383.0f;

        if (logicConnectionState == ConnectionState::MIDI_HANDSHAKE_COMPLETE) {
            logicConnectionState = ConnectionState::CONNECTED;
            needsTOTALRedraw = true;
            log_i("DAW conectado: Primer PitchBend en Track %d. -> CONNECTED.", channel + 1);
        }

        if (abs(faderPositions[channel] - faderPositionNormalized) > 0.001f) {
            faderPositions[channel] = faderPositionNormalized;
            needsMainAreaRedraw = true;
        }
    }
}

bool isLogicConnected() {
    return (logicConnectionState == ConnectionState::CONNECTED);
}