    #include "GPS.h"
    #include "Config.h"

    GPS::GPS()
        : gpsSerial(1) {}

    void GPS::setup() {
        gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
        while (gpsSerial.available() > 0) {
            gps.encode(gpsSerial.read());
        }
        while (!gps.date.isValid() || !gps.time.isValid()) {
            if (gpsSerial.available() > 0) {
                gps.encode(gpsSerial.read());
            }
        }
    }

    void GPS::update() {
        smartDelay(1000);  // Actualizamos el GPS cada segundo

        if (gps.speed.isValid()) {
            float currentSpeed = gps.speed.kmph();
        }
    }

    int GPS::getYear() const { return const_cast<TinyGPSDate&>(gps.date).year(); }
    int GPS::getMonth() const { return const_cast<TinyGPSDate&>(gps.date).month(); }
    int GPS::getDay() const { return const_cast<TinyGPSDate&>(gps.date).day(); }
    int GPS::getHour() const { 
        int hour = const_cast<TinyGPSTime&>(gps.time).hour();
        return (hour >= 24) ? hour - 24 : hour;
    }
    int GPS::getMinute() const { return const_cast<TinyGPSTime&>(gps.time).minute(); }
    int GPS::getSecond() const { return const_cast<TinyGPSTime&>(gps.time).second(); }
    uint8_t GPS::getSatellites() const { return const_cast<TinyGPSInteger&>(gps.satellites).value(); }
    float GPS::getLatitude() const { return const_cast<TinyGPSLocation&>(gps.location).lat(); }
    float GPS::getLongitude() const { return const_cast<TinyGPSLocation&>(gps.location).lng(); }
    float GPS::getAltitude() const { return const_cast<TinyGPSAltitude&>(gps.altitude).meters(); }
    float GPS::getSpeed() const { return const_cast<TinyGPSSpeed&>(gps.speed).kmph(); }
    char GPS::getCourse() const { return gps.course.isValid() ? TinyGPSPlus::cardinal(const_cast<TinyGPSCourse&>(gps.course).value())[0] : 'N'; }

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
