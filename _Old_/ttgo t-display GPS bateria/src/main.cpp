#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <time.h>

// Pines y objetos
#define BATTERY_PIN 36        // Pin analógico donde está conectada la batería
#define MY_CS       33        // Pin CS de la tarjeta SD
#define MY_SCLK     25        // Pin SCLK de la tarjeta SD
#define MY_MISO     27        // Pin MISO de la tarjeta SD
#define MY_MOSI     26        // Pin MOSI de la tarjeta SD

TFT_eSPI tft = TFT_eSPI();    // Inicializa la pantalla TFT

// Configuración WiFi y NTP
const char* ssid     = "Julianno-WiFi";       
const char* password = "JULIANf1";           
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;      
const int daylightOffset_sec = 3600;

// Variables globales
const int loggingInterval = 5 * 60 * 1000;  // Guardar datos cada 5 minutos (en milisegundos)
unsigned long lastLogTime = 0;
unsigned long lastReadTime = 0;
const int readInterval = 1000;  // Leer la batería cada 1 segundo
String logData = "";
const float batteryThreshold = 3.2; // Umbral de descarga (en voltios)
bool lowBatteryMode = false;   // Activar si el voltaje cae bajo el umbral

SPIClass hspi(HSPI);  // Crear instancia HSPI

// Inicializar la tarjeta SD en el bus HSPI
void initSD() {
  hspi.begin(MY_SCLK, MY_MISO, MY_MOSI, MY_CS);  // Configurar pines HSPI

  if (!SD.begin(MY_CS, hspi)) {
    Serial.println("No se pudo inicializar la tarjeta SD en el bus HSPI.");
    return;
  }
  Serial.println("Tarjeta SD inicializada correctamente en el bus HSPI.");
}

// Obtener la hora actual en formato "YYYY-MM-DD HH:MM:SS" con ajuste para el horario de verano
String getTimeStamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Error al obtener la hora";
  }

  // Sumar una hora para el horario de verano
  if (timeinfo.tm_isdst > 0) {
    timeinfo.tm_hour += 1;  // Ajustar la hora
    mktime(&timeinfo);      // Recalcular la estructura tm para manejar correctamente la suma de la hora
  }

  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}


// Guardar datos en la tarjeta SD
void saveToSD(String data) {
  File file = SD.open("/log.txt", FILE_APPEND);
  if (!file) {
    Serial.println("Error al abrir el archivo");
    return;
  }
  file.println(data);
  file.close();
  Serial.println("Datos guardados en SD");
}

// Leer el valor de la batería
float readBatteryLevel() {
    int analogValue = analogRead(BATTERY_PIN);
    float voltage = analogValue * (3.3 / 4095.0);  // Convertir el valor ADC a voltaje (0-3.3V)
    float batteryVoltage = voltage * 3.127 * 1.134; // Ajuste por el divisor resistivo y el factor de corrección
    return batteryVoltage;
}



// Obtener el tiempo encendido en formato horas:minutos:segundos
String getUptime() {
  unsigned long currentMillis = millis();
  unsigned long seconds = currentMillis / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  seconds = seconds % 60;
  minutes = minutes % 60;

  char uptimeString[20];
  sprintf(uptimeString, "%02lu:%02lu:%02lu", hours, minutes, seconds);
  return String(uptimeString);
}

void setup() {
  Serial.begin(115200);

  // Inicializar TFT
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);

  // Conectar al Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando al WiFi...");
  }
  Serial.println("Conectado al WiFi");

  // Configurar la hora desde NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Error al obtener la hora");
    return;
  }
  Serial.println("Hora sincronizada desde NTP");

  

  // Inicializar SD
  initSD();
}

void loop() {
  unsigned long currentTime = millis();

  // Leer el nivel de la batería cada 1 segundo
  if (currentTime - lastReadTime >= readInterval) {
    lastReadTime = currentTime;
    
    // Leer nivel de batería
    float batteryLevel = readBatteryLevel();
    
    // Obtener el tiempo encendido
    String uptime = getUptime();
    
    // Obtener la hora actual para mostrar en la pantalla
    String timeStamp = getTimeStamp();

    // Limpiar sólo las áreas que se van a actualizar
    tft.fillRect(20, 10, 160, 20, TFT_BLACK);  // Limpiar la parte de la hora
    tft.setCursor(20, 10);
    tft.printf("%s", timeStamp.c_str());  // Hora en la parte superior
    
    tft.fillRect(10, 40, 160, 20, TFT_BLACK);  // Limpiar la parte del voltaje de la batería
    tft.setCursor(10, 40);
    tft.printf("Bateria: %.2fV", batteryLevel);
    
    tft.fillRect(10, 70, 160, 20, TFT_BLACK);  // Limpiar la parte del uptime
    tft.setCursor(10, 70);
    tft.printf("Uptime: %s", uptime.c_str());

    // Agregar lectura al buffer de datos para guardar
    logData += timeStamp + " - Bateria: " + String(batteryLevel) + "V - Uptime: " + uptime + "\n";
    
    // Verificar si el nivel de batería está bajo el umbral
    if (batteryLevel < batteryThreshold) {
      lowBatteryMode = true;
    } else {
      lowBatteryMode = false;
    }
  }

  // Guardar datos en la SD si la batería está baja o cada 5 minutos
  if (lowBatteryMode || (currentTime - lastLogTime >= loggingInterval)) {
    lastLogTime = currentTime;

    // Guardar buffer de datos en SD
    saveToSD(logData);
    
    // Limpiar el buffer de datos
    logData = "";
  }
}
