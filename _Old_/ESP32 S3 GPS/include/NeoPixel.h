#ifndef NEOPIXEL_H
#define NEOPIXEL_H

#include <Adafruit_NeoPixel.h>

class NeoPixel {
public:
    NeoPixel(int pin, int numLeds);
    void initialize();
    void setColorRed();
    void setColorGreen();
    void setColorBlue();
    void clear();

private:
    Adafruit_NeoPixel strip;
};

#endif // NEOPIXEL_H