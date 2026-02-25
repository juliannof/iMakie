#include "MPU6500.h"

// Definición del objeto mpu
MPU6500 mpu; // Definición única del objeto mpu

MPU6500::MPU6500() : 
    aX(0), aY(0), aZ(0), 
    gX(0), gY(0), gZ(0), 
    lastUpdateTime(0) {}

void MPU6500::setup() {
    Serial.println("Inicializando MPU9250...");

    // Inicializa el bus I2C usando los pines estándar
    Wire.begin(8, 9); // SDA en GPIO 8, SCL en GPIO 9
    mySensor.setWire(&Wire); // Asigna el bus I2C al sensor

    // Inicializa los componentes del sensor
    mySensor.beginAccel();
    mySensor.beginGyro();
    

    Serial.println("MPU9250 inicializado correctamente.");
}

void MPU6500::update() {
    if (millis() - lastUpdateTime < 100) return; // Actualizar cada 100 ms
    lastUpdateTime = millis();

    // Leer aceleración
    if (mySensor.accelUpdate() == 0) {
        aX = mySensor.accelX();
        aY = mySensor.accelY();
        aZ = -mySensor.accelZ();
        //Serial.println("Aceleración X: " + String(aX));
        //Serial.println("Aceleración Y: " + String(aY));
        //Serial.println("Aceleración Z: " + String(aZ));
    } else {
        Serial.println("No se pueden leer los valores de aceleración.");
    }

    // Leer giro
    if (mySensor.gyroUpdate() == 0) {
        gX = mySensor.gyroX();
        gY = mySensor.gyroY();
        gZ = mySensor.gyroZ();
        //Serial.println("Giro X: " + String(gX));
        //Serial.println("Giro Y: " + String(gY));
        //Serial.println("Giro Z: " + String(gZ));
    } else {
        Serial.println("No se pueden leer los valores de giro.");
    }
}

float MPU6500::getAccelerationX() const { return aX; }
float MPU6500::getAccelerationY() const { return aY; }
float MPU6500::getAccelerationZ() const { return aZ; }
float MPU6500::getGyroX() const { return gX; }
float MPU6500::getGyroY() const { return gY; }
float MPU6500::getGyroZ() const { return gZ; }