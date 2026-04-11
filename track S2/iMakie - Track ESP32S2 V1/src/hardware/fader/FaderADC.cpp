#include "FaderADC.h"
#include "../../config.h"

// GPIO10 = ADC1_CH9 en ESP32-S2
#define FADER_ADC_UNIT    ADC_UNIT_1
#define FADER_ADC_CHANNEL ADC_CHANNEL_9
#define FADER_ADC_ATTEN   ADC_ATTEN_DB_11


void FaderADC::begin() {
    adc_oneshot_unit_init_cfg_t unitCfg = {
        .unit_id  = FADER_ADC_UNIT,
        .clk_src  = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unitCfg, &_adcHandle));

    adc_oneshot_chan_cfg_t chanCfg = {
        .atten    = FADER_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_13,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(_adcHandle, FADER_ADC_CHANNEL, &chanCfg));
   

    // Sin calibración — raw directo
    int raw = 0;
    adc_oneshot_read(_adcHandle, FADER_ADC_CHANNEL, &raw);
    _rawLast  = raw;
    _emaValue = (float)raw;
    log_i("[ADC] seed=%d raw", _rawLast);
}

void FaderADC::update() {
    int raw = 0;
    adc_oneshot_read(_adcHandle, FADER_ADC_CHANNEL, &raw);
    _rawLast   = raw;
    _emaValue += FADER_EMA_ALPHA * ((float)raw - _emaValue);
    int filtered = constrain((int)_emaValue, FADER_ADC_MIN, FADER_ADC_MAX);
    _faderPos    = (uint16_t)map(filtered, FADER_ADC_MIN, FADER_ADC_MAX, 0, 8191);
}

void FaderADC::measureRange() {
    int minVal = 8191, maxVal = 0;
    for (int i = 0; i < 100; i++) {
        int raw = 0;
        adc_oneshot_read(_adcHandle, FADER_ADC_CHANNEL, &raw);
        int filtered = constrain(raw, FADER_ADC_MIN, FADER_ADC_MAX);
        int pos = (int)map(filtered, FADER_ADC_MIN, FADER_ADC_MAX, 0, 8191);
        if (pos < minVal) minVal = pos;
        if (pos > maxVal) maxVal = pos;
        delay(10);
    }
    log_i("[ADC] RANGE  min=%d  max=%d  span=%d", minVal, maxVal, maxVal - minVal);
}