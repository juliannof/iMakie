#pragma once
#include <Arduino.h>
#include "esp_adc/adc_oneshot.h"

class FaderADC {
public:
    void     begin();
    void     update();
    void     measureRange();
    uint16_t getFaderPos() const { return _faderPos; }
    int      getRawLast()  const { return _rawLast;  }  // raw ADC
private:
    adc_oneshot_unit_handle_t _adcHandle = nullptr;
    float    _emaValue = 0.0f;
    uint16_t _faderPos = 0;
    int      _rawLast  = 0;
};