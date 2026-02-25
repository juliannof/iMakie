#include "NTP.h"

NTP::NTP(const char* ntpServer, long gmtOffset_sec, int daylightOffset_sec)
    : ntpServer(ntpServer), gmtOffset_sec(gmtOffset_sec), daylightOffset_sec(daylightOffset_sec) {}

void NTP::begin() {
    // Inicializa la obtención de tiempo mediante NTP
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.println("Sincronizando hora con NTP...");
}

String NTP::getFormattedTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Error al obtener la hora");
        return "00:00:00";
    }
    char timeString[9];
    strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);
    return String(timeString);
}

time_t NTP::getEpochTime() {
    return time(nullptr);
}
