#ifndef DISPLAY_H
#define DISPLAY_H

#include <TFT_eSPI.h>
#include "MPU6050.h"
#include "GPS.h"
#include "Battery.h"



class Display {
public:
    Display();
    void setup();
    void updateGPS(const GPS& gps);
    void updateMPU(const MPU6050& mpu);
    void showEndSessionData(float avgAx, float avgAy, float avgAz, float maxAx, float maxAy, float maxAz);

    void showSDStatus(bool sdStatus);
    void drawSignalIcon(int x, int y, int bars);
    void showMessage(const char* message); // Declaración de showMessag

private:
    TFT_eSPI tft;
    Battery battery;
    int prevYear, prevMonth, prevDay, prevHour, prevMinute, prevSecond;
    int prevSatellites;
    float prevLatitude, prevLongitude, prevAltitude, prevSpeed, prevAvgSpeed, prevMaxSpeed;
    char prevCourse;

    void displayGPSData(const GPS& gps);
    void displayMPUData(const MPU6050& mpu);
    void showBatteryStatus();
    void drawBattery(int x, int y, int width, int height, int chargeLevel, uint32_t color);
    void drawChargingSymbol(int x, int y, int width, int height);
};

#endif

