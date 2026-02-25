#include <Arduino.h>
// NeoPixel Ring simple sketch (c) 2013 Shae Erisson
// Released under the GPLv3 license to match the rest of the
// Adafruit NeoPixel library

#include <Adafruit_NeoPixel.h>

// Which pin on the Arduino is connected to the NeoPixels?
#define PIN        5// On Trinket or Gemma, suggest changing this to 1

// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS 4 // Popular NeoPixel ring size

// When setting up the NeoPixel library, we tell it how many pixels,
// and which pin to use to send signals. Note that for older NeoPixel
// strips you might need to change the third parameter -- see the
// strandtest example for more information on possible values.
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

#define DELAYVAL 500 // Time (in milliseconds) to pause between pixels

void setup() {

  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
}

void loop() {
  pixels.clear(); // Set all pixel colors to 'off'

  for(int i=0; i<NUMPIXELS; i++) { // Para cada pixel...
    switch(i % 3) {
      case 0:
        pixels.setPixelColor(i, pixels.Color(50, 0, 0)); // Rojo
        break;
      case 1:
        pixels.setPixelColor(i, pixels.Color(0, 50, 0)); // Verde
        break;
      case 2:
        pixels.setPixelColor(i, pixels.Color(0, 0, 50)); // Azul
        break;
    }
    pixels.show();   // Enviar los colores de los pixeles actualizados al hardware.
    delay(DELAYVAL); // Esperar antes de pasar al siguiente paso en el bucle.
  }
}
