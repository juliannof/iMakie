#include <Arduino.h>
#include <MIDI.h>

MIDI_CREATE_DEFAULT_INSTANCE();

void setup() {
  Serial.begin(115200);
  MIDI.begin(MIDI_CHANNEL_OMNI);  // Escuchar mensajes en todos los canales
}

void loop() {
  // Leer los mensajes MIDI
  if (MIDI.read()) {
    byte type = MIDI.getType();
    if (type == midi::ControlChange) {
      byte control = MIDI.getData1();
      byte value = MIDI.getData2();
      Serial.print("Control Change: Control ");
      Serial.print(control);
      Serial.print(", Value ");
      Serial.println(value);
    }
  }
  delay(10);
}