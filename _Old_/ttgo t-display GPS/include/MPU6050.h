#ifndef MPU6050_H
#define MPU6050_H

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <EEPROM.h>

class MPU6050 {
public:
    MPU6050();
    void setup();
    void update();
    float getAccelerationX() const;
    float getAccelerationY() const;
    float getAccelerationZ() const;
    float getGyroX() const;
    float getGyroY() const;
    float getGyroZ() const;
    float getTemperature() const;
    float getInclinationX() const;
    float getInclinationY() const;
    float getInclinationZ() const;
    float getMaxAccelerationX() const;
    float getMaxAccelerationY() const;
    float getMaxAccelerationZ() const;
    float getAvgAccelerationX() const;
    float getAvgAccelerationY() const;
    float getAvgAccelerationZ() const;

private:
    void calculateOffsets();
    void readEEPROMOffsets();
    void writeEEPROMOffsets();
    Adafruit_MPU6050 mpu;
    float ax, ay, az;
    float gx, gy, gz;
    float temperature;
    float inclinationX, inclinationY, inclinationZ;
    float maxAx, maxAy, maxAz;
    float sumAx, sumAy, sumAz;
    int count;
    float offsetAx, offsetAy, offsetAz;
    float offsetGx, offsetGy, offsetGz;
    unsigned long lastUpdateTime;
};

#endif // MPU6050_H

