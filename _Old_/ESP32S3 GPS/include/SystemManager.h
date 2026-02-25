#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include "Display.h"
#include "MPU6050.h"
#include "GPS.h"
#include "SDCardLogger.h"  // Asegúrate de incluir SDCardLogger.h
#include "Buttons.h"


class SystemManager {
public:
    SystemManager();
    void setup();
    void loop();

private:
    Display display;
    MPU6050 mpu;
    GPS gps;
    Battery battery;  // Declarar battery como una variable miembro
    SDCardLogger sdCardLogger;
    float stopThreshold; // Declaración de stopThreshold
    unsigned long lastMovementTime; // Declaración de lastMovementTime
    bool checkStopCondition(); // Declaración de checkStopCondition
};

#endif // SYSTEM_MANAGER_H