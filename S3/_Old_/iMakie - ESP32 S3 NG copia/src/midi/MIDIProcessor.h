#pragma once
#include <Arduino.h>

void processMidiByte(byte b);
void onHostQueryDetected(); 

void processMackieSysEx(byte* payload, int len);
void processNote(byte status, byte note, byte velocity);
void handleMcuHandshake(byte* challenge_code);
void processChannelPressure(byte channel, byte value);
void processControlChange(byte channel, byte controller, byte value);
void processPitchBend(byte channel, int bendValue);

String formatBeatString();
String formatTimecodeString();
