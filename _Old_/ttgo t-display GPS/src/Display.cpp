#include "Display.h"
#include "User_Setup.h"
#include "Battery.h"
#include "Config.h"

// Posicion Senal
int PosSignalX = TFT_HEIGHT-63;
int PosSignalY = 5;
int barWidth = 5;  // Ancho de cada barra
int totalBars = 4;  // Número total de barras (máximo 10)

// Colores
//uint32_t TFT_GREY = 0x8410;  // Definir un color gris
uint32_t TFT_GREY = 0x6b4d;  // Definir un color gris

void Display::showSDStatus(bool sdStatus) {
    tft.fillCircle(10, 10, 5, sdStatus ? TFT_WHITE : TFT_RED);
}


Display::Display()
    : tft(TFT_eSPI()), prevYear(0), prevMonth(0), prevDay(0), prevHour(0), prevMinute(0), prevSecond(0),
      prevSatellites(0), prevLatitude(0), prevLongitude(0), prevAltitude(0), prevSpeed(0), prevAvgSpeed(0),
      prevMaxSpeed(0), prevCourse('N') {}

void Display::setup() {
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_WHITE);
    delay(1000);
    tft.fillScreen(TFT_BLACK);


    // Fondos grises 
    tft.setTextColor(TFT_GREY, TFT_BLACK);
    tft.setTextFont(1); // Restablecer la fuente del texto
    tft.setTextSize(1);

    // Coordenadas para la línea
    int x0 = 20;
    int y0 = 18;
    int x1 = tft.width() - 110; // x1 será el ancho de la pantalla menos 10 píxeles
    int y1 = y0; // Misma coordenada Y que el punto de inicio

    // Dibujar la línea
    tft.drawLine(x0, y0, x1, y1, TFT_GREY);

    // Coordenadas para la línea
    int x2 = 10;
    int y2 = tft.height()- 40;
    int x3 = tft.width()/2 - 50; // x1 será el ancho de la pantalla menos 10 píxeles
    int y3 = y2; // Misma coordenada Y que el punto de inicio

    // Dibujar la línea
    tft.drawLine(x2, y2, x3, y3, TFT_GREY);

    
    tft.setCursor((tft.width()/2)+3,(tft.height()/2)+30);
    tft.printf("km/h");
    tft.setCursor(50,(tft.height()-16));
    tft.printf("m");


    // Dibujar las barras grises inicialmente
    //drawSignalIcon(PosSignalX , PosSignalY, 0);
}
void Display::showMessage(const char* message) {
    tft.fillScreen(TFT_BLACK); // Limpiar la pantalla
    tft.setTextColor(TFT_WHITE);
    tft.drawString(message, tft.width() / 2, tft.height() / 2);
}


void Display::updateGPS(const GPS& gps) {
    if (gps.getYear() != prevYear || gps.getMonth() != prevMonth || gps.getDay() != prevDay ||
        gps.getHour() != prevHour || gps.getMinute() != prevMinute || gps.getSecond() != prevSecond ||
        gps.getSatellites() != prevSatellites || gps.getLatitude() != prevLatitude ||
        gps.getLongitude() != prevLongitude || gps.getAltitude() != prevAltitude ||
        gps.getSpeed() != prevSpeed || gps.getAvgSpeed() != prevAvgSpeed || gps.getMaxSpeed() != prevMaxSpeed ||
        gps.getCourse() != prevCourse) {
        
        displayGPSData(gps);

        prevYear = gps.getYear();
        prevMonth = gps.getMonth();
        prevDay = gps.getDay();
        prevHour = gps.getHour();
        prevMinute = gps.getMinute();
        prevSecond = gps.getSecond();
        prevSatellites = gps.getSatellites();
        prevLatitude = gps.getLatitude();
        prevLongitude = gps.getLongitude();
        prevAltitude = gps.getAltitude();
        prevSpeed = gps.getSpeed();
        prevAvgSpeed = gps.getAvgSpeed();
        prevMaxSpeed = gps.getMaxSpeed();
        prevCourse = gps.getCourse();
    }
    
    // Actualizar las barras de señal
    showBatteryStatus();
    drawSignalIcon(PosSignalX, PosSignalY, gps.getSatellites());
}

void Display::updateMPU(const MPU6050& mpu) {
    displayMPUData(mpu);
}

void Display::displayGPSData(const GPS& gps) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(1); // Restablecer la fuente del texto
    tft.setTextSize(1);

    char dateTime[50];
    sprintf(dateTime, "%02d:%02d:%02d - %02d-%02d-%04d", gps.getHour(), gps.getMinute(), gps.getSecond(), gps.getDay(), gps.getMonth(), gps.getYear());

    tft.setCursor(20, 7);
    tft.printf("%s\n", dateTime);
    tft.setCursor(PosSignalX + 28, PosSignalY + 2);
    tft.printf("%d\n", gps.getSatellites());

    tft.setCursor((tft.width() / 2), (tft.height() / 2 - 25));
    tft.setTextFont(7); // Restablecer la fuente del texto
    tft.setTextDatum(MC_DATUM);
    tft.printf("%.0f", gps.getSpeed());

    tft.setCursor(30, 105);
    tft.setTextFont(4); // Restablecer la fuente del texto
    tft.setTextDatum(ML_DATUM);
    tft.printf("%.0f", gps.getAltitude());
}

void Display::displayMPUData(const MPU6050& mpu) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(1); // Restablecer la fuente del texto
    tft.setTextSize(1);

    tft.setCursor(20, 22);
    tft.printf("AccX: %.2f\n", mpu.getAccelerationX());
    tft.setCursor(20, 32);
    tft.printf("AccY: %.2f\n", mpu.getAccelerationY());
    tft.setCursor(20, 42);
    tft.printf("AccZ: %.2f\n", mpu.getAccelerationZ());
    tft.setCursor(20, 55);
    tft.printf("X: %.2f\n", mpu.getInclinationX());
    tft.setCursor(20, 65);
    tft.printf("Y: %.2f\n", mpu.getInclinationY());
}


void Display::showBatteryStatus() {
    static uint64_t timeStamp = 0;
    if (millis() - timeStamp > 1000) {
        timeStamp = millis();
        
        float battery_voltage = battery.getVoltage();
        int chargeLevel = battery.getChargeLevel();
        bool isCharging = battery.isCharging();

        int batteryWidth = 20;
        int batteryHeight = 10;
        int batteryX = tft.width() - batteryWidth - 5;
        int batteryY = 5;

        tft.fillRect(batteryX + 2, batteryY + 2, batteryWidth - 4, batteryHeight - 4, TFT_BLACK);

        if (battery_voltage > battery.getEmptyVoltage()) {
            drawBattery(batteryX, batteryY, batteryWidth, batteryHeight, chargeLevel, TFT_GREEN);
            if (isCharging) {
                tft.fillRect(batteryX + 2, batteryY + 2, batteryWidth - 4, batteryHeight - 4, TFT_BLACK);
                drawChargingSymbol(batteryX, batteryY, batteryWidth, batteryHeight);
            }
        } else {
            drawBattery(batteryX, batteryY, batteryWidth, batteryHeight, 0, TFT_BLACK);
        }

        // Mostrar el valor de battery_voltage en la esquina derecha inferior
        int voltageWidth = 40;  // Ancho suficiente para el texto
        int voltageX = tft.width() - voltageWidth - 5;
        int voltageY = tft.height() - 15;  // Ajusta la posición vertical según necesidad

        // Crear una cadena para mostrar el voltaje
        char voltageStr[10];
        snprintf(voltageStr, sizeof(voltageStr), "%.2fV", battery_voltage);

        // Establecer el color del texto y el fondo
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextFont(1); // Restablecer la fuente del texto
        tft.setTextSize(1);

        // Imprimir el voltaje en la pantalla
        tft.drawString(voltageStr, voltageX, voltageY);
    }
}

void Display::drawBattery(int x, int y, int width, int height, int chargeLevel, uint32_t color) {
    int terminalWidth = 3;
    int terminalHeight = height / 2;

    tft.drawRect(x, y, width, height, TFT_WHITE);
    tft.fillRect(x + width, y + (height / 2) - (terminalHeight / 2), terminalWidth, terminalHeight, TFT_WHITE);

    int chargeWidth = (width - 4) * chargeLevel / 100;
    tft.fillRect(x + 2, y + 2, chargeWidth, height - 4, color);
}

void Display::drawChargingSymbol(int x, int y, int width, int height) {
    int centerX = x + width / 2;
    int centerY = y + height / 2;
    tft.fillTriangle(centerX - 2, centerY - 3, centerX + 2, centerY - 3, centerX, centerY + 3, TFT_WHITE);
}

void Display::drawSignalIcon(int x, int y, int bars) {
    // Convertimos bars a printBars para asegurarnos de que no sea superior a totalBars
    int printBars = min(bars, totalBars);

    // Dibujar todas las barras en TFT_GREY
    for (int i = 0; i < totalBars; i++) {
        int barHeight = 2 * (i + 1);  // Altura de cada barra (incrementa en 2 píxeles)
        int barX = x + i * (barWidth + 1);  // Posición X de cada barra
        int barY = y + 10 - barHeight;  // Posición Y de cada barra (desde abajo hacia arriba)

        // Dibujar una barra en TFT_GREY
        tft.fillRect(barX, barY, barWidth, barHeight, TFT_GREY);
    }

    // Dibujar las barras de señal en TFT_WHITE
    for (int i = 0; i < printBars; i++) {
        int barHeight = 2 * (i + 1);  // Altura de cada barra (incrementa en 2 píxeles)
        int barX = x + i * (barWidth + 1);  // Posición X de cada barra
        int barY = y + 10 - barHeight;  // Posición Y de cada barra (desde abajo hacia arriba)

        // Dibujar una barra en TFT_WHITE
        tft.fillRect(barX, barY, barWidth, barHeight, TFT_WHITE);
    }
}

void Display::showEndSessionData(float avgAx, float avgAy, float avgAz, float maxAx, float maxAy, float maxAz) {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.printf("Session Ended\n");
    tft.printf("Avg AccX: %.2f\n", avgAx);
    tft.printf("Avg AccY: %.2f\n", avgAy);
    tft.printf("Avg AccZ: %.2f\n", avgAz);
    tft.printf("Max AccX: %.2f\n", maxAx);
    tft.printf("Max AccY: %.2f\n", maxAy);
    tft.printf("Max AccZ: %.2f\n", maxAz);
}

