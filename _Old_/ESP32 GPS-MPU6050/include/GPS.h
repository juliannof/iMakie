#ifndef GPS_H
#define GPS_H

#include <TinyGPS++.h>
#include <HardwareSerial.h>

class GPS {
public:
    GPS();
    void setup();
    void update();
    int getYear() const;
    int getMonth() const;
    int getDay() const;
    int getHour() const;
    int getMinute() const;
    int getSecond() const;
    uint8_t getSatellites() const;
    float getLatitude() const;
    float getLongitude() const;
    float getAltitude() const;
    float getSpeed() const;
    float getAvgSpeed() const;
    float getMaxSpeed() const;
    char getCourse() const;

private:
    void smartDelay(unsigned long ms);

    HardwareSerial gpsSerial;
    TinyGPSPlus gps;
    float avgSpeed;
    float maxSpeed;
    float totalSpeed;
    int speedCount;
};

#endif // GPS_H
