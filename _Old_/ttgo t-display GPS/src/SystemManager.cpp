#include "SystemManager.h"
#include "Config.h"
#include "SDCardLogger.h"
#include "MPU6050.h"
#include "GPS.h"
#include "Display.h"
#include "Buttons.h" // Incluir el encabezado de los botones
#include "Battery.h"  // Asegúrate de incluir Battery.h


GPS gps;       // Objeto GPS global
Display display; // Objeto Display global

SystemManager::SystemManager()
    : stopThreshold(0.1), lastMovementTime(0)
{
    // Otras inicializaciones si son necesarias
}


void SystemManager::setup() {
    Serial.begin(115200);  // Inicializar el puerto serie
    
    Serial.println("Iniciando...");

    display.setup();
    Serial.println("Display inicializado");

    // Mostrar mensaje de inicialización en el display
    display.showMessage("Inicializando...");

    /*
    ADC_EN is the ADC detection enable port
    If the USB port is used for power supply, it is turned on by default.
    If it is powered by battery, it needs to be set to high level
    */
    pinMode(ADC_EN, OUTPUT);
    digitalWrite(ADC_EN, HIGH);

    // Configurar MPU
    mpu.setup();
    display.showMessage("Inicialización mpu");

    // Configurar el GPS y obtener los datos iniciales del GPS
    gps.setup();
    Serial.println("GPS inicializado");
    display.showMessage("Inicialización GPS");

    // Configurar el Display
    //display.setup();
    //Serial.println("Display inicializado");

    // Inicializar la tarjeta SD con los datos del GPS
    bool sdStatus = sdCardLogger.setup(gps);
    display.showSDStatus(sdStatus);
    Serial.println("Tarjeta SD inicializada");
    display.showMessage("Inicialización SD");

    // Inicializar los botones
    button_init();
    display.showMessage(" ");
}

bool SystemManager::checkStopCondition() {
    if (abs(mpu.getAccelerationX()) < stopThreshold &&
        abs(mpu.getAccelerationY()) < stopThreshold &&
        abs(mpu.getAccelerationZ()) < stopThreshold) {
        if (millis() - lastMovementTime > 180000) { // 3 minutes
            return true;
        }
    } else {
        lastMovementTime = millis();
    }
    return false;
}

void SystemManager::loop() {
    btn1.loop();
    btn2.loop();
    
    if (checkStopCondition()) {
        float avgAx = mpu.getAvgAccelerationX();
        float avgAy = mpu.getAvgAccelerationY();
        float avgAz = mpu.getAvgAccelerationZ();
        float maxAx = mpu.getMaxAccelerationX();
        float maxAy = mpu.getMaxAccelerationY();
        float maxAz = mpu.getMaxAccelerationZ();

        sdCardLogger.writeSessionSummary(avgAx, avgAy, avgAz, maxAx, maxAy, maxAz);
        display.showEndSessionData(avgAx, avgAy, avgAz, maxAx, maxAy, maxAz);
        sdCardLogger.endSession(); // Cerrar el archivo de sesión
    }
    
    gps.update();
    display.updateGPS(gps);
    mpu.update();
    display.updateMPU(mpu);
    bool sdStatus = sdCardLogger.isSDAvailable();
    display.showSDStatus(sdStatus);

    if (sdStatus && !sdCardLogger.isSDAvailable()) {
        sdCardLogger.setup(gps); // Crear el archivo si se detecta la tarjeta
    }

    sdCardLogger.logData(gps, battery); // Pasar battery como parámetro
}