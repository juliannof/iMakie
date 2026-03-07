#pragma once
#include <Arduino.h>
#include "../config.h"

static constexpr float FADER_EMA_ALPHA = 0.10f;

class FaderADC {
public:
    void     begin();
    void     update();
    uint16_t getFaderPos()  const { return _faderPos; }   // 0–8191
    int      getRaw()       const { return (int)_emaValue; }

private:
    float    _emaValue  = 0.0f;
    uint16_t _faderPos  = 0;
};