#include "MPU6050.h"


MPU6050::MPU6050() : 
    ax(0), ay(0), az(0), gx(0), gy(0), gz(0),
    temperature(0), inclinationX(0), inclinationY(0), inclinationZ(0),
    maxAx(0), maxAy(0), maxAz(0), sumAx(0), sumAy(0), sumAz(0), count(0),
    offsetAx(0), offsetAy(0), offsetAz(0), offsetGx(0), offsetGy(0), offsetGz(0),
    lastUpdateTime(0) {}


void MPU6050::setup() {
    if (!mpu.begin()) {
        Serial.println("Failed to find MPU6050 chip");
        return; // agregamos este return para terminar la función si hay un error
    }
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    Serial.println("MPU6050 Rango 8G");
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    Serial.println("MPU6050 Rango 600 DEG");
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    Serial.println("MPU6050 Band 21 HZ");
    //readEEPROMOffsets();
    calculateOffsets();
}


void MPU6050::update() {
    if (millis() - lastUpdateTime < 100) return; // Update every 100ms
    lastUpdateTime = millis();

    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    ax = a.acceleration.x - offsetAx;
    ay = a.acceleration.y - offsetAy;
    az = a.acceleration.z - offsetAz;
    gx = g.gyro.x - offsetGx;
    gy = g.gyro.y - offsetGy;
    gz = g.gyro.z - offsetGz;
    temperature = temp.temperature;

    inclinationX = atan2(ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
    inclinationY = atan2(ay, sqrt(ax * ax + az * az)) * 180.0 / PI;
    inclinationZ = atan2(az, sqrt(ax * ax + ay * ay)) * 180.0 / PI;

    if (abs(ax) > maxAx) maxAx = abs(ax);
    if (abs(ay) > maxAy) maxAy = abs(ay);
    if (abs(az) > maxAz) maxAz = abs(az);

    sumAx += ax;
    sumAy += ay;
    sumAz += az;
    count++;
}

float MPU6050::getAccelerationX() const { return ax; }
float MPU6050::getAccelerationY() const { return ay; }
float MPU6050::getAccelerationZ() const { return az; }
float MPU6050::getGyroX() const { return gx; }
float MPU6050::getGyroY() const { return gy; }
float MPU6050::getGyroZ() const { return gz; }
float MPU6050::getTemperature() const { return temperature; }
float MPU6050::getInclinationX() const { return inclinationX; }
float MPU6050::getInclinationY() const { return inclinationY; }
float MPU6050::getInclinationZ() const { return inclinationZ; }
float MPU6050::getMaxAccelerationX() const { return maxAx; }
float MPU6050::getMaxAccelerationY() const { return maxAy; }
float MPU6050::getMaxAccelerationZ() const { return maxAz; }
float MPU6050::getAvgAccelerationX() const { return sumAx / count; }
float MPU6050::getAvgAccelerationY() const { return sumAy / count; }
float MPU6050::getAvgAccelerationZ() const { return sumAz / count; }

void MPU6050::calculateOffsets() {
    Serial.println("Calculando Offsets");
    const int sampleCount = 1000;
    float sumAx = 0, sumAy = 0, sumAz = 0;
    float sumGx = 0, sumGy = 0, sumGz = 0;

    for (int i = 0; i < sampleCount; i++) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);
        sumAx += a.acceleration.x;
        sumAy += a.acceleration.y;
        sumAz += a.acceleration.z;
        sumGx += g.gyro.x;
        sumGy += g.gyro.y;
        sumGz += g.gyro.z;
        delay(5);
    }

    offsetAx = sumAx / sampleCount;
    offsetAy = sumAy / sampleCount;
    offsetAz = sumAz / sampleCount - 9.81; // Subtract gravity
    offsetGx = sumGx / sampleCount;
    offsetGy = sumGy / sampleCount;
    offsetGz = sumGz / sampleCount;

    Serial.println("Offsets Calculados");

    //writeEEPROMOffsets();
}

void MPU6050::readEEPROMOffsets() {
    EEPROM.get(0, offsetAx);
    EEPROM.get(sizeof(offsetAx), offsetAy);
    EEPROM.get(sizeof(offsetAx) + sizeof(offsetAy), offsetAz);
    EEPROM.get(sizeof(offsetAx) + sizeof(offsetAy) + sizeof(offsetAz), offsetGx);
    EEPROM.get(sizeof(offsetAx) + sizeof(offsetAy) + sizeof(offsetAz) + sizeof(offsetGx), offsetGy);
    EEPROM.get(sizeof(offsetAx) + sizeof(offsetAy) + sizeof(offsetAz) + sizeof(offsetGx) + sizeof(offsetGy), offsetGz);
}

void MPU6050::writeEEPROMOffsets() {
    EEPROM.put(0, offsetAx);
    EEPROM.put(sizeof(offsetAx), offsetAy);
    EEPROM.put(sizeof(offsetAx) + sizeof(offsetAy), offsetAz);
    EEPROM.put(sizeof(offsetAx) + sizeof(offsetAy) + sizeof(offsetAz), offsetGx);
    EEPROM.put(sizeof(offsetAx) + sizeof(offsetAy) + sizeof(offsetAz) + sizeof(offsetGx), offsetGy);
    EEPROM.put(sizeof(offsetAx) + sizeof(offsetAy) + sizeof(offsetAz) + sizeof(offsetGx) + sizeof(offsetGy), offsetGz);
    EEPROM.commit();
}
