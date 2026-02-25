#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H


#include "MPU6050.h"
#include "GPS.h"



class SystemManager {
public:
    SystemManager();
    void setup();
    void loop();

private:
    GPS gps;
    Battery battery;  // Declarar battery como una variable miembro
    float stopThreshold; // Declaración de stopThreshold
    unsigned long lastMovementTime; // Declaración de lastMovementTime
    bool checkStopCondition(); // Declaración de checkStopCondition
};

#endif // SYSTEM_MANAGER_H