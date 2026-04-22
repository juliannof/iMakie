#include "Transporte.h"
#include "../midi/MIDIProcessor.h"
#include "../config.h"

namespace Transporte {

static const uint8_t LEDS[] = { LED_REC, LED_PLAY, LED_FF, LED_STOP, LED_RW };
static const uint8_t BTNS[] = { BTN_REC, BTN_PLAY, BTN_FF, BTN_STOP, BTN_RW };
static const uint8_t N = 5;

static Button2 buttons[5] = {
    Button2(BTN_REC),
    Button2(BTN_PLAY),
    Button2(BTN_FF),
    Button2(BTN_STOP),
    Button2(BTN_RW)
};

// Notas MCU transporte — igual que MIDI_NOTES_PG1 del S3-01
static const uint8_t MIDI_NOTES[] = {
    0x5F, // REC
    0x5E, // PLAY
    0x5C, // FF
    0x5D, // STOP
    0x5B  // RW
};

void setLed(uint8_t pin, bool on) {
    digitalWrite(pin, on ? LOW : HIGH); // ánodo común 5V, sink
}

static void onButtonPressed(Button2& b) {
    for (uint8_t i = 0; i < N; i++) {
        if (&buttons[i] == &b) {
            byte msg[3] = { 0x90, MIDI_NOTES[i], 0x7F };
            sendMIDIBytes(msg, 3);
            byte msgOff[3] = { 0x90, MIDI_NOTES[i], 0x00 };
            sendMIDIBytes(msgOff, 3);
            return;
        }
    }
}

void begin() {
    for (uint8_t i = 0; i < N; i++) {
        pinMode(LEDS[i], OUTPUT);
        setLed(LEDS[i], false);
        buttons[i].setPressedHandler(onButtonPressed);
    }

    // Secuencia de encendido
    for (uint8_t i = 0; i < N; i++) {
        setLed(LEDS[i], true);
        delay(150);
        setLed(LEDS[i], false);
    }

    delay(50); // estabilizar pull-ups
}

void setLedByNote(uint8_t note, bool on) {
    for (uint8_t i = 0; i < N; i++) {
        if (MIDI_NOTES[i] == note) {
            setLed(LEDS[i], on);
            return;
        }
    }
}

void update() {
    for (uint8_t i = 0; i < N; i++) {
        buttons[i].loop();
    }
}

} // namespace Transporte