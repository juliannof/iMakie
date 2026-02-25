#include <Arduino.h>
#include "driver/adc.h"
#include "esp_adc_cal.h"

#define POT_PIN 3             // GPIO conectado al wiper del fader
#define ADC_CHANNEL ADC_CHANNEL_2  // canal ADC correspondiente al GPIO3

esp_adc_cal_characteristics_t adc_chars;

// Ajuste de rango máximo (milivoltios reales que producen 4095)
#define MAX_REAL_MV 2680  // 2.68 V medidos con multímetro

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("ADC calibrado por software - Escala completa 0..4095");

  // Configura resolución y atenuación
  analogReadResolution(12);                 // Arduino usa 0..4095
  analogSetPinAttenuation(POT_PIN, ADC_11db);

  // Caracterización del ADC usando Vref ajustado al máximo real
  esp_adc_cal_characterize(
    ADC_UNIT_1,
    ADC_ATTEN_DB_12,
    ADC_WIDTH_BIT_13,
    MAX_REAL_MV,  // uso del valor real medido como Vref
    &adc_chars
  );
}

void loop() {
  uint32_t raw = analogRead(POT_PIN);
  uint32_t mv  = esp_adc_cal_raw_to_voltage(raw, &adc_chars); // milivoltios ajustados

  // Escala lineal 0..4095 para usar toda la resolución
  uint32_t scaled = map(mv, 0, MAX_REAL_MV, 0, 4095);
  if (scaled > 4095) scaled = 4095; // recorte de seguridad

  // Escala opcional a 0..127 (para MIDI)
  uint8_t midiVal = map(scaled, 0, 4095, 0, 127);

  Serial.print("RAW: "); Serial.print(raw);
  Serial.print("\tV_calibrada: "); Serial.print(mv); Serial.print(" mV");
  Serial.print("\tScaled: "); Serial.print(scaled);
  Serial.print("\tMIDI: "); Serial.println(midiVal);

  delay(100); // 10 Hz aprox.
}
