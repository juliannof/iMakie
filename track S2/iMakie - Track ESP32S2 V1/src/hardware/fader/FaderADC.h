#pragma once
#include <Arduino.h>
#include "esp_adc/adc_oneshot.h"

class FaderADC {
public:
    void     begin();
    void     update();
    void     measureRange();                              // diagnóstico de ruido en raw
    uint16_t getFaderPos() const { return _faderPos; }  // raw EMA (≈1200–6700)
    int      getRawLast()  const { return _rawLast;  }  // raw ADC sin filtrar
private:
    adc_oneshot_unit_handle_t _adcHandle = nullptr;
    float    _emaValue = 0.0f;
    uint16_t _faderPos = 0;
    int      _rawLast  = 0;
};