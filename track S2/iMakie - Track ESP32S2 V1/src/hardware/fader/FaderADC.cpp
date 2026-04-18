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
    log_i("[ADC] seed=%d raw", _rawLast);
}

void FaderADC::update() {
    // 1. Muestreo con sobremuestreo (Oversampling)
    auto readOneAverage = [&]() {
        uint32_t s = 0;
        const int samples = 32; 
        for (int i = 0; i < samples; i++) {
            int r = 0;
            adc_oneshot_read(_adcHandle, FADER_ADC_CHANNEL, &r);
            s += r;
        }
        return (float)s / (float)samples;
    };

    // 2. Filtro de Mediana (Elimina picos de ruido esporádicos)
    float a = readOneAverage();
    float b = readOneAverage();
    float c = readOneAverage();

    float median;
    if ((a <= b && b <= c) || (c <= b && b <= a)) median = b;
    else if ((b <= a && a <= c) || (c <= a && a <= b)) median = a;
    else median = c;

    // 3. Lógica de Histéresis Dinámica (Zona Muerta)
    // Para 13 bits (0-8191), un ruido de 40-60 puntos es común.
    // Usamos un umbral de 'ruido' para decidir qué tan agresivo es el EMA.
    float diff = abs(median - _emaValue);
    
    if (diff > 120.0f) { 
        // Movimiento real detectado: Respuesta rápida
        // FADER_EMA_ALPHA debería estar entre 0.1 y 0.3
        _emaValue += FADER_EMA_ALPHA * (median - _emaValue);
    } 
    else if (diff > 20.0f) {
        // Micro-movimientos o ruido residual: Filtrado ultra agresivo
        // Esto elimina el "jitter" cuando el fader está casi quieto
        _emaValue += (FADER_EMA_ALPHA * 0.05f) * (median - _emaValue);
    }
    // Si diff < 8.0f, ignoramos el cambio por completo (estabilidad absoluta)

    // 4. Actualización de estado
    _rawLast = (int)median;
    
    // Aplicamos un redondeo simple para evitar saltos decimales en la salida
    _faderPos = (uint16_t)(_emaValue + 0.5f);

    // Opcional: Solo loguear si hay cambios significativos para no saturar el Serial
    if (diff > 10.0f) {
        log_i("[ADC] MED: %.2f | EMA: %d | Δ: %.2f", median, _faderPos, diff);
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
        ema += FADER_EMA_ALPHA * ((float)raw - ema);
        int pos = (int)ema;
        if (pos < minVal) minVal = pos;
        if (pos > maxVal) maxVal = pos;
        delay(10);
    }
    log_i("[ADC] RANGE  min=%d  max=%d  span=%d  (raw EMA, 13-bit)",
          minVal, maxVal, maxVal - minVal);
}