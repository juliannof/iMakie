#include "SDCardLogger.h"
#include "Config.h"

SPIClass SDSPI(HSPI);

SDCardLogger::SDCardLogger() {}

bool SDCardLogger::setup(const GPS& gps) {
    SDSPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);

    if (!SD.begin(SD_CS, SDSPI)) {
        Serial.println("No se pudo inicializar la tarjeta SD.");
        return false;
    }
    Serial.println("Tarjeta SD inicializada correctamente.");

    currentFileName = getFileName(gps);

    // Verificar si el archivo ya existe antes de crearlo
    if (!SD.exists(currentFileName)) {
        dataFile = SD.open(currentFileName, FILE_WRITE);
        if (!dataFile) {
            Serial.println("Error al abrir el archivo para escribir.");
            return false;
        }
        if (gps.getYear() > 2000) {
            writeInitialLine(); // Escribir la cabecera solo si el archivo no existe
        }
    } else {
        dataFile = SD.open(currentFileName, FILE_APPEND);
    }

    if (!dataFile) {
        Serial.println("Error al abrir el archivo en modo append.");
        return false;
    }

    return true;
}

bool SDCardLogger::isSDAvailable() const {
    return SD.cardType() != CARD_NONE;
}

void SDCardLogger::logData(const GPS& gps, const Battery& battery) {
    if (!dataFile) {
        Serial.println("La tarjeta SD no está disponible.");
        return;
    }

    // Verificar si hay datos válidos de latitud y longitud
    if (gps.getLatitude() == 0.0 || gps.getLongitude() == 0.0) {
        Serial.println("Datos de latitud y/o longitud no válidos. No se escribirán los datos.");
        return; // Salir del método si los datos no son válidos
    }

    float batteryVoltage = battery.getVoltage();

    String dataString = String(gps.getYear()) + "-" +
                        String(gps.getMonth()) + "-" +
                        String(gps.getDay()) + "," +
                        String(gps.getHour()) + ":" +
                        String(gps.getMinute()) + ":" +
                        String(gps.getSecond()) + "," +
                        String(gps.getLatitude(), 6) + "," +
                        String(gps.getLongitude(), 6) + "," +
                        String(gps.getSatellites()) + "," +
                        String(gps.getSpeed()) + "," +
                        String(gps.getAvgSpeed()) + "," +
                        String(gps.getMaxSpeed()) + "," +
                        String(gps.getCourse()) + "," +
                        String(batteryVoltage, 2);

    Serial.println("Datos a escribir: " + dataString); // Mensaje de depuración

    // Escribir los datos en el archivo
    dataFile.println(dataString);
    if (!dataFile) {
        Serial.println("Error al escribir en el archivo.");
    }
    dataFile.flush(); // Forzar la escritura inmediata en la tarjeta SD
}

void SDCardLogger::writeInitialLine() {
    dataFile.println("fecha,hora,latitude,longitude,satelites,speed,velocidad_media,velocidad_maxima,course,voltage");
    if (!dataFile) {
        Serial.println("Error al escribir la línea inicial.");
    }
    dataFile.flush(); // Asegurar que la cabecera se escriba inmediatamente
}

String SDCardLogger::getFileName(const GPS& gps) const {
    String fileName = "/GPS_" + 
                      String(gps.getYear()) +
                      (gps.getMonth() < 10 ? "0" : "") + String(gps.getMonth()) +
                      String(gps.getDay()) + 
                      String(gps.getHour()) + 
                      (gps.getMinute() < 10 ? "0" : "") + String(gps.getMinute()) + ".csv";
    return fileName;
}

void SDCardLogger::writeSessionSummary(float avgAx, float avgAy, float avgAz, float maxAx, float maxAy, float maxAz) {
    File summaryFile = SD.open("/session_summary.txt", FILE_WRITE);
    if (!summaryFile) {
        Serial.println("Error al abrir el archivo de resumen.");
        return;
    }

    summaryFile.printf("Archivo de sesión: %s\n", currentFileName.c_str());
    summaryFile.printf("Avg AccX: %.2f, Avg AccY: %.2f, Avg AccZ: %.2f\n", avgAx, avgAy, avgAz);
    summaryFile.printf("Max AccX: %.2f, Max AccY: %.2f, Max AccZ: %.2f\n", maxAx, maxAy, maxAz);

    summaryFile.close();
}

void SDCardLogger::endSession() {
    if (dataFile) {
        dataFile.close();
    }
}
