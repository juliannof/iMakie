#include <Arduino.h>
#include <USB.h>
#include <USBMIDI.h>

USBMIDI MidiUSB;

void setup() {
    Serial.begin(115200);
    MidiUSB.begin();
    USB.begin();
    Serial.println("iMakie P4 MIDI OK");
}

void loop() {
    uint8_t rx[64];
    uint32_t n = tud_midi_stream_read(rx, sizeof(rx));
    if (n > 0) {
        Serial.printf("MIDI IN: %d bytes\n", n);
        for (uint32_t i = 0; i < n; i++) {
            Serial.printf("%02X ", rx[i]);
        }
        Serial.println();
    }
}