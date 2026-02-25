#ifndef DISPLAY_H
#define DISPLAY_H

#include <TFT_eSPI.h> // Librería para la pantalla TFT
#include "GPS.h"
#include "MPU6050.h"
#include "QMC5883L.h"
#include "Battery.h"

class Display {
public:
    Display();
    void setup();
    void showSDStatus(bool sdStatus);
    void showMessage(const char* message);
    void updateGPS(const GPS& gps);
    void updateMPU(const MPU6050& mpu);
    void updateSensorData();
    void updateCompass(const QMC5883L& compass); // Nuevo método
    void background();
    void showBatteryStatus();
    void drawSignalIcon(int x, int y, int bars);
    void showEndSessionData(float avgAx, float avgAy, float avgAz, float maxAx, float maxAy, float maxAz);

private:
    TFT_eSPI tft;
    TFT_eSprite spr;     // Sprite agregado
    int radius;          // Radio de la brújula
    int centerX;         // Centro X de la brújula
    int centerY;         // Centro Y de la brújula
    int textRadius;      // Radio del texto alrededor de la brújula
    int prevYear, prevMonth, prevDay, prevHour, prevMinute, prevSecond;
    int prevSatellites;
    float prevLatitude, prevLongitude, prevAltitude, prevSpeed, prevAvgSpeed, prevMaxSpeed;
    char prevCourse;
    Battery battery;  // Añadir esto si `battery` debe ser un miembro

    void drawBattery(int x, int y, int width, int height, int chargeLevel, uint32_t color);
    void drawChargingSymbol(int x, int y, int width, int height);
    void displayGPSData(const GPS& gps);
    void displayMPUData(const MPU6050& mpu);
    void drawRotatedText(TFT_eSprite &spr, int cx, int cy, float angle, String text, int textRadius); // Función de texto rotado
};

#endif // DISPLAY_H
