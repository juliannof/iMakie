#include <Arduino.h>
#include "SystemManager.h"

SystemManager systemManager;

void setup() {
    Serial.begin(115200);  // Inicializar el puerto serie
    //while (!Serial)
    //delay(10); // will pause Zero, Leonardo, etc until serial console opens

    systemManager.setup();
    //Serial.println("Iniciando...");
}

void loop() {
    systemManager.loop();
}
