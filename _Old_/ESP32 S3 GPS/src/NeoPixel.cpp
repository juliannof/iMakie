#include "NeoPixel.h"

NeoPixel::NeoPixel(int pin, int numLeds) : strip(numLeds, pin, NEO_GRB + NEO_KHZ800) {}

void NeoPixel::initialize() {
    strip.begin();
    clear(); // Apagar todos los LEDs al inicio
}

void NeoPixel::setColorRed() {
    for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.Color(255, 0, 0)); // Rojo
    }
    strip.show();
}

void NeoPixel::setColorGreen() {
    for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.Color(0, 10, 0)); // Verde
    }
    strip.show();
}

void NeoPixel::setColorBlue() {
    for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 10)); // Azul
    }
    strip.show();
}

void NeoPixel::clear() {
    for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 0)); // Apagar
    }
    strip.show();
}