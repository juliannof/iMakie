#pragma once

// Configuración pines Lolin S2 Mini
#define PIN_TOUCH_T          T1    // GPIO1 para touch
#define PIN_LED_BUILTIN      15    // LED integrado

// Parámetros de configuración
const int DEBOUNCE_TIME_MS = 100;    // Tiempo anti-rebotes
const int CALIBRATION_SAMPLES = 100;      // Aumentado de 20 a 100
const int CALIBRATION_SAMPLE_DELAY = 20;  // Reducido delay entre muestras

// Parámetros de configuración
const float TOUCH_THRESHOLD_RATIO = 0.7f;


