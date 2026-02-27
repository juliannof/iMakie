// src/midi/MIDIProcessor.cpp
#include "MIDIProcessor.h"
#include "../config.h"
#include <USBMIDI.h>  // ✅ NUEVO: para sendMIDIBytes()

/* =========================================================
   ACCESO AL OBJETO MIDI (definido en main.cpp)
   ========================================================= */
extern USBMIDI MIDI;
extern void updateLeds(); // definida en Hardware.cpp
extern bool btnStatePG1[32];
extern bool btnStatePG2[32];


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

    static uint16_t fadersAtMinMask = 0;
    static unsigned long firstFaderMinTime = 0;
    static const uint16_t ALL_FADERS_MIN_MASK = 0x01FF; // bits 0-8
    static unsigned long lastMidiActivityTime = 0;
    
    static const unsigned long MIDI_TIMEOUT_MS = 28000; // sin MIDI = desconectado
    static const int DISCONNECT_THRESHOLD = 9;           // 7 de 9 es suficiente
    static const unsigned long DISCONNECT_WINDOW_MS = 150; // ventana de 300ms

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


/* =========================================================
   sendMIDIBytes() — Reemplaza sendToPico() en todo el archivo
   Envía un array de bytes MIDI crudos por USB.
   Para SysEx: envía byte a byte (único método válido en Core 3.1.1)
   ========================================================= */
void sendMIDIBytes(const byte* data, size_t len) {
    log_v("[MIDI OUT] Enviando %d bytes", len);

    if (data[0] == 0xF0) {
        size_t i = 0;
        while (i < len) {
            midiEventPacket_t packet;
            size_t remaining = len - i;

            if (remaining >= 3 && (i + 3) < len) {
                // SysEx start / continue
                packet.header = 0x04;
                packet.byte1  = data[i];
                packet.byte2  = data[i+1];
                packet.byte3  = data[i+2];
                i += 3;
            } else if (remaining == 1) {
                // SysEx end con 1 byte
                packet.header = 0x05;
                packet.byte1  = data[i];
                packet.byte2  = 0x00;
                packet.byte3  = 0x00;
                i += 1;
            } else if (remaining == 2) {
                // SysEx end con 2 bytes
                packet.header = 0x06;
                packet.byte1  = data[i];
                packet.byte2  = data[i+1];
                packet.byte3  = 0x00;
                i += 2;
            } else {
                // SysEx end con 3 bytes
                packet.header = 0x07;
                packet.byte1  = data[i];
                packet.byte2  = data[i+1];
                packet.byte3  = data[i+2];
                i += 3;
            }
            MIDI.writePacket(&packet);
        }
        log_v("[MIDI OUT] SysEx enviado OK (%d bytes)", len);
        return;
    }

    if (len == 3) {
        byte status  = data[0] & 0xF0;
        byte channel = (data[0] & 0x0F) + 1;
        byte byte1   = data[1];
        byte byte2   = data[2];

        switch (status) {
            case 0x90:
                if (byte2 > 0) MIDI.noteOn(byte1, byte2, channel);
                else           MIDI.noteOff(byte1, 0, channel);
                break;
            case 0x80:
                MIDI.noteOff(byte1, byte2, channel);
                break;
            case 0xB0:
                MIDI.controlChange(byte1, byte2, channel);
                break;
            default:
                log_e("[MIDI OUT] Mensaje no soportado: 0x%02X", status);
                break;
        }
    }
}

// ****************************************************************************
// Procesar byte MIDI entrante
// ****************************************************************************

void processMidiByte(byte b) {
    log_v(">> RAW: 0x%02X", b);

    if (b >= 0xF8) return; // RealTime

    if (in_sysex && (b & 0x80) && b != 0xF7) {
        log_w("¡SysEx interrumpido! Recibido Status 0x%02X dentro de SysEx. Reseteando.", b);
        in_sysex = false;
        midi_idx = 0;
        // ✅ Cancelar handshake si estaba pescando bytes
        if (handshakeState == HandshakeState::AWAITING_CHALLENGE_BYTES) {
            handshakeState = HandshakeState::IDLE;
            challenge_idx = 0;
            log_w("[HANDSHAKE] Cancelado por interrupción SysEx.");
        }
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
            return; // solo consume data bytes
        }
        // b >= 0x80: cae al procesamiento normal SIN cancelar handshakeState
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
            log_v("processMidiByte: Buffer SysEx desbordado. Descartando.");
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
            log_v("processMidiByte: Mensaje malformado. Descartando.");
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
    ConnectionState previousState = logicConnectionState;

    // ✅ challenge_code[] para todos los bytes — no mezclar con challenge_buffer
    byte response[15] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x01, 0x00,
                          challenge_code[0], challenge_code[1], challenge_code[2],
                          challenge_code[3], challenge_code[4], challenge_code[5],
                          challenge_code[6], 0xF7 };
    sendMIDIBytes(response, sizeof(response));  // ✅ sendMIDIBytes, no sendToPico
    log_e(">>> MCU Handshake: Respuesta enviada (15 bytes).");

    byte online_msg[] = {0xF0, 0x00, 0x00, 0x66, 0x14, 0x02, 0xF7};
    sendMIDIBytes(online_msg, sizeof(online_msg));  // ✅
    log_i(">>> MCU Handshake: Confirmación 'Online' enviada.");

    if (previousState == ConnectionState::CONNECTED) {
        log_i("handleMcuHandshake(): Re-confirmación en CONNECTED. Sin redibujo.");
    } else {
        logicConnectionState = ConnectionState::MIDI_HANDSHAKE_COMPLETE;
        if (previousState == ConnectionState::DISCONNECTED ||
            previousState == ConnectionState::AWAITING_SESSION) {
            needsTOTALRedraw = true;
            log_i("Handshake completado. -> MIDI_HANDSHAKE_COMPLETE.");
        }
    }
}


// *****************************************************************
// Control Change — Display de Timecode/Beats
// *****************************************************************
void processControlChange(byte channel, byte controller, byte value) {
    log_d("CC CH=%d, CC=%d, Val=0x%02X", channel, controller, value);

    if ((channel != 0 && channel != 15) || (controller < 64 || controller > 73)) {
        log_w("CC ignorado. CH=%d, CC=%d", channel, controller);
        return;
    }

    int digit_index = 73 - controller;  // CC64→idx9 (MSB), CC73→idx0 (LSB)

    byte char_code   = value & 0x3F;
    char ascii_char  = (char_code < 64) ? MACKIE_CHAR_MAP[char_code] : '?';
    byte char_to_store = (byte)ascii_char;
    if (value & 0x40) char_to_store |= 0x80;  // bit separador

    // *** Guardar en AMBOS buffers siempre ***
    // El DAW envía CCs antes de la nota de modo → no sabemos aún qué modo vendrá
    beatsChars_clean[digit_index]    = char_to_store;
    timeCodeChars_clean[digit_index] = char_to_store;

    needsHeaderRedraw = true;
}

// ****************************************************************************
// Formatear Beat String  BAR.BEAT.SUB.TICK
// ****************************************************************************
String formatBeatString() {
    char formatted[14];
    int pos = 0;
    for (int i = 0; i < 10; i++) {
        byte b = beatsChars_clean[i];
        char c = b & 0x7F;
        if (c == 0 || c < 32) c = ' ';
        formatted[pos++] = c;
        if (b & 0x80) formatted[pos++] = '.';
    }
    formatted[pos] = '\0';
    String result = String(formatted);
    result.trim();
    return (result.length() == 0) ? "---.---.---" : result;
}

// ****************************************************************************
// Formatear Timecode String  HH:MM:SS:FF
// ****************************************************************************
String formatTimecodeString() {
    char formatted[14];
    int pos = 0;
    for (int i = 0; i < 10; i++) {
        byte b = timeCodeChars_clean[i];
        char c = b & 0x7F;
        if (c == 0 || c < 32) c = ' ';
        formatted[pos++] = c;
        if (b & 0x80) formatted[pos++] = '.';  // igual que BEATS
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
    log_v(">> CP IN: Ch=%d, Val=%d", channel, value);

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

    if (device_family != 0x14 && device_family != 0x15) return;

    // ── HANDSHAKE: Query ──────────────────────────────────────────────────────
    if (command == 0x00 && len == 5) {

        if (logicConnectionState == ConnectionState::CONNECTED) {
            log_i("[HANDSHAKE] Re-query en CONNECTED.");
            fadersAtMinMask   = 0;
            firstFaderMinTime = 0;
            lastMidiActivityTime = millis();
            byte online_msg[] = {0xF0, 0x00, 0x00, 0x66, 0x14, 0x02, 0xF7};
            sendMIDIBytes(online_msg, sizeof(online_msg));
            return;
        }

        // Generar challenge aleatorio (4 bytes, 7-bit)
        byte l1 = random(0x01, 0x7F);
        byte l2 = random(0x01, 0x7F);
        byte l3 = random(0x01, 0x7F);
        byte l4 = random(0x01, 0x7F);
        challenge_buffer[0] = l1;
        challenge_buffer[1] = l2;
        challenge_buffer[2] = l3;
        challenge_buffer[3] = l4;

        byte response[15] = {0xF0, 0x00, 0x00, 0x66, 0x14, 0x01, 0x00,
                             l1, l2, l3, l4, 0x00, 0x00, 0x00, 0xF7};
        sendMIDIBytes(response, sizeof(response));
        handshakeState = HandshakeState::AWAITING_CHALLENGE_BYTES;
        challenge_idx  = 0;
        log_e("[HANDSHAKE] Challenge enviado: 0x%02X 0x%02X 0x%02X 0x%02X", l1, l2, l3, l4);
        return;
    }

    switch (command) {

        // ── HANDSHAKE: Respuesta de Logic ─────────────────────────────────────
        case 0x01: {
            if (len < 12) break;

            byte l1 = challenge_buffer[0];
            byte l2 = challenge_buffer[1];
            byte l3 = challenge_buffer[2];
            byte l4 = challenge_buffer[3];

            // Respuesta esperada según algoritmo Mackie
            byte r1 = 0x7F & (l1 + (l2 ^ 0x0a) - l4);
            byte r2 = 0x7F & ((l3 >> 4) ^ (l1 + l4));
            byte r3 = 0x7F & (l4 - (l3 << 2) ^ (l1 | l2));
            byte r4 = 0x7F & (l2 - l3 + (0xF0 ^ (l4 << 4)));

            byte gr1 = payload[7];
            byte gr2 = payload[8];
            byte gr3 = payload[9];
            byte gr4 = payload[10];

            log_e("[HANDSHAKE] Esperado:  0x%02X 0x%02X 0x%02X 0x%02X", r1, r2, r3, r4);
            log_e("[HANDSHAKE] Recibido:  0x%02X 0x%02X 0x%02X 0x%02X", gr1, gr2, gr3, gr4);

            if (gr1 == r1 && gr2 == r2 && gr3 == r3 && gr4 == r4) {
                log_e("[HANDSHAKE] ✅ Verificación correcta.");
            } else {
                log_w("[HANDSHAKE] ⚠️ Verificación incorrecta — aceptando igualmente.");
            }

            byte online_msg[] = {0xF0, 0x00, 0x00, 0x66, 0x14, 0x02, 0xF7};
            sendMIDIBytes(online_msg, sizeof(online_msg));
            handshakeState       = HandshakeState::IDLE;
            logicConnectionState = ConnectionState::MIDI_HANDSHAKE_COMPLETE;
            needsTOTALRedraw     = true;
            log_e("[HANDSHAKE] Handshake completado. -> MIDI_HANDSHAKE_COMPLETE.");
            break;
        }

        // ── Version Request ───────────────────────────────────────────────────
        case 0x13: {
            unsigned long currentTime = millis();
            if (currentTime - lastVersionReplyTime > VERSION_REPLY_COOLDOWN_MS) {
                log_d("<<< Version Request (0x13). Enviando respuesta.");
                byte version_reply[] = {0xF0, 0x00, 0x00, 0x66, 0x14, 0x14,
                                        '1', '.', '2', '.', '0', 0xF7};
                sendMIDIBytes(version_reply, sizeof(version_reply));
                lastVersionReplyTime = currentTime;
            }
            break;
        }

        // ── LCD / Nombres de pista ────────────────────────────────────────────
        case 0x12: {
            if (len < 6) break;
            byte startOffset = payload[5];
            int text_len = len - 6;
            if (text_len <= 0) break;
            for (int i = 0; i < text_len; i++) {
                byte currentOffset = startOffset + i;
                if (currentOffset % 7 != 0) continue;
                if (currentOffset >= 56) continue;
                int track_idx = currentOffset / 7;
                if (track_idx >= 8) continue;
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
            break;
        }

        // ── Assignment Display ────────────────────────────────────────────────
        case 0x11: {
            if (len < 7) break;
            byte b1 = payload[5];
            byte b2 = payload[6];
            char c1 = (b1 >= 32 && b1 <= 126) ? (char)b1 : '?';
            char c2 = (b2 >= 32 && b2 <= 126) ? (char)b2 : '?';
            char assign_buf[3] = {c1, c2, '\0'};
            if (assignmentString != assign_buf) {
                assignmentString = String(assign_buf);
                needsHeaderRedraw = true;
            }
            break;
        }

        // ── VU Meters ─────────────────────────────────────────────────────────
        case 0x72: {
            if (len < 13) break;

            lastMidiActivityTime = millis();

            if (logicConnectionState == ConnectionState::CONNECTED) {
                bool allReset = true;
                for (int i = 0; i < 8; i++) {
                    if (payload[5 + i] != 0x07) { allReset = false; break; }
                }
                if (allReset) {
                    logicConnectionState = ConnectionState::DISCONNECTED;
                    needsTOTALRedraw = true;
                    fadersAtMinMask  = 0;
                    log_e("[DISCONNECT] VU reset 0x07x8 -> DISCONNECTED.");
                    return;
                }
            }

            for (int i = 0; i < 8; i++) {
                byte raw     = payload[5 + i];
                byte channel = raw & 0x0F;
                byte level   = (raw >> 4);
                bool clip    = (raw == 0x0F);
                if (channel < 8) {
                    float normalized = level / 7.0f;
                    if (vuLevels[channel] != normalized) {
                        vuLevels[channel]  = normalized;
                        vuClipState[channel] = clip;
                        needsVUMetersRedraw = true;
                    }
                }
            }
            break;
        }

        // ── Meter Mode ────────────────────────────────────────────────────────
        case 0x0E: {
            log_d("<<< Meter Mode (0x0E): track=%d, mode=%d", payload[5], payload[6]);
            break;
        }

        default:
            log_v("processMackieSysEx: Comando 0x%02X no manejado.", command);
            break;
    }
}

// ****************************************************************************
// Note On/Off — Botones REC/SOLO/MUTE/SELECT
// ****************************************************************************
void processNote(byte status, byte note, byte velocity) {
    bool is_on = ((status & 0xF0) == 0x90 && velocity > 0);

    // --- Modo display ---
    if (note == 113) {
        if (is_on) { currentTimecodeMode = MODE_SMPTE; needsHeaderRedraw = true; }
        return;
    }
    if (note == 114) {
        if (is_on) { currentTimecodeMode = MODE_BEATS; needsHeaderRedraw = true; }
        return;
    }

    // --- PG3: notas Mackie 0-31 ---
    if (note <= 31) {
        int group     = note / 8;
        int track_idx = note % 8;
        bool stateChanged = false;

        switch (group) {
            case 0: if (recStates[track_idx]    != is_on) { recStates[track_idx]    = is_on; stateChanged = true; } break;
            case 1: if (soloStates[track_idx]   != is_on) { soloStates[track_idx]   = is_on; stateChanged = true; } break;
            case 2: if (muteStates[track_idx]   != is_on) { muteStates[track_idx]   = is_on; stateChanged = true; } break;
            case 3: if (selectStates[track_idx] != is_on) { selectStates[track_idx] = is_on; stateChanged = true; } break;
        }

        if (stateChanged) {
            needsMainAreaRedraw = true;
            log_i("<<< PG3 Pista %d grupo %d -> %s", track_idx + 1, group, is_on ? "ON" : "OFF");
        }
        return;
    }

    // --- PG1 y PG2: cada mapa actualiza su propio array independientemente ---
    // IMPORTANTE: una misma nota puede estar en AMBOS mapas (filas compartidas)
    // por eso NO hacemos return al encontrarla en PG1, seguimos buscando en PG2
    bool stateChanged = false;

    for (int key = 0; key < 32; key++) {
        if (MIDI_NOTES_PG1[key] != 0x00 && MIDI_NOTES_PG1[key] == note) {
            if (btnStatePG1[key] != is_on) {
                btnStatePG1[key] = is_on;
                stateChanged = true;
                log_i("<<< PG1 key=%d note=0x%02X -> %s", key, note, is_on ? "ON" : "OFF");
            }
        }
        if (MIDI_NOTES_PG2[key] != 0x00 && MIDI_NOTES_PG2[key] == note) {
            if (btnStatePG2[key] != is_on) {
                btnStatePG2[key] = is_on;
                stateChanged = true;
                log_i("<<< PG2 key=%d note=0x%02X -> %s", key, note, is_on ? "ON" : "OFF");
            }
        }
    }

    if (stateChanged) {
        needsMainAreaRedraw = true;
        updateLeds();
    }
}

// ****************************************************************************
// Pitch Bend — Faders
// ****************************************************************************
void processPitchBend(byte channel, int bendValue) {
    if (channel > 9) return;

    // --- Detección de desconexión ---
    if (bendValue == -8192) {
        if (logicConnectionState == ConnectionState::CONNECTED) {
            unsigned long now = millis();

            if (fadersAtMinMask == 0) firstFaderMinTime = now;

            fadersAtMinMask |= (1 << channel);
            int bitsSet = __builtin_popcount(fadersAtMinMask);

            if (bitsSet >= DISCONNECT_THRESHOLD && 
                (now - firstFaderMinTime) <= DISCONNECT_WINDOW_MS) {
                unsigned long elapsed = now - firstFaderMinTime; // ✅ ANTES de resetear
                logicConnectionState = ConnectionState::DISCONNECTED;
                needsTOTALRedraw = true;
                fadersAtMinMask = 0;
                firstFaderMinTime = 0;
                log_e("[DISCONNECT] %d faders en -8192 en %lums -> DISCONNECTED.", bitsSet, elapsed);
                return;
            }

            if ((now - firstFaderMinTime) > DISCONNECT_WINDOW_MS) {
                fadersAtMinMask = (1 << channel);
                firstFaderMinTime = now;
            }
        }
    } else {
        fadersAtMinMask &= ~(1 << channel);
    }

    // --- Transición HANDSHAKE_COMPLETE → CONNECTED ---
    if (logicConnectionState == ConnectionState::MIDI_HANDSHAKE_COMPLETE) {
        logicConnectionState = ConnectionState::CONNECTED;
        needsTOTALRedraw = true;
        fadersAtMinMask = 0;
        log_e("DAW conectado: Primer PitchBend Track %d -> CONNECTED.", channel + 1);
    }

    // --- Actualizar posición del fader ---
    if (channel < 9) {
        float faderPositionNormalized = (float)(bendValue + 8192) / 16383.0f;
        if (abs(faderPositions[channel] - faderPositionNormalized) > 0.001f) {
            faderPositions[channel] = faderPositionNormalized;
            needsMainAreaRedraw = true;
        }
    }
}

// Función pública para llamar desde loop():
void checkMidiTimeout() {
    if (logicConnectionState == ConnectionState::CONNECTED) {
        if (millis() - lastMidiActivityTime > MIDI_TIMEOUT_MS) {
            logicConnectionState = ConnectionState::DISCONNECTED;
            needsTOTALRedraw = true;
            fadersAtMinMask = 0;
            log_e("[TIMEOUT] Sin MIDI por %lums -> DISCONNECTED.", 
                  millis() - lastMidiActivityTime);
        }
    }
}
bool isLogicConnected() {
    return (logicConnectionState == ConnectionState::CONNECTED);
}