// FaderADC.h
#pragma once
#include <Arduino.h>

class FaderADC {
public:
    void     begin();
    void     update();
    uint16_t getFaderPos() const { return _faderPos; }
    int      getRawLast()  const { return _rawLast;  }  // solo debug
private:
    float    _emaValue = 0.0f;
    uint16_t _faderPos = 0;
    int      _rawLast  = 0;
};