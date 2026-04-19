#include "FaderADC.h"
#include "../../config.h"

#define FADER_ADC_UNIT    ADC_UNIT_1
#define FADER_ADC_CHANNEL ADC_CHANNEL_9   // GPIO10 en ESP32-S2
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

    // Seed inicial — raw directo, sin EMA aún
    int raw = 0;
    adc_oneshot_read(_adcHandle, FADER_ADC_CHANNEL, &raw);
    _rawLast  = raw;
    _emaValue = (float)raw;
    
    for (int i = 0; i < NOISE_WINDOW_SIZE; i++) _noiseWindow[i] = (float)raw;

    log_i("[ADC] seed=%d raw", _rawLast);
}

void FaderADC::update() {
    // 1. Oversampling — 3 grupos × 8 muestras
    auto readGroup = [&]() -> float {
        uint32_t s = 0;
        for (int i = 0; i < 8; i++) {
            int r = 0;
            adc_oneshot_read(_adcHandle, FADER_ADC_CHANNEL, &r);
            s += r;
        }
        return (float)s / 8.0f;
    };

    float a = readGroup();
    float b = readGroup();
    float c = readGroup();

    // 2. Mediana de los 3 grupos
    float median;
    if ((a <= b && b <= c) || (c <= b && b <= a)) median = b;
    else if ((b <= a && a <= c) || (c <= a && a <= b)) median = a;
    else median = c;

    // 3. Ventana deslizante — ruido local
    _noiseWindow[_noiseHead] = median;
    _noiseHead = (_noiseHead + 1) % NOISE_WINDOW_SIZE;

    float nMin = 8191.0f, nMax = 0.0f;
    for (int i = 0; i < NOISE_WINDOW_SIZE; i++) {
        if (_noiseWindow[i] < nMin) nMin = _noiseWindow[i];
        if (_noiseWindow[i] > nMax) nMax = _noiseWindow[i];
    }
    _noiseSpan = nMax - nMin;

    // 4. Detección de movimiento: umbral clásico OR tendencia direccional
    float diff        = fabsf(median - _emaValue);
    float threshMove  = _noiseSpan * NOISE_K_MOVE;
    float threshMicro = _noiseSpan * NOISE_K_MICRO;

    bool moving   = (diff > threshMove);
    bool trending = _isTrending(20.0f);   // 20 raw deadband por sample

    if (moving || trending) {
        _emaValue += FADER_EMA_ALPHA_FAST * (median - _emaValue);
    } else if (diff > threshMicro) {
        _emaValue += (FADER_EMA_ALPHA_FAST * 0.05f) * (median - _emaValue);
    }
    // diff ≤ threshMicro && !trending → congelado

    _rawLast  = (int)median;
    _faderPos = (uint16_t)(_emaValue + 0.5f);

    // 5. Log solo cuando hay actividad
    if (diff > threshMicro || trending) {
        log_i("[ADC] med=%.0f ema=%d span=%.0f diff=%.0f tMov=%.0f tr=%d",
              median, _faderPos, _noiseSpan, diff, threshMove, (int)trending);
    }
}

// Mide ruido y span en escala raw real.
// Llamar con el fader quieto en una posición conocida.
void FaderADC::measureRange() {
    int minVal = 8191, maxVal = 0;
    float ema  = (float)_rawLast;       // parte del estado EMA actual

    for (int i = 0; i < 100; i++) {
        int raw = 0;
        adc_oneshot_read(_adcHandle, FADER_ADC_CHANNEL, &raw);
        ema += FADER_EMA_ALPHA_FAST * ((float)raw - ema);
        int pos = (int)ema;
        if (pos < minVal) minVal = pos;
        if (pos > maxVal) maxVal = pos;
        delay(10);
    }
    log_i("[ADC] RANGE  min=%d  max=%d  span=%d  (raw EMA, 13-bit)",
          minVal, maxVal, maxVal - minVal);
}

bool FaderADC::_isTrending(float deadband) const {
    // Compara primera mitad de la ventana contra segunda
    // Si el desplazamiento neto es consistente → movimiento real
    float oldest = _noiseWindow[_noiseHead];
    float newest = _noiseWindow[(_noiseHead + NOISE_WINDOW_SIZE - 1) % NOISE_WINDOW_SIZE];
    
    // Contar cuántas muestras están por encima/debajo de la más antigua
    uint8_t up = 0, down = 0;
    for (uint8_t i = 1; i < NOISE_WINDOW_SIZE; i++) {
        float s = _noiseWindow[(_noiseHead + i) % NOISE_WINDOW_SIZE];
        if (s > oldest + deadband) up++;
        else if (s < oldest - deadband) down++;
    }
    return (up >= 5 || down >= 5);
}