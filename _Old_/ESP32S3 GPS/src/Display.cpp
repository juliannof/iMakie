#include "Display.h"
#include "User_Setup.h"
#include "Battery.h"
#include "AHT20_BMP280.h"
#include "Config.h"

// Posicion Senal GPS
int PosSignalX = TFT_HEIGHT-63;
int PosSignalY = 5;
int barWidth = 5;  // Ancho de cada barra de senal GPS
int totalBars = 4;  // Número total de barra de senal GPS

// Colores
uint32_t TFT_GREY = 0x6b4d;  // Definir un color gris
extern AHT20_BME280 aht20_bmp;

// Posicion Acelerometro
#define LINE_LENGTH 5
#define LINE_SPACING 4
#define LINE_COUNT 11 // 1 central + 5 a cada lado
#define EXTENDED_LINE_LENGTH (LINE_LENGTH * 2) // Doble tamaño para las líneas de los extremos

// Variables para las posiciones de las líneas
// Fondo horizonte artificial
int MPUcenterX = (TFT_HEIGHT / 2)-120;
int MPUcenterY = TFT_WIDTH / 2;
int verticalLineX[LINE_COUNT];
int horizontalLineY[LINE_COUNT];



// Estatus de la tarjeta
void Display::showSDStatus(bool sdStatus) {
    tft.fillCircle(10, 10, 5, sdStatus ? TFT_WHITE : TFT_RED);
}



// Constructor de la clase Display
Display::Display()
    : tft(TFT_eSPI()),       // Inicializar el objeto TFT_eSPI
      spr(&tft),              // Inicializar el sprite con el puntero a tft
      radius(25),            // Radio de la brújula
      centerX(280),          // Centro X de la brújula
      centerY(120),          // Centro Y de la brújula
      textRadius(30),        // Radio del texto
      prevYear(0), 
      prevMonth(0), 
      prevDay(0), 
      prevHour(0), 
      prevMinute(0), 
      prevSecond(0),
      prevSatellites(0), 
      prevLatitude(0), 
      prevLongitude(0), 
      prevAltitude(0), 
      prevSpeed(0), 
      prevAvgSpeed(0),
      prevMaxSpeed(0), 
      prevCourse('N') 
{
    // Inicialización adicional si es necesario
}

void Display::setup() {
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_WHITE);
    delay(100);
    tft.fillScreen(TFT_BLACK);

    // Crear sprite con tamaño suficiente para la brújula
    spr.createSprite(textRadius * 2 + 20, textRadius * 2 + 20);
}

void Display::background() {
    // Fondos grises 
    tft.setTextColor(TFT_GREY, TFT_BLACK);
    tft.setTextFont(1); // Restablecer la fuente del texto
    tft.setTextSize(1);


    // Dibujar las líneas y almacenar sus posiciones
    for (int i = 0; i < LINE_COUNT; i++) {
        int offset = (i - (LINE_COUNT / 2)) * (LINE_SPACING + 2);

        // Alternar entre líneas dobles y normales, comenzando por las de los extremos
        int currentLineLength = (i % 2 == 0) ? EXTENDED_LINE_LENGTH : LINE_LENGTH;

        // Dibujar línea vertical
        verticalLineX[i] = MPUcenterX + offset;
        tft.fillRect(verticalLineX[i] - 1, MPUcenterY - (currentLineLength / 2), 2, currentLineLength, TFT_GREY);

        // Dibujar línea horizontal
        horizontalLineY[i] = MPUcenterY + offset;
        tft.fillRect(MPUcenterX - (currentLineLength / 2), horizontalLineY[i] - 1, currentLineLength, 2, TFT_GREY);
    }

    // Coordenadas para la línea
    int x0 = 20;
    int y0 = 18;
    int x1 = tft.width() - 10; // x1 será el ancho de la pantalla menos 10 píxeles
    int y1 = y0; // Misma coordenada Y que el punto de inicio

    // Dibujar la línea
    tft.drawLine(x0, y0, x1, y1, TFT_GREY);

    // Coordenadas para la línea
    int x2 = 10;
    int y2 = tft.height()- 25;
    int x3 = tft.width() - 10; // x1 será el ancho de la pantalla menos 10 píxeles
    int y3 = y2; // Misma coordenada Y que el punto de inicio

    // Dibujar la línea
    tft.drawLine(x2, y2, x3, y3, TFT_GREY);

    tft.setCursor((tft.width()/2)-15,(tft.height()/2)+30);
    tft.printf("km/h");
    //tft.setCursor(50,(tft.height()-16));
    //tft.printf("m");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);


}

// ************************************* Temperatura, humedad y presion ************************************************
void Display::updateSensorData() {
    // Fetch data from the sensor class
    float temperature = aht20_bmp.getTemperature();
    float humidity = aht20_bmp.getHumidity();
    float pressure = aht20_bmp.getPressure();

    // Set text font and cursor
    tft.setTextFont(2);
    tft.setCursor(10, tft.height() - 20);

    // Print sensor data to the display
    tft.printf("%.0f C | %.0f %%rH | %.0f hPa", temperature, humidity, pressure);
}

void Display::showMessage(const char* message) {
    tft.fillScreen(TFT_BLACK); // Limpiar la pantalla
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);

    tft.drawString(message, tft.width()/2, tft.height() / 2);
}

// ************************************************* GPS ******************************************************************
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
    
    
}

void Display::displayGPSData(const GPS& gps) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(1); // Restablecer la fuente del texto
    tft.setTextSize(1);

    if (gps.getYear() > 2000){

        char dateTime[50];
        sprintf(dateTime, "%02d:%02d:%02d - %02d-%02d-%04d", gps.getHour(), gps.getMinute(), gps.getSecond(), gps.getDay(), gps.getMonth(), gps.getYear());
        
        tft.setCursor(20, 7);
        tft.printf("%s\n", dateTime);
        tft.setCursor(PosSignalX + 28, PosSignalY + 2);
        tft.printf("%d\n", gps.getSatellites());
        
        tft.setCursor((tft.width() / 2), (tft.height() / 2 - 25));
        tft.setTextFont(7); // Restablecer la fuente del texto
        
        tft.printf("%.0f", gps.getSpeed());

        tft.setCursor(30, 105);
        tft.setTextFont(4); // Restablecer la fuente del texto
        tft.setTextDatum(ML_DATUM);
        tft.printf("%.0f", gps.getAltitude());
    
        // Actualizar las barras de señal
        //showBatteryStatus();
        drawSignalIcon(PosSignalX, PosSignalY, gps.getSatellites());

    }

    else{
        tft.setCursor(20, 7);
        tft.printf("Sin GPS");
    }

    tft.setCursor((tft.width() / 2 - 50), (tft.height() / 2 - 25));
    tft.setTextFont(7); // Restablecer la fuente del texto
    tft.setTextColor(TFT_GREY, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);

    tft.printf("000");

}

void Display::drawSignalIcon(int x, int y, int bars) {
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


// ************************************************* ACELEROMETRO ******************************************************************
void Display::updateMPU(const MPU6050& mpu) {
    displayMPUData(mpu);
}

void Display::displayMPUData(const MPU6050& mpu) {
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(1); // Restablecer la fuente del texto
    tft.setTextSize(1);

    
    tft.setCursor(20, 22);
    if (mpu.getAccelerationX()>1){
        tft.printf("AccX: %.2f\n", mpu.getAccelerationX());
    }
    
    tft.setCursor(20, 32);
    //tft.printf("AccY: %.2f\n", mpu.getAccelerationY());
    tft.setCursor(20, 42);
    //tft.printf("AccZ: %.2f\n", mpu.getAccelerationZ());

    //int inclinationX = mpu.getInclinationY();
    //int inclinationY = mpu.getInclinationX();
    tft.setCursor(20, 55);
    //tft.printf("X: %3.0f\n", inclinationX);
    //tft.printf("X: %.2f\n", mpu.getInclinationX());
    
    //tft.printf("X: %.2f\n", inclinationX);
    tft.setCursor(20, 65);
    //tft.printf("X: %3.0f\n", mpu.getInclinationX());
    //tft.printf("Y: %.2f\n", mpu.getInclinationY());
    
    // Mapear las inclinaciones a un índice de línea
    //Serial.println|(mpu.getAccelerationX());

   



    int lineIndexX = map(mpu.getInclinationY(), -90, 90, 0, LINE_COUNT - 1);
    int lineIndexY = map(mpu.getInclinationX(), -90, 90, 0, LINE_COUNT - 1);

    // Asegurarse de que el índice esté dentro de los límites
    lineIndexX = constrain(lineIndexX, 0, LINE_COUNT - 1);
    lineIndexY = constrain(lineIndexY, 0, LINE_COUNT - 1);

    // Limpiar las líneas anteriores
    for (int i = 0; i < LINE_COUNT; i++) {
        int currentLineLength = (i % 2 == 0) ? EXTENDED_LINE_LENGTH : LINE_LENGTH;
        tft.fillRect(verticalLineX[i] - 1, MPUcenterY - (currentLineLength / 2), 2, currentLineLength, TFT_GREY);
        tft.fillRect(MPUcenterX - (currentLineLength / 2), horizontalLineY[i] - 1, currentLineLength, 2, TFT_GREY);
    }

  // Colorear de rojo las líneas correspondientes a las inclinaciones
  int selectedLineLengthX = (lineIndexX % 2 == 0) ? EXTENDED_LINE_LENGTH : LINE_LENGTH;
  int selectedLineLengthY = (lineIndexY % 2 == 0) ? EXTENDED_LINE_LENGTH : LINE_LENGTH;

  tft.fillRect(verticalLineX[lineIndexX] - 1, MPUcenterY - (selectedLineLengthX / 2), 2, selectedLineLengthX, TFT_RED);
  tft.fillRect(MPUcenterX - (selectedLineLengthY / 2), horizontalLineY[lineIndexY] - 1, selectedLineLengthY, 2, TFT_RED);

  // Fijar posiciones independientes para imprimir inclinationY e inclinationX
  int fixedCursorXPosY = MPUcenterX + 10; // Posición para inclinationY
  int fixedCursorYPosY = MPUcenterY + 22; 

  int fixedCursorXPosX = MPUcenterX + 12; // Posición para inclinationX
  int fixedCursorYPosX = MPUcenterY - 15;

  // Imprimir inclinationY en una posición fija
  tft.setCursor(fixedCursorXPosY, fixedCursorYPosY);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.printf("%3.0f", mpu.getInclinationX());

  // Imprimir inclinationX en una posición fija
  tft.setCursor(fixedCursorXPosX, fixedCursorYPosX);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.printf("%3.0f", mpu.getInclinationY());
}



/************************************************* brujula ******************************************************************/

void Display::updateCompass(const QMC5883L& compass) {
    int azimuth = compass.getAzimuth();
    
    // Limpiar el sprite antes de dibujar
    spr.fillSprite(TFT_BLACK);

    // Dibujar un círculo azul para la rosa de los vientos
    spr.drawCircle(radius + 10, radius + 10, radius, TFT_BLUE);

    // Dibujar los puntos cardinales N, S, E, W rotados según el azimut
    drawRotatedText(spr, radius + 10, radius + 10, azimuth, "N", textRadius);
    drawRotatedText(spr, radius + 10, radius + 10, (azimuth + 180) % 360, "S", textRadius);
    drawRotatedText(spr, radius + 10, radius + 10, (azimuth + 90) % 360, "E", textRadius);
    drawRotatedText(spr, radius + 10, radius + 10, (azimuth - 90) % 360, "W", textRadius);

    // Mostrar el valor del azimut en el centro del círculo
    spr.setTextColor(TFT_WHITE);
    spr.setTextDatum(MC_DATUM);
    spr.drawString(String(int(azimuth)), radius + 10, radius + 10);

    // Dibujar la flecha que indica la orientación actual
    spr.drawLine(radius + 10, radius + 10, radius + 10, radius + 10 - radius + 10, TFT_RED);
    spr.fillTriangle(radius + 10, radius + 10 - radius + 10, radius + 5, radius + 10 - radius + 10 + 10, radius + 15, radius + 10 - radius + 10 + 10, TFT_RED);

    // Dibujar el sprite en la pantalla en las coordenadas dadas
    spr.pushSprite(centerX - textRadius - 10, centerY - textRadius - 10);
}


void Display::drawRotatedText(TFT_eSprite &spr, int cx, int cy, float angle, String text, int textRadius) {
    float rad = radians(angle);
    int tx = cx + textRadius * sin(rad);
    int ty = cy - textRadius * cos(rad);
    
    spr.setTextColor(TFT_WHITE);
    spr.setTextDatum(MC_DATUM);
    spr.drawString(text, tx, ty);
}


// ************************************************* BATERIA ******************************************************************
void Display::showBatteryStatus() {
    
    float battery_voltage = battery.getVoltage();
    int chargeLevel = battery.getChargeLevel();
    bool isCharging = battery.isCharging();

    int batteryWidth = 20;
    int batteryHeight = 10;
    int batteryX = tft.width() - batteryWidth - 5;
    int batteryY = 5;

    /// Crear una cadena para mostrar el voltaje
    char voltageStr[10];
    snprintf(voltageStr, sizeof(voltageStr), "%.2fV", battery_voltage);

    tft.setTextFont(1); // Restablecer la fuente del texto
     
    // Imprimir el voltaje en la pantalla
    tft.drawString(voltageStr, tft.width() - 40, tft.height() - 10);
    //tft.drawString(voltageStr, 300, 210);


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
