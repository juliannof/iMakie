#include "GPS.h"
#include "Config.h"

GPS::GPS()
    : gpsSerial(1), avgSpeed(0), maxSpeed(0), totalSpeed(0), speedCount(0) {}

void GPS::setup() {

    gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

    /*
    while (gpsSerial.available() > 0) {
        gps.encode(gpsSerial.read());
        Serial.println("Leyendo GPS");
       
    }
    while (!gps.date.isValid() || !gps.time.isValid()) {
        if (gpsSerial.available() > 0) {
            gps.encode(gpsSerial.read());
        }
    }
    //Serial.println("Fecha: " + GPS::getYear());
    Serial.print("Fecha de setup : ");
    char fecha[15];
    sprintf(fecha, "%02d/%02d/%04d", GPS::getDay(), GPS::getMonth(), GPS::getYear());
    Serial.println(fecha);
    Serial.println("mirando el GPS");
    */
}

void GPS::update() {
    Serial.println("actualizando el GPS"); 
    smartDelay(1000);  // Actualizamos el GPS cada segundo
    //Serial.println("Fecha: " + GPS::getYear());
    Serial.print("Fecha : ");
    char fecha[15];
    sprintf(fecha, "%02d/%02d/%04d", GPS::getDay(), GPS::getMonth(), GPS::getYear());
    Serial.println(fecha);
    
    if (gps.speed.isValid()) {
        float currentSpeed = gps.speed.kmph();
        totalSpeed += currentSpeed;
        speedCount++;
        avgSpeed = totalSpeed / speedCount;

        if (currentSpeed > maxSpeed) {
            maxSpeed = currentSpeed;
        }
    }
}


int GPS::getYear() const { return const_cast<TinyGPSDate&>(gps.date).year(); }
int GPS::getMonth() const { return const_cast<TinyGPSDate&>(gps.date).month(); }
int GPS::getDay() const { return const_cast<TinyGPSDate&>(gps.date).day(); }
int GPS::getHour() const { 
    int hour = const_cast<TinyGPSTime&>(gps.time).hour() + 2; // Ajuste de hora para España
    return (hour >= 24) ? hour - 24 : hour;
}
int GPS::getMinute() const { return const_cast<TinyGPSTime&>(gps.time).minute(); }
int GPS::getSecond() const { return const_cast<TinyGPSTime&>(gps.time).second(); }
uint8_t GPS::getSatellites() const { return const_cast<TinyGPSInteger&>(gps.satellites).value(); }
float GPS::getLatitude() const { return const_cast<TinyGPSLocation&>(gps.location).lat(); }
float GPS::getLongitude() const { return const_cast<TinyGPSLocation&>(gps.location).lng(); }
float GPS::getAltitude() const { return const_cast<TinyGPSAltitude&>(gps.altitude).meters(); }
float GPS::getSpeed() const { return const_cast<TinyGPSSpeed&>(gps.speed).kmph(); }
float GPS::getAvgSpeed() const { return avgSpeed; }
float GPS::getMaxSpeed() const { return maxSpeed; }
char GPS::getCourse() const { return gps.course.isValid() ? TinyGPSPlus::cardinal(const_cast<TinyGPSCourse&>(gps.course).value())[0] : 'N'; }
// 
// This custom version of delay() ensures that the gps object
// is being "fed".
void GPS::smartDelay(unsigned long ms) {
    unsigned long start = millis();
    do {
        while (gpsSerial.available()) {
            gps.encode(gpsSerial.read());
        }
    } while (millis() - start < ms);
}
