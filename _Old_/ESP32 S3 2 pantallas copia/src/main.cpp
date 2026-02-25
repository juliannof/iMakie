#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

// Descriptor de informe HID para un teclado
const uint8_t HID_KEYBOARD_REPORT_DESC[] = {
  TUD_HID_REPORT_DESC_KEYBOARD()
};

Adafruit_USBD_HID usb_hid;

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando USB HID!");

  // Configurar el dispositivo HID USB
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(HID_KEYBOARD_REPORT_DESC, sizeof(HID_KEYBOARD_REPORT_DESC));
  usb_hid.begin();

  // Esperar a que se monte el dispositivo USB
  while (!TinyUSBDevice.mounted()) {
    delay(1);
  }
}

void loop() {
  // Verificar si el dispositivo está listo para enviar datos
  if (usb_hid.ready()) {
    uint8_t keycode[6] = { HID_KEY_A, 0, 0, 0, 0, 0 }; // Enviar 'A'
    usb_hid.keyboardReport(0, 0, keycode);
    delay(100);
    usb_hid.keyboardRelease(0);
  }
  delay(1000); // Esperar un segundo antes de enviar nuevamente
}