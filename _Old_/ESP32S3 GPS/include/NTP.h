#ifndef NTP_H
#define NTP_H

#include <WiFi.h>
#include "time.h"

class NTP {
public:
    NTP(const char* ntpServer = "pool.ntp.org", long gmtOffset_sec = 0, int daylightOffset_sec = 3600);
    void begin();
    String getFormattedTime();
    time_t getEpochTime();

private:
    const char* ntpServer;
    long gmtOffset_sec;
    int daylightOffset_sec;
};

#endif // NTP_H
