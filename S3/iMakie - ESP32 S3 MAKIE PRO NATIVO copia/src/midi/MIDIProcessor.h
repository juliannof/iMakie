// src/midi/MIDIProcessor.h
#pragma once
#include <Arduino.h>

// ✅ NUEVO: Reemplaza sendToPico() — envía bytes MIDI crudos por USB
void sendMIDIBytes(const byte* data, size_t len);

// Parser byte a byte (sin cambios)
void processMidiByte(byte b);

// Procesadores de mensajes MIDI específicos
void processMackieSysEx(byte* payload, int len);
void processNote(byte status, byte note, byte velocity);
void handleMcuHandshake(byte* challenge_code);
void processChannelPressure(byte channel, byte value);
void processControlChange(byte channel, byte controller, byte value);
void processPitchBend(byte channel, int bendValue);

// Formateo del display de tiempo
String formatBeatString();
String formatTimecodeString();