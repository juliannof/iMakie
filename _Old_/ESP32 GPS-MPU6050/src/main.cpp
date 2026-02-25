#include <Arduino.h>
#include "Battery.h"
#include "MPU6050.h"
#include "BLE.h"
#include "WiFi.h"
#include "GPS.h"


SemaphoreHandle_t xSemaphore;  // Semáforo para sincronizar el acceso a las variables

GPS gps;  // Instancia del objeto GPS

// Definimos un manejador para la tarea del GPS
TaskHandle_t gpsTaskHandle;

void gpsTask(void *parameter) {
    gps.setup();
    while (true) {
        gps.update();  // Llamamos a update cada segundo desde la tarea
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Reforzamos el delay de un segundo
    }
}

extern bool deviceConnected;
extern bool oldDeviceConnected;
// constants won't change. Used here to set a pin number:
const int ledPin = LED_BUILTIN;  // the number of the LED pin
// Variables will change:
int ledState = LOW;  // ledState used to set the LED

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;  // will store last time LED was updated

// constants won't change:
const long interval = 100;  // interval at which to blink (milliseconds)


// Resto de la implementación de `setup` y `loop`...


void setup() {
    Serial.begin(115200);
    // Configurar MPU
    mpu.setup();
    // set the digital pin as output:
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, ledState);
    // Desactivar WiFi
    WiFi.disconnect(true);  // Desconecta de cualquier red y apaga el WiFi
    WiFi.mode(WIFI_OFF);    // Pone el WiFi en modo off
    setupBLE(); // Inicializar BLE

    // Creamos el semáforo para proteger el acceso a las variables compartidas
    xSemaphore = xSemaphoreCreateMutex();
    if (xSemaphore == NULL) {
        Serial.println("Error al crear semáforo");
        return;
    }

    // Crear tarea para ejecutar GPS::update() en el core 1
    xTaskCreatePinnedToCore(
        gpsTask,           // Función de la tarea
        "GPS Task",        // Nombre de la tarea
        4096,              // Tamaño del stack
        NULL,              // Parámetro (en este caso, no hay)
        1,                 // Prioridad de la tarea
        NULL,              // Handle de la tarea (no lo usamos aquí)
        1                  // Core 1
    );
}

void loop() {
    mpu.update();
    // Lógica para verificar el estado de la conexión
     // set the LED with the ledState of the variable:
    //digitalWrite(ledPin, ledState);
    if (deviceConnected && !oldDeviceConnected) {
      
      Serial.println("Estamos Conectados");
      ledState = HIGH;  // ledState used to set the LED
      
        // Hacer algo cuando se conecta
      oldDeviceConnected = deviceConnected;
    }

    if (!deviceConnected && oldDeviceConnected) {
        ledState = LOW;  // ledState used to set the LED
        startAdvertising();// Hacer algo cuando se desconecta
        oldDeviceConnected = deviceConnected;

    }
 
  
digitalWrite(ledPin, ledState);

// here is where you'd put code that needs to be running all the time.

  // check to see if it's time to blink the LED; that is, if the difference
  // between the current time and last time you blinked the LED is bigger than
  // the interval at which you want to blink the LED.
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    sendData();
  }
  

}
