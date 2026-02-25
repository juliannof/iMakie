// functions.cpp - Versión corregida con fuentes funcionando
#include <WiFiManager.h>
#include <TFT_eSPI.h>
#include <SparkFun_ENS160.h>
#include <Adafruit_BME280.h>
#include <SparkFun_Qwiic_Humidity_AHT20.h>
#include <time.h>
#include <Wire.h>

// ==================== INCLUSIÓN DE FUENTES ====================
#include "fonts/NotoSansBold36.h"
#include "fonts/smallFont.h"
#include "fonts/hugeFatFont.h"

#define AA_FONT_XLARGE hugeFatFont
#define AA_FONT_LARGE NotoSansBold36
#define AA_FONT_MEDIUM NotoSansBold36
#define AA_FONT_SMALL smallFont

// ==================== DECLARACIONES FORWARD ====================

struct SensorData;
void setupDisplay();
void setupWiFi();
void setupSensors();
void setupClock();
void adjustForDaylightSaving(struct tm &timeinfo);
void updateCurrentTime();
void displaySensorData(const SensorData& data);
SensorData collectSensorData();
void readAndDisplayData();
void logSensorData(const SensorData& data);

// ==================== DEFINICIÓN DE STRUCT ====================

struct SensorData {
    float pressure;
    uint8_t aqi;
    uint16_t tvoc;
    uint16_t eco2;
    float temperatureAHT20;
    float humidityAHT20;
    float temperatureBME280;
    float humidityBME280;
};

// ==================== VARIABLES GLOBALES ====================

// Objetos de pantalla ORIGINALES (como tu código)
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);  // Sprite principal (como tu código original)

// Sensores
SparkFun_ENS160 ens160;
Adafruit_BME280 bme280;
AHT20 aht20;

// Buffers de tiempo
char currentTime[10] = "";
char currentDate[12] = "";

// Estado para control de actualizaciones
struct DisplayState {
    char lastTime[10];
    char lastDate[12];
    uint8_t lastAQI;
    float lastTemp;
    float lastHum;
    bool compensationShown;
    uint32_t lastWarmupAnimation;
    uint8_t warmupDots;
};

DisplayState displayState = {
    "",     // lastTime
    "",     // lastDate
    0,      // lastAQI
    0.0f,   // lastTemp
    0.0f,   // lastHum
    false,  // compensationShown
    0,      // lastWarmupAnimation
    0       // warmupDots
};

// ==================== FUNCIONES AUXILIARES ====================

void adjustForDaylightSaving(struct tm &timeinfo) {
    struct tm startDST = timeinfo;
    startDST.tm_mon = 2;
    startDST.tm_mday = 31;
    mktime(&startDST);
    while (startDST.tm_wday != 0) {
        startDST.tm_mday--;
        mktime(&startDST);
    }
    
    struct tm endDST = timeinfo;
    endDST.tm_mon = 9;
    endDST.tm_mday = 31;
    mktime(&endDST);
    while (endDST.tm_wday != 0) {
        endDST.tm_mday--;
        mktime(&endDST);
    }
    
    time_t now = mktime(&timeinfo);
    time_t startDSTTime = mktime(&startDST);
    time_t endDSTTime = mktime(&endDST);
    
    if (now >= startDSTTime && now < endDSTTime && timeinfo.tm_isdst == 0) {
        timeinfo.tm_hour += 1;
        timeinfo.tm_isdst = 1;
    }
}

// ==================== FUNCIONES DE INICIALIZACIÓN ====================

void setupDisplay() {
    Wire.begin();
    tft.init();
    digitalWrite(TFT_BL, LOW);
    
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    
    tft.setCursor(0, 0);
    tft.println("Display Initialized");
    Serial.println("Display initialized correctly.");
    delay(500);
    
    // Escáner I2C
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.println("I2C Scanner:");
    Serial.println("I2C Scanner:");
    
    byte error, address;
    int nDevices = 0;
    
    for (address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        
        if (error == 0) {
            tft.printf("0x%02X\n", address);
            Serial.printf("I2C device found at address 0x%02X\n", address);
            nDevices++;
        } else if (error == 4) {
            Serial.printf("Unknown error at address 0x%02X\n", address);
        }
    }
    
    if (nDevices == 0) {
        tft.println("No I2C devices found");
        Serial.println("No I2C devices found");
    } else {
        tft.printf("%d devices found\n", nDevices);
        Serial.printf("%d devices found\n", nDevices);
    }
    
    delay(200);
    tft.fillScreen(TFT_BLACK);
}

void setupWiFi() {
    WiFiManager wm;
    
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(0, 0);
    
    tft.println("Iniciando WiFi...");
    Serial.println("Iniciando WiFi...");
    
    wm.setDebugOutput(true);
    wm.setConnectTimeout(30);
    wm.setConfigPortalTimeout(120);
    
    tft.println("Conectando...");
    Serial.println("Conectando...");
    
    bool res = wm.autoConnect("ESP32-C3-Config");
    
    if (!res) {
        tft.println("Error de conexion");
        Serial.println("Falló la conexión y el portal de configuración");
        delay(2000);
        ESP.restart();
    } else {
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0, 0);
        tft.println("Conectado a WiFi!");
        tft.print("IP: ");
        tft.println(WiFi.localIP());
        
        Serial.println("\nConectado a WiFi!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        delay(1000);
    }
}

void setupSensors() {
    Wire.begin();
    
    bool sensorsInitialized = true;
    
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    
    if (!bme280.begin(0x77)) {
        Serial.println("No se pudo encontrar un sensor BME280 en la dirección 0x77!");
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.println("Error: BME280");
        sensorsInitialized = false;
    }
    
    if (!ens160.begin()) {
        Serial.println("No se pudo inicializar el sensor ENS160.");
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.println("Error: ENS160");
        sensorsInitialized = false;
    }
    
    if (!aht20.begin()) {
        Serial.println("No se pudo inicializar el sensor AHT20.");
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.println("Error: AHT20");
        sensorsInitialized = false;
    }
    
    if (sensorsInitialized) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(0, 0);
        tft.println("Sensors Initialized");
        Serial.println("Sensors initialized correctly.");
        
        // Reset ENS160
        if (ens160.setOperatingMode(SFE_ENS160_RESET)) {
            tft.println("ENS160 Reset");
            delay(100);
            ens160.setOperatingMode(SFE_ENS160_STANDARD);
            tft.println("Standard mode activated");
        }
    }
    delay(1000);
    tft.fillScreen(TFT_BLACK);
}

void setupClock() {
    const long gmtOffset_sec = 3600;
    const int daylightOffset_sec = 3600;
    
    configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
    
    Serial.print("Waiting for time");
    time_t now;
    struct tm timeinfo;
    
    while (true) {
        now = time(nullptr);
        localtime_r(&now, &timeinfo);
        
        if (timeinfo.tm_year >= (2023 - 1900)) {
            break;
        }
        Serial.print(".");
        delay(1000);
    }
    Serial.println("");
    
    adjustForDaylightSaving(timeinfo);
    
    strftime(currentTime, sizeof(currentTime), "%H:%M:%S", &timeinfo);
    strftime(currentDate, sizeof(currentDate), "%d-%m-%Y", &timeinfo);
    
    Serial.printf("Current time: %s\n", currentTime);
    Serial.printf("Current date: %s\n", currentDate);
}

// ==================== FUNCIONES DE VISUALIZACIÓN ====================

void updateCurrentTime() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    adjustForDaylightSaving(timeinfo);
    
    strftime(currentTime, sizeof(currentTime), "%H:%M:%S", &timeinfo);
    strftime(currentDate, sizeof(currentDate), "%d-%m-%Y", &timeinfo);
}

void logSensorData(const SensorData& data) {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    char serverTime[9];
    strftime(serverTime, sizeof(serverTime), "%H:%M:%S", &timeinfo);
    
    adjustForDaylightSaving(timeinfo);
    
    char correctedTime[9];
    strftime(correctedTime, sizeof(correctedTime), "%H:%M:%S", &timeinfo);
    
    Serial.printf("Server Time: %s, Corrected Time: %s, Date: %s\n", 
                  serverTime, correctedTime, currentDate);
    Serial.printf("Temp AHT20: %.2f C, Hum AHT20: %.2f %%, Temp BME280: %.2f C, Hum BME280: %.2f %%, Pres: %.2f hPa, AQI: %d, TVOC: %d ppb, eCO2: %d ppm\n",
                  data.temperatureAHT20, data.humidityAHT20, 
                  data.temperatureBME280, data.humidityBME280, 
                  data.pressure, data.aqi, data.tvoc, data.eco2);
}

void displaySensorData(const SensorData& data) {
    updateCurrentTime();
    
    // --- Área de hora y fecha ---
    static char lastShownTime[20] = "";
    static char lastShownDate[20] = "";
    
    if (strcmp(currentTime, lastShownTime) != 0 || 
        strcmp(currentDate, lastShownDate) != 0) {
        
        tft.fillRect(0, 0, tft.width(), 25, TFT_BLACK); // Área más grande para fuentes
        
        // Hora con fuente MEDIUM
        tft.loadFont(AA_FONT_MEDIUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(5, 0);
        tft.print(currentTime);
        tft.unloadFont();
        
        // Fecha con fuente SMALL
        tft.loadFont(AA_FONT_SMALL);
        tft.setCursor(5, 18);
        tft.print(currentDate);
        tft.unloadFont();
        
        strcpy(lastShownTime, currentTime);
        strcpy(lastShownDate, currentDate);
    }
    
    // --- AQI numérico ---
    static uint8_t lastShownAQI = 0;
    if (data.aqi != lastShownAQI) {
        tft.fillRect(190, 203, 40, 20, TFT_BLACK);
        tft.loadFont(AA_FONT_SMALL);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(190, 203);
        tft.printf("AQI:%d", data.aqi);
        tft.unloadFont();
        lastShownAQI = data.aqi;
    }
    
    // --- Círculo AQI ---
    static uint8_t lastAQI = 0;
    uint16_t color;
    if (data.aqi != lastAQI) {
        switch (data.aqi) {
            case 5: color = TFT_RED; break;
            case 4: color = TFT_ORANGE; break;
            case 3: color = TFT_YELLOW; break;
            case 2: color = TFT_GREEN; break;
            case 1: color = TFT_BLUE; break;
            default: color = TFT_WHITE; break;
        }
        
        tft.fillCircle(tft.width() - 30, tft.height() - 30, 15, color);
        lastAQI = data.aqi;
    }
    
    // Calcular temperaturas promediadas
    float tempM;
    if (fabs(data.temperatureAHT20 - data.temperatureBME280) > 2.0) {
        tempM = data.temperatureBME280;
    } else {
        tempM = (data.temperatureAHT20 + data.temperatureBME280) / 2;
    }
    
    // Calcular humedades promediadas
    const float humidityThreshold = 6.0;
    float humM;
    if (fabs(data.humidityAHT20 - data.humidityBME280) > humidityThreshold) {
        humM = data.humidityBME280;
    } else {
        humM = (data.humidityAHT20 + data.humidityBME280) / 2;
    }
    
    // Compensación ambiental
    static bool compensationSet = false;
    static bool compensationShown = false;
    if (!compensationSet) {
        bool tempSuccess = ens160.setTempCompensationCelsius(tempM);
        bool humSuccess = ens160.setRHCompensationFloat(humM);
        
        if (tempSuccess && humSuccess) {
            compensationSet = true;
            compensationShown = false;
        }
    }
    
    // Estado ENS160
    tft.fillRect(0, 209, tft.width(), 10, TFT_BLACK);
    tft.setCursor(0, 209);
    
    if (ens160.checkDataStatus()) {
        if (!compensationShown) {
            compensationShown = true;
        }
    } else {
        static uint8_t warmupDots = 0;
        for (int i = 0; i < warmupDots; i++) tft.print(".");
        warmupDots = (warmupDots + 1) % 4;
    }
    
    // Calcular ángulo para el arco de humedad
    int endAngleHum = (humM * 315) / 100;
    
    // Crear y dibujar sprite PRINCIPAL
    spr.createSprite(240, 180);
    spr.fillSprite(TFT_BLACK);
    
    // Arco base
    spr.drawSmoothArc(tft.width() / 2, 95, 85, 70, 45, 315, 0x18e3, 0x0000, true);
    
    // Arco de humedad con color condicional
    if (humM <= 60) {
        spr.drawSmoothArc(tft.width() / 2, 95, 85, 70, 45, endAngleHum, 0x77e0, 0x0000, true);
    } else {
        spr.drawSmoothArc(tft.width() / 2, 95, 85, 70, 45, endAngleHum, 0xf800, 0x0000, true);
    }
    
    // Mostrar temperatura con fuente XLARGE (hugeFatFont)
    int tempMInt = static_cast<int>(tempM);
    
    // Temperatura con Font 7 (Segmentos de 48px)
    spr.setTextFont(7); // Usar Font 7 (siete segmentos)
    spr.setTextSize(1); // Tamaño es 1 para fuentes bitmap. Usa 'setTextSize()' para escalar si es necesario.
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.setTextDatum(TC_DATUM);
    
    // Dibujar número de temperatura
    spr.drawString(String(tempMInt), tft.width() / 2, 45);
    spr.unloadFont();
    
    // Símbolo de grados pequeño
    spr.loadFont(AA_FONT_SMALL);
    spr.drawString("o", tft.width() / 2 + 50, 40);
    spr.unloadFont();
    
    // Letra "C" al lado
    spr.loadFont(AA_FONT_SMALL);
    spr.drawString("C", tft.width() / 2 + 60, 40);
    spr.unloadFont();
    
    // Mostrar humedad con fuente LARGE (NotoSansBold36)
    spr.loadFont(AA_FONT_LARGE);
    spr.drawString(String(static_cast<int>(humM)) + "%", tft.width() / 2, 120);
    spr.unloadFont();
    
    // Actualizar sprite en pantalla
    spr.pushSprite(0, 28);
    spr.deleteSprite();
}

SensorData collectSensorData() {
    SensorData data;
    data.pressure = bme280.readPressure() / 100.0F;
    data.aqi = ens160.getAQI();
    data.tvoc = ens160.getTVOC();
    data.eco2 = ens160.getECO2();
    data.temperatureAHT20 = aht20.getTemperature() - 7;
    data.humidityAHT20 = aht20.getHumidity();
    data.temperatureBME280 = bme280.readTemperature() - 7;
    data.humidityBME280 = bme280.readHumidity();
    return data;
}

void readAndDisplayData() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    adjustForDaylightSaving(timeinfo);
    
    strftime(currentTime, sizeof(currentTime), "%H:%M:%S", &timeinfo);
    strftime(currentDate, sizeof(currentDate), "%d-%m-%Y", &timeinfo);
    
    SensorData data = collectSensorData();
    displaySensorData(data);
    logSensorData(data);
}