#include <Arduino.h>
#include <USB.h>
#include <USBMIDI.h>

/* =========================================================
   CONFIG LOG
   ========================================================= */
#define LOG_ENABLED 1

#if LOG_ENABLED
  #define LOG(tag, msg) Serial.println(String("[") + tag + "] " + msg)
#else
  #define LOG(tag, msg)
#endif

USBMIDI MIDI;

/* =========================================================
   PROTOTIPOS
   ========================================================= */
void sendTestMessages();
void sendFadersToZero();
void sendSysEx(const uint8_t *data, size_t len);

/* =========================================================
   SETUP
   ========================================================= */
void setup() {
  Serial.begin(115200);
  delay(300);

  LOG("BOOT", "ESP32-S3 arrancando");
  LOG("BOOT", "Inicializando USB stack");

  USB.begin();
  delay(200);

  LOG("USB", "USB.begin() ejecutado");

  MIDI.begin();
  LOG("MIDI", "USB MIDI iniciado");

  LOG("STATE", "Esperando enumeracion del host");
  delay(3000);   // CLAVE: esperar a macOS / Logic

  LOG("STATE", "Enviando mensajes de prueba");
  sendTestMessages();

  LOG("STATE", "Inicializando faders a 0 dB");
  sendFadersToZero();

  LOG("BOOT", "Setup finalizado");
}

/* =========================================================
   LOOP
   ========================================================= */
void loop() {
  static uint32_t last = 0;

  if (millis() - last > 3000) {
    LOG("STATE", "Firmware activo");
    last = millis();
  }
}

/* =========================================================
   MENSAJES MIDI DE PRUEBA
   ========================================================= */
void sendTestMessages() {
  LOG("MIDI", "NOTE ON  C3 ch=1 vel=100");
  MIDI.noteOn(60, 100, 1);
  delay(100);

  LOG("MIDI", "NOTE OFF C3 ch=1");
  MIDI.noteOff(60, 0, 1);
  delay(100);

  LOG("MIDI", "CC7 Volume = 100 ch=1");
  MIDI.controlChange(7, 100, 1);
  delay(50);

  LOG("MIDI", "Channel Pressure = 80 ch=1");
  MIDI.channelPressure(80, 1);
  delay(50);

  LOG("MIDI", "Pitch Bend centro (8192) ch=1");
  MIDI.pitchBend((int16_t)8192, 1);
  delay(50);

  /* -------- SysEx corto -------- */
  const uint8_t sysexShort[] = {0xF0, 0x7D, 0x01, 0x02, 0x03, 0xF7};
  sendSysEx(sysexShort, sizeof(sysexShort));

  delay(100);

  /* -------- SysEx largo -------- */
  const uint8_t sysexLong[] = {
    0xF0, 0x7D,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0xF7
  };
  sendSysEx(sysexLong, sizeof(sysexLong));
}

/* =========================================================
   SYSEX BYTE A BYTE (USBMIDI 3.1.1)
   ========================================================= */
void sendSysEx(const uint8_t *data, size_t len) {
  LOG("SYSEX", "Enviando SysEx");

  Serial.print("[SYSEX] DATA: ");
  for (size_t i = 0; i < len; i++) {
    Serial.print("0x");
    if (data[i] < 16) Serial.print("0");
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  for (size_t i = 0; i < len; i++) {
    MIDI.write(data[i]);   // UNICO METODO VALIDO
  }

  LOG("SYSEX", "SysEx enviado");
}

/* =========================================================
   FADERS A 0 dB (LOGIC = PITCH BEND CENTRO)
   ========================================================= */
void sendFadersToZero() {
  for (uint8_t ch = 1; ch <= 8; ch++) {
    LOG("FADER", String("Fader CH ") + ch + " -> 8192");
    MIDI.pitchBend((int16_t)8192, ch);
    delay(10);
  }

  LOG("FADER", "Todos los faders inicializados");
}
