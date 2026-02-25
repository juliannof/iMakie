#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

Adafruit_USBD_MIDI usb_midi;

// Buffer para mensajes SysEx
const int SYSEX_BUFFER_SIZE = 256;
uint8_t sysexBuffer[SYSEX_BUFFER_SIZE];
int sysexIndex = 0;


void processSysExMessage(uint8_t* message, int length) {
  if (length >= 7) {
    Serial.print("SysEx Message: ");
    for (int i = 0; i < length; i++) {
      Serial.print(message[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    // Ejemplo de procesamiento de datos específicos
    if (message[4] == 0x10) {  // Puedes cambiar esto según el mensaje específico
      int value = message[5];  // Extraer el valor
      Serial.print("Value: ");
      Serial.println(value);
    }
  }
}


void setup() {
  Serial.begin(115200);
  usb_midi.begin();  // Iniciar MIDI USB
}

void loop() {
  while (usb_midi.available()) {
    uint8_t byte = usb_midi.read();  // Leer el byte MIDI entrante

    if (byte == 0xF0) {  // Start of SysEx
      sysexIndex = 0;  // Resetear el índice del buffer
      sysexBuffer[sysexIndex++] = byte;
    }
    else if (byte == 0xF7 && sysexIndex > 0) {  // End of SysEx
      sysexBuffer[sysexIndex++] = byte;
      // Procesar el mensaje SysEx
      processSysExMessage(sysexBuffer, sysexIndex);
      sysexIndex = 0;  // Resetear el índice del buffer
    }
    else if (sysexIndex > 0) {  // Acumular bytes de SysEx
      if (sysexIndex < SYSEX_BUFFER_SIZE) {
        sysexBuffer[sysexIndex++] = byte;
      }
      else {
        // Buffer overflow, resetear
        sysexIndex = 0;
      }
    }
  }
}

