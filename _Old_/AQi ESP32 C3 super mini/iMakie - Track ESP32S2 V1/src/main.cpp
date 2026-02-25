#include <Arduino.h>
#if CONFIG_IDF_TARGET_ESP32S2
  #include "USB.h"
  #include "USBCDC.h"
  USBCDC USBSerial;
#endif

#include <TFT_eSPI.h>
#include <SPI.h>
#include "config.h"
#include "display/Display.h"

#include <Adafruit_NeoPixel.h>
#include <Button2.h> // Librería para el botón

Adafruit_NeoPixel pixels(NUMPIXELS, NEO_PIN, NEO_GRB + NEO_KHZ800);

#define DELAYVAL 500 // Time (in milliseconds) to pause between pixels


// Definición de numero de pista
int TrackID = 1; // <<<--- DEFINICIÓN DE String


//#include "esp_log.h" // A veces es necesario incluirlo explícitamente

// Definir la TAG para los logs de ESP-IDF
static const char *TAG = "MiApp"; // O el nombre que quieras para esta parte del código


TFT_eSPI tft = TFT_eSPI();
TFT_eSprite header(&tft), mainArea(&tft), vuSprite(&tft), vPotSprite(&tft);

// Declaraciones extern para las funciones públicas de display/Display.cpp.
// Esto permite que main.cpp las llame sin conocer su implementación interna.
extern void initDisplay();                // Inicialización de toda la pantalla
extern void updateDisplay();              // Función principal de refresco de pantalla
extern void setScreenBrightness(uint8_t brightness); // Control del brillo de la pantalla

void setup() {
    #if CONFIG_IDF_TARGET_ESP32S2
    USB.begin();
    USBSerial.begin(115200);
    while (!USBSerial) delay(10);
    #define Serial USBSerial
  #else
    Serial.begin(115200);
  #endif
    
    // Configurar el pin de retroiluminación y apagarlo inicialmente
    //pinMode(TFT_BL, OUTPUT);
    //digitalWrite(TFT_BL, LOW);
    // Inicializar la pantalla
    initDisplay();
    pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
    pixels.clear(); // Set all pixel colors to 'off'
    pixels.setPixelColor(0, pixels.Color(2, 0, 0));
    pixels.setPixelColor(1, pixels.Color(2, 2, 0));
    pixels.setPixelColor(2, pixels.Color(2, 0, 0));
    pixels.setPixelColor(3, pixels.Color(2, 2, 2));
    pixels.show();   // Send the updated pixel colors to the hardware.


   
    // Encender la retroiluminación después de tener todo listo
    //digitalWrite(BACKLIGHT_PIN, HIGH);
    log_v("Track 1 DEMO started.");
    Serial.println("Track 1 DEMO started.");

}

void loop() {
    // Delega toda la lógica de actualización y dibujo de la pantalla a updateDisplay().
    // Esta función, implementada en Display.cpp, se encargará de:
    // - Ejecutar tu lógica de simulación del loop original.
    // - Llamar a tus funciones de dibujo (`updateMeter`, `updateVPot`).
    // - Gestionar los pushSprite().
    updateDisplay();
    log_v("Track 1 DEMO loop.");
    //Serial.println("--- loop() ejecutándose ---");

    // Aquí iría el resto de TU LÓGICA DE APLICACIÓN principal,
    // NO RELACIONADA CON LA PANTALLA o que no bloquee.
    // Ejemplos: lectura de faders, encoders, botones, procesamiento de MIDI, etc.
    // Es crucial NO PONER DELAYS BLOQUEANTES aquí para no afectar la fluidez del refresco de pantalla.
}