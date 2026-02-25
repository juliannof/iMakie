#ifndef SDCARDLOGGER_H
#define SDCARDLOGGER_H

#include <SPI.h>
#include <SD.h>
#include "GPS.h"
#include "Battery.h"

class SDCardLogger {
public:
    SDCardLogger();
    bool setup(const GPS& gps);
    bool isSDAvailable() const;
    void logData(const GPS& gps, const Battery& battery);
    void writeSessionSummary(float avgAx, float avgAy, float avgAz, float maxAx, float maxAy, float maxAz);
    void endSession();
private:
    void writeInitialLine();
    String getFileName(const GPS& gps) const;
    File dataFile;
    String currentFileName;
    bool sdAvailable = false;
};

#endif // SDCARDLOGGER_H