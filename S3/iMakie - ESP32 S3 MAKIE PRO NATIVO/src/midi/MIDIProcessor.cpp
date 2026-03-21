// src/midi/MIDIProcessor.cpp
#include "MIDIProcessor.h"
#include "../config.h"
#include <USBMIDI.h>
#include "../RS485/RS485.h"   // ← RS485 NUEVO

/* =========================================================
   ACCESO AL OBJETO MIDI (definido en main.cpp)
   ========================================================= */
extern USBMIDI MIDI;
extern void updateLeds();
extern bool btnStatePG1[32];
extern bool btnStatePG2[32];
extern uint8_t g_logicConnected;


// --- Variables internas del parser MIDI ---
namespace {
    byte midi_buffer[512];
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
    static const uint16_t ALL_FADERS_MIN_MASK = 0x01FF;
    static unsigned long lastMidiActivityTime = 0;
    
    static const unsigned long MIDI_TIMEOUT_MS = 28000;
    static const int DISCONNECT_THRESHOLD = 9;
    static const unsigned long DISCONNECT_WINDOW_MS = 150;

    unsigned long lastVersionReplyTime = 0;
    const unsigned long VERSION_REPLY_COOLDOWN_MS = 200;
    // Añadir junto a las otras variables estáticas del namespace
    static int8_t  g_selectedChannel    = -1;      // canal activo (0-7)
    static uint8_t g_channelAutoMode[8] = {};       // AutoMode por canal
    static unsigned long connectedSinceTime  = 0;
    static const unsigned long CONNECT_GRACE_MS = 1500;
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
   sendMIDIBytes()
   ========================================================= */
void sendMIDIBytes(const byte* data, size_t len) {
    log_v("[MIDI OUT] Enviando %d bytes", len);

    if (data[0] == 0xF0) {
        size_t i = 0;
        while (i < len) {
            midiEventPacket_t packet;
            size_t remaining = len - i;

            if (remaining >= 3 && (i + 3) < len) {
                packet.header = 0x04;
                packet.byte1  = data[i];
                packet.byte2  = data[i+1];
                packet.byte3  = data[i+2];
                i += 3;
            } else if (remaining == 1) {
                packet.header = 0x05;
                packet.byte1  = data[i];
                packet.byte2  = 0x00;
                packet.byte3  = 0x00;
                i += 1;
            } else if (remaining == 2) {
                packet.header = 0x06;
                packet.byte1  = data[i];
                packet.byte2  = data[i+1];
                packet.byte3  = 0x00;
                i += 2;
            } else {
                packet.header = 0x07;
                packet.byte1  = data[i];
                packet.byte2  = data[i+1];
                packet.byte3  = data[i+2];
                i += 3;
            }
            MIDI.writePacket(&packet);
        }
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
            case 0xE0: {
                // Pitch Bend — MIDI.writePacket directo
                midiEventPacket_t packet;
                packet.header = 0x0E | ((data[0] & 0x0F) << 4); // cable 0
                packet.byte1  = data[0];
                packet.byte2  = data[1];
                packet.byte3  = data[2];
                MIDI.writePacket(&packet);
                break;
            }
            default:
                log_d("[MIDI OUT] Mensaje no soportado: 0x%02X", status);
                break;
        }
    }
}


// ****************************************************************************
// processMidiByte — sin cambios
// ****************************************************************************
void processMidiByte(byte b) {
    if (b >= 0xF8) return;

    if (b & 0x80) {
        if (b == 0xF0) {
            in_sysex = true;
            midi_idx = 0;
            return;
        }
        if (b == 0xF7) {
            if (in_sysex) {
                in_sysex = false;
                processMackieSysEx(midi_buffer, midi_idx);
            }
            return;
        }
        if (in_sysex) {
            in_sysex = false;
            if (handshakeState == HandshakeState::AWAITING_CHALLENGE_BYTES)
                handshakeState = HandshakeState::IDLE;
        }
        last_status_byte = b;
        midi_idx = 0;
        return;
    }

    if (in_sysex) {
        if (handshakeState == HandshakeState::AWAITING_CHALLENGE_BYTES) {
            if (challenge_idx < 7) {
                challenge_buffer[challenge_idx++] = b;
                if (challenge_idx == 7) {
                    handleMcuHandshake(challenge_buffer);
                    handshakeState = HandshakeState::IDLE;
                }
            }
        }
        if (midi_idx < (int)sizeof(midi_buffer)) {
            midi_buffer[midi_idx++] = b;
        }
        return;
    }

    if (last_status_byte != 0) {
        if (midi_idx >= 250) midi_idx = 0;
        midi_buffer[midi_idx++] = b;

        byte cmd_type = last_status_byte & 0xF0;
        int msg_len_expected = 0;

        switch (cmd_type) {
            case 0xC0: case 0xD0: msg_len_expected = 1; break;
            case 0xF0: msg_len_expected = 0; break;
            default:   msg_len_expected = 2; break;
        }

        if (midi_idx == msg_len_expected) {
            byte data1 = midi_buffer[0];
            byte data2 = (msg_len_expected > 1) ? midi_buffer[1] : 0;

            switch (cmd_type) {
                case 0x90: case 0x80:
                    processNote(last_status_byte, data1, data2);
                    break;
                case 0xD0:
                    processChannelPressure(last_status_byte & 0x0F, data1);
                    break;
                case 0xB0:
                    processControlChange(last_status_byte & 0x0F, data1, data2);
                    break;
                case 0xE0: {
                    int bendValue = (data2 << 7) | data1;
                    processPitchBend(last_status_byte & 0x0F, bendValue);
                    break;
                }
                default: break;
            }
            midi_idx = 0;
        }
    }
}


// ****************************************************************************
// handleMcuHandshake — sin cambios
// ****************************************************************************
void handleMcuHandshake(byte* challenge_code) {
    ConnectionState previousState = logicConnectionState;

    byte response[15] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x01, 0x00,
                          challenge_code[0], challenge_code[1], challenge_code[2],
                          challenge_code[3], challenge_code[4], challenge_code[5],
                          challenge_code[6], 0xF7 };
    sendMIDIBytes(response, sizeof(response));

    byte online_msg[] = {0xF0, 0x00, 0x00, 0x66, 0x14, 0x02, 0xF7};
    sendMIDIBytes(online_msg, sizeof(online_msg));

    if (previousState != ConnectionState::CONNECTED) {
        logicConnectionState = ConnectionState::MIDI_HANDSHAKE_COMPLETE;
        needsTOTALRedraw = true;
    }
}


// ****************************************************************************
// processControlChange — sin cambios
// ****************************************************************************
void processControlChange(byte channel, byte controller, byte value) {
    log_d("CC CH=%d, CC=%d, Val=0x%02X", channel, controller, value);

    if (channel != 0 && channel != 15) return;  // solo filtro de canal

    // VPot LED Ring: CC 48-55 → strips 1-8
    if (controller >= 48 && controller <= 55) {
        uint8_t strip = controller - 48;
        rs485.setVPotValue(strip + 1, value);
        log_v("[VPot] strip=%u raw=0x%02X mode=%u pos=%u center=%u",
              strip, value,
              (value >> 4) & 0x03,
              value & 0x0F,
              (value >> 6) & 0x01);
        return;
    }

    // Timecode digits: CC 64-73
    if (controller < 64 || controller > 73) return;

    int digit_index  = 73 - controller;
    byte char_code   = value & 0x3F;
    char ascii_char  = (char_code < 64) ? MACKIE_CHAR_MAP[char_code] : '?';
    byte char_to_store = (byte)ascii_char;
    if (value & 0x40) char_to_store |= 0x80;

    beatsChars_clean[digit_index]    = char_to_store;
    timeCodeChars_clean[digit_index] = char_to_store;

    needsHeaderRedraw = true;
}

// ****************************************************************************
// Helper para formatear la cadena de timecode a mostrar en pantalla
// ****************************************************************************

String formatTimecodeString() {
    char formatted[14];
    int pos = 0;
    for (int i = 0; i < 10; i++) {
        byte b = timeCodeChars_clean[i];
        char c = b & 0x7F;
        if (c == 0 || c < 32) c = ':';
        if (c == ';') c = ':';
        formatted[pos++] = c;
        if (b & 0x80) formatted[pos++] = ':';
    }
    formatted[pos] = '\0';
    String result = String(formatted);
    result.trim();
    return (result.length() == 0) ? "--:--:--:--" : result;
}

// ****************************************************************************
// Helper para formatear la cadena de beats a mostrar en pantalla
// ****************************************************************************

String formatBeatString() {
    char formatted[14];
    int pos = 0;
    for (int i = 0; i < 10; i++) {
        byte b = beatsChars_clean[i];
        char c = b & 0x7F;
        if (c == 0 || c < 32) c = '.';
        if (c == ';') c = '.';
        formatted[pos++] = c;
        if (b & 0x80) formatted[pos++] = '.';
    }
    formatted[pos] = '\0';
    String result = String(formatted);
    result.trim();
    return (result.length() == 0) ? "  1.  1.  1.  1" : result;
}


// ****************************************************************************
// processChannelPressure — ← RS485: enviar VU al slave
// ****************************************************************************
void processChannelPressure(byte channel, byte value) {
    log_v(">> CP IN: Ch=%d, Val=%d", channel, value);

    float normalizedLevel = 0.0f;
    int targetChannel = -1;
    bool newClipState = false;
    bool clearClip = false;
    uint8_t vuLevel7bit = 0;  // ← RS485 NUEVO

    if (channel == 0) {
        targetChannel = (value >> 4) & 0x0F;
        byte mcu_level = value & 0x0F;

        if (targetChannel >= 8) return;

        switch (mcu_level) {
            case 0x0F: clearClip = true; normalizedLevel = vuLevels[targetChannel]; break;
            case 0x0E: newClipState = true; normalizedLevel = 1.0f; vuLevel7bit = 127; break;
            case 0x0D: case 0x0C: normalizedLevel = 1.0f; vuLevel7bit = 120; break;
            default:
                normalizedLevel = (mcu_level <= 11) ? (float)mcu_level / 11.0f : 0.0f;
                vuLevel7bit = (uint8_t)(normalizedLevel * 127.0f);  // ← RS485 NUEVO
                break;
        }

        // ← RS485 NUEVO: enviar VU al slave correspondiente (1-indexed)
        rs485.setVuLevel(targetChannel + 1, vuLevel7bit);

    } else if (channel >= 1 && channel <= 7) {
        targetChannel = channel;
        normalizedLevel = (float)value / 127.0f;
        if (value >= 127) newClipState = true;
        rs485.setVuLevel(targetChannel + 1, value);  // ← RS485 NUEVO
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


// ****************************************************************************
// processMackieSysEx — ← RS485: enviar trackName al slave
// ****************************************************************************
void processMackieSysEx(byte* payload, int len) {
    if (len < 5) return;

    byte device_family = payload[3];
    byte command = payload[4];

    if (device_family != 0x14 && device_family != 0x15) return;

    if (command == 0x00 && len == 5) {
        if (logicConnectionState == ConnectionState::CONNECTED) {
            fadersAtMinMask   = 0;
            firstFaderMinTime = 0;
            lastMidiActivityTime = millis();
            byte online_msg[] = {0xF0, 0x00, 0x00, 0x66, 0x14, 0x02, 0xF7};
            sendMIDIBytes(online_msg, sizeof(online_msg));
            return;
        }

        byte l1 = random(0x01, 0x7F);
        byte l2 = random(0x01, 0x7F);
        byte l3 = random(0x01, 0x7F);
        byte l4 = random(0x01, 0x7F);
        challenge_buffer[0] = l1; challenge_buffer[1] = l2;
        challenge_buffer[2] = l3; challenge_buffer[3] = l4;

        byte response[15] = {0xF0, 0x00, 0x00, 0x66, 0x14, 0x01, 0x00,
                             l1, l2, l3, l4, 0x00, 0x00, 0x00, 0xF7};
        sendMIDIBytes(response, sizeof(response));
        handshakeState = HandshakeState::AWAITING_CHALLENGE_BYTES;
        challenge_idx  = 0;
        return;
    }

    switch (command) {

        case 0x01: {
            if (len < 12) break;
            byte l1 = challenge_buffer[0], l2 = challenge_buffer[1];
            byte l3 = challenge_buffer[2], l4 = challenge_buffer[3];
            byte r1 = 0x7F & (l1 + (l2 ^ 0x0a) - l4);
            byte r2 = 0x7F & ((l3 >> 4) ^ (l1 + l4));
            byte r3 = 0x7F & (l4 - (l3 << 2) ^ (l1 | l2));
            byte r4 = 0x7F & (l2 - l3 + (0xF0 ^ (l4 << 4)));
            (void)r1; (void)r2; (void)r3; (void)r4;  // verificación opcional

            byte online_msg[] = {0xF0, 0x00, 0x00, 0x66, 0x14, 0x02, 0xF7};
            sendMIDIBytes(online_msg, sizeof(online_msg));
            handshakeState       = HandshakeState::IDLE;
            logicConnectionState = ConnectionState::MIDI_HANDSHAKE_COMPLETE;
            needsTOTALRedraw     = true;
            break;
        }

        case 0x13: {
            unsigned long now = millis();
            if (now - lastVersionReplyTime > VERSION_REPLY_COOLDOWN_MS) {
                byte version_reply[] = {0xF0, 0x00, 0x00, 0x66, 0x14, 0x14,
                        '1', '.', '2', '.', '0', 0xF7};
                sendMIDIBytes(version_reply, sizeof(version_reply));
                lastVersionReplyTime = now;
            }
            break;
        }

        // ← RS485 NUEVO: enviar trackName al slave
        case 0x12: {
    if (len < 6) break;
    byte startOffset = payload[5];
    int  text_len    = len - 6;
    if (text_len <= 0) break;

    // Buffers temporales inicializados con los nombres actuales
    char nameBufs[8][8] = {};
    bool nameChanged[8] = {};
    for (int t = 0; t < 8; t++) {
        strncpy(nameBufs[t], trackNames[t].c_str(), 7);
        nameBufs[t][7] = '\0';
    }

    for (int i = 0; i < text_len; i++) {
        byte currentOffset = startOffset + i;
        if (currentOffset >= 56) break;          // 8 pistas × 7 chars = 56

        int track_idx = currentOffset / 7;       // qué pista
        int char_pos  = currentOffset % 7;       // qué carácter dentro del nombre

        if (track_idx >= 8) break;

        nameBufs[track_idx][char_pos] = (char)payload[6 + i];
        nameChanged[track_idx] = true;
    }

    for (int t = 0; t < 8; t++) {
        if (!nameChanged[t]) continue;

        // Trim espacios finales
        for (int j = 6; j >= 0; j--) {
            if (nameBufs[t][j] == ' ' || nameBufs[t][j] == '\0') nameBufs[t][j] = '\0';
            else break;
        }

        if (trackNames[t] != nameBufs[t]) {
            trackNames[t] = String(nameBufs[t]);
            needsMainAreaRedraw = true;
        }
        rs485.setTrackName(t + 1, nameBufs[t]);
    }
    break;
}

        case 0x11: {
            if (len < 7) break;
            byte b1 = payload[5], b2 = payload[6];
            char c1 = (b1 >= 32 && b1 <= 126) ? (char)b1 : '?';
            char c2 = (b2 >= 32 && b2 <= 126) ? (char)b2 : '?';
            char assign_buf[3] = {c1, c2, '\0'};
            if (assignmentString != assign_buf) {
                assignmentString = String(assign_buf);
                needsHeaderRedraw = true;
            }
            break;
        }

        case 0x72: {
            if (len < 13) break;
            lastMidiActivityTime = millis();
            for (int i = 0; i < 8; i++) {
                byte raw     = payload[5 + i];
                byte channel = raw & 0x0F;
                byte level   = (raw >> 4);
                bool clip    = (raw == 0x0F);
                if (channel < 8) {
                    float normalized = level / 7.0f;
                    if (vuLevels[channel] != normalized) {
                        vuLevels[channel]    = normalized;
                        vuClipState[channel] = clip;
                        needsVUMetersRedraw  = true;
                    }
                    // ← RS485 NUEVO
                    rs485.setVuLevel(channel + 1, (uint8_t)(normalized * 127.0f));
                }
            }
            break;
        }

        case 0x0E:
            break;

        default:
            log_v("processMackieSysEx: Comando 0x%02X no manejado.", command);
            break;
    }
}


// ****************************************************************************
// processNote — ← RS485: enviar flags al slave
// ****************************************************************************
void processNote(byte status, byte note, byte velocity) {
    bool is_on = ((status & 0xF0) == 0x90 && velocity > 0);

    if (note == 113) { if (is_on) { currentTimecodeMode = MODE_SMPTE; needsHeaderRedraw = true; } return; }
    if (note == 114) { if (is_on) { currentTimecodeMode = MODE_BEATS; needsHeaderRedraw = true; } return; }

    if (note <= 31) {
        int group     = note / 8;
        int track_idx = note % 8;
        bool stateChanged = false;

        switch (group) {
            case 0: if (recStates[track_idx]    != is_on) { recStates[track_idx]    = is_on; stateChanged = true; } break;
            case 1: if (soloStates[track_idx]   != is_on) { soloStates[track_idx]   = is_on; stateChanged = true; } break;
            case 2: if (muteStates[track_idx]   != is_on) { muteStates[track_idx]   = is_on; stateChanged = true; } break;
            case 3:
                if (selectStates[track_idx] != is_on) { selectStates[track_idx] = is_on; stateChanged = true; }
                if (is_on) g_selectedChannel = track_idx;
                else if (g_selectedChannel == track_idx) g_selectedChannel = -1;
                break;
        }

        if (stateChanged) {
            needsMainAreaRedraw = true;

            uint8_t slaveId = track_idx + 1;
            uint8_t flags = 0;
            if (recStates[track_idx])    flags |= FLAG_REC;
            if (soloStates[track_idx])   flags |= FLAG_SOLO;
            if (muteStates[track_idx])   flags |= FLAG_MUTE;
            if (selectStates[track_idx]) flags |= FLAG_SELECT;
            flags = setAutoMode(flags, (AutoMode)g_channelAutoMode[track_idx]);
            rs485.setFlags(slaveId, flags);
        }
        return;
    }

    if (note >= 74 && note <= 79 && is_on && g_selectedChannel >= 0) {
        const AutoMode modeMap[] = {
            AUTO_READ, AUTO_WRITE, AUTO_TRIM, AUTO_TOUCH, AUTO_LATCH, AUTO_OFF
        };
        AutoMode mode = modeMap[note - 74];
        g_channelAutoMode[g_selectedChannel] = (uint8_t)mode;
        rs485.setAutoMode(g_selectedChannel + 1, mode);

        // ← Actualizar botones en display y trellis
        for (int key = 0; key < 32; key++) {
            if (MIDI_NOTES_PG1[key] != 0x00 && MIDI_NOTES_PG1[key] == note) {
                btnStatePG1[key] = is_on;
        }
    }
    needsMainAreaRedraw = true;
    return;
    }

    bool stateChanged = false;
    for (int key = 0; key < 32; key++) {
        if (MIDI_NOTES_PG1[key] != 0x00 && MIDI_NOTES_PG1[key] == note) {
            if (btnStatePG1[key] != is_on) { btnStatePG1[key] = is_on; stateChanged = true; }
        }
        if (MIDI_NOTES_PG2[key] != 0x00 && MIDI_NOTES_PG2[key] == note) {
            if (btnStatePG2[key] != is_on) { btnStatePG2[key] = is_on; stateChanged = true; }
        }
    }
    if (stateChanged) needsMainAreaRedraw = true;
}


// ****************************************************************************
// processPitchBend — ← RS485: enviar faderTarget al slave
// ****************************************************************************
void processPitchBend(byte channel, int bendValue) {
    log_e("PB ch%d raw:%d", channel, bendValue);

    if (channel > 9) return;

    // --- Detección de desconexión: fader mínimo = 0 ---
    if (bendValue == 0) {
        if (logicConnectionState == ConnectionState::CONNECTED) {
            // Grace period — ignorar faders a 0 justo tras conectar
            if (millis() - connectedSinceTime < CONNECT_GRACE_MS) return;

            unsigned long now = millis();
            if (fadersAtMinMask == 0) firstFaderMinTime = now;
            fadersAtMinMask |= (1 << channel);
            int bitsSet = __builtin_popcount(fadersAtMinMask);
            if (bitsSet >= DISCONNECT_THRESHOLD &&
                (now - firstFaderMinTime) <= DISCONNECT_WINDOW_MS) {
                unsigned long elapsed = now - firstFaderMinTime;
                logicConnectionState = ConnectionState::DISCONNECTED;
                g_logicConnected = 0;
                needsTOTALRedraw = true;
                fadersAtMinMask = 0;
                firstFaderMinTime = 0;
                log_d("[DISCONNECT] %d faders en 0 en %lums.", bitsSet, elapsed);
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
        g_logicConnected     = 1;
        connectedSinceTime   = millis();
        needsTOTALRedraw     = true;
        fadersAtMinMask      = 0;
        log_d("DAW conectado: Primer PitchBend Track %d -> CONNECTED.", channel + 1);
    }

    // --- Actualizar posición fader ---
    if (channel < 9) {
        uint16_t fader14bit = (uint16_t)bendValue;

        if (channel < 8) {
            rs485.setFaderTarget(channel + 1, fader14bit);
        }

        float faderPositionNormalized = (float)fader14bit / 16383.0f;
        if (abs(faderPositions[channel] - faderPositionNormalized) > 0.001f) {
            faderPositions[channel] = faderPositionNormalized;
            needsMainAreaRedraw = true;
        }
    }
}

// ****************************************************************************
// helper para detectar inactividad MIDI y desconexión lógica — sin cambios
// ****************************************************************************

void checkMidiTimeout() {
    if (logicConnectionState == ConnectionState::CONNECTED) {
        if (millis() - lastMidiActivityTime > MIDI_TIMEOUT_MS) {
            logicConnectionState = ConnectionState::DISCONNECTED;
            needsTOTALRedraw = true;
            fadersAtMinMask = 0;
        }
    }
}
// ****************************************************************************
// helper para verificar estado de conexión lógica — sin cambios
// ****************************************************************************

bool isLogicConnected() {
    return (logicConnectionState == ConnectionState::CONNECTED);
}