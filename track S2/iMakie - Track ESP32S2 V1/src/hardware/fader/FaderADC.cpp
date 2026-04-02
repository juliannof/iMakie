// FaderADC.cpp
#include "FaderADC.h"
#include "../../config.h"

void FaderADC::begin() {
    analogReadResolution(12);
    analogRead(FADER_POT_PIN);                              // primer read fuerza config del pin
    analogSetPinAttenuation(FADER_POT_PIN, ADC_0db);        // volver al original
    delay(50);
    _rawLast  = analogRead(FADER_POT_PIN);
    _emaValue = (float)_rawLast;
    log_i("[ADC] seed=%d", _rawLast);
}

void FaderADC::update() {
    _rawLast   = analogRead(FADER_POT_PIN);
    _emaValue += FADER_EMA_ALPHA * ((float)_rawLast - _emaValue);

    int filtered = constrain((int)_emaValue, FADER_ADC_MIN, FADER_ADC_MAX);
    _faderPos    = (uint16_t)map(filtered, FADER_ADC_MIN, FADER_ADC_MAX, 0, 8191);
}