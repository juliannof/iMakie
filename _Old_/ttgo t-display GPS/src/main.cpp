#include <Arduino.h>
#include "SystemManager.h"

SystemManager systemManager;

void setup() {
    systemManager.setup();
}

void loop() {
    systemManager.loop();
}
