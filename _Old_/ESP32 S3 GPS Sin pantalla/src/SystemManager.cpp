#include "SystemManager.h"
#include "Config.h"
#include "MPU6050.h"
#include "GPS.h"
#include "Battery.h"
#include "QMC5883L.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

QMC5883L compass;
GPS gps;       // Objeto GPS global


unsigned long previousTime = 0;

/* GPS en el segundo core *************************/

SemaphoreHandle_t xSemaphore = NULL;

void gpsUpdateTask(void *pvParameters) {
    for (;;) {
        if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
            gps.update();
            xSemaphoreGive(xSemaphore);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // Espera de 1 segundo entre cada actualización
    }
}


SystemManager::SystemManager()
    : stopThreshold(0.1), lastMovementTime(0)
{
    // Otras inicializaciones si son necesarias
}

void SystemManager::setup() {
    Serial.println("Iniciando...");

 
    Serial.println("Display inicializado");


    //pinMode(ADC_EN, OUTPUT);
    //digitalWrite(ADC_EN, HIGH);

    compass.begin();

    

    gps.setup();
    Serial.println("GPS inicializado");

    //bool sdStatus = sdCardLogger.setup(gps);
    //display.showSDStatus(sdStatus);
    Serial.println("Tarjeta SD inicializada");
    //display.showMessage("Inicialización SD");



    /* Crear el semáforo */
    xSemaphore = xSemaphoreCreateMutex();

    if (xSemaphore != NULL) {
        // Crear la tarea en el segundo núcleo
        xTaskCreatePinnedToCore(
            gpsUpdateTask,    // Nombre de la función que contiene la tarea
            "GPS Update Task",// Nombre de la tarea (para depuración)
            10000,            // Tamaño de la pila en palabras
            NULL,             // Parámetros de la tarea
            1,                // Prioridad de la tarea
            NULL,             // Puntero a la tarea (no necesitamos este valor)
            1                 // Núcleo al que se va a asociar la tarea (0 o 1)
        );
    }
    
    

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
   
   
   int PosSignalX = TFT_HEIGHT-63;
   int PosSignalY = 5;
   //display.showBatteryStatus();
   unsigned long currentTime = millis();

    if (currentTime - previousTime >= 100) { // 100 ms
        previousTime = currentTime;
       
       //display.updateGPS(gps);
        
        //mpu.update();
        //display.updateMPU(mpu);

        //compass.update();  // Llamar el método update()
        //display.updateCompass(compass);

        aht20_bmp.update();
        display.updateSensorData();

        display.showBatteryStatus();
    }
    
}
