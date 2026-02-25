#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "Config.h"


// Configuración de SDSPI para usar HSPI
SPIClass SDSPI(HSPI);

void setup() {
    // Iniciar comunicación serie para depuración
    Serial.begin(115200);
    delay(1000);

    // Configurar pines para HSPI
    SDSPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);

    // Inicializar SD
    if (!SD.begin(SD_CS, SDSPI)) {
        Serial.println("Falló la inicialización de la tarjeta SD");
        return;
    }
    Serial.println("Tarjeta SD inicializada correctamente");
}

void loop() {
    // Abrir archivo para escribir
    File file = SD.open("/test.txt", FILE_WRITE);

    if (file) {
        file.println("Hola desde ESP32 con HSPI y SD!");
        file.close();
        Serial.println("Archivo escrito con éxito");
    } else {
        Serial.println("Error al abrir el archivo para escribir");
    }

    // Abrir archivo para leer
    file = SD.open("/test.txt");
    if (file) {
        Serial.println("Contenido del archivo:");
        while (file.available()) {
            Serial.write(file.read());
        }
        file.close();
    } else {
        Serial.println("Error al abrir el archivo para leer");
    }

    delay(2000); // Esperar 2 segundos antes de repetir el loop
}

/*

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <TFT_eSPI.h> // Incluimos la biblioteca TFT_eSPI

// Pines para RX y TX
static const int RXPin = 25, TXPin = 27;
static const uint32_t GPSBaud = 9600;

// Objeto TinyGPSPlus
TinyGPSPlus gps;

// Conexión Serial al dispositivo GPS
HardwareSerial ss(1); // Usando Serial1 en el ESP32

// Crear una instancia de TFT_eSPI
TFT_eSPI tft = TFT_eSPI();

// Declaración de funciones
static void printFloat(float val, bool valid, int len, int prec, int x, int y);
static void printInt(unsigned long val, bool valid, int len, int x, int y);
static void printDateTime(TinyGPSDate &d, TinyGPSTime &t, int x, int y);
static void printStr(const char *str, int len, int x, int y);
static void smartDelay(unsigned long ms);

void setup()
{
  Serial.begin(115200);
  ss.begin(GPSBaud, SERIAL_8N1, RXPin, TXPin);

  // Inicializar la pantalla TFT
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // Configurar el texto en la TFT
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);

  // Información inicial en TFT y Serial
  tft.setCursor(0, 0);
  tft.println(F("FullExample.ino"));
  tft.println(F("An extensive example of TinyGPSPlus"));
  tft.print(F("TinyGPSPlus v. "));
  tft.println(TinyGPSPlus::libraryVersion());
  tft.println(F("by Mikal Hart"));
  
  // Encabezados para los datos
  tft.println(F("Sats HDOP  Latitude   Longitude   Fix  Date       Time"));
}

void loop()
{
  static const double LONDON_LAT = 51.508131, LONDON_LON = -0.128002;

  // Actualizar datos en posiciones específicas
  printInt(gps.satellites.value(), gps.satellites.isValid(), 5, 0, 50);
  printFloat(gps.hdop.hdop(), gps.hdop.isValid(), 6, 1, 60, 50);
  printFloat(gps.location.lat(), gps.location.isValid(), 11, 6, 120, 50);
  printFloat(gps.location.lng(), gps.location.isValid(), 12, 6, 240, 50);
  printInt(gps.location.age(), gps.location.isValid(), 5, 360, 50);
  printDateTime(gps.date, gps.time, 420, 50);
  
  // Mostrar más datos adicionales si lo deseas (altura, velocidad, etc.)
  printFloat(gps.altitude.meters(), gps.altitude.isValid(), 7, 2, 0, 70);
  printFloat(gps.course.deg(), gps.course.isValid(), 7, 2, 80, 70);
  printFloat(gps.speed.kmph(), gps.speed.isValid(), 6, 2, 160, 70);
  printStr(gps.course.isValid() ? TinyGPSPlus::cardinal(gps.course.deg()) : "*** ", 6, 240, 70);
  
  unsigned long distanceKmToLondon =
    (unsigned long)TinyGPSPlus::distanceBetween(
      gps.location.lat(),
      gps.location.lng(),
      LONDON_LAT, 
      LONDON_LON) / 1000;
  printInt(distanceKmToLondon, gps.location.isValid(), 9, 320, 70);

  double courseToLondon =
    TinyGPSPlus::courseTo(
      gps.location.lat(),
      gps.location.lng(),
      LONDON_LAT, 
      LONDON_LON);
  printFloat(courseToLondon, gps.location.isValid(), 7, 2, 400, 70);

  const char *cardinalToLondon = TinyGPSPlus::cardinal(courseToLondon);
  printStr(gps.location.isValid() ? cardinalToLondon : "*** ", 6, 480, 70);

  printInt(gps.charsProcessed(), true, 6, 560, 70);
  printInt(gps.sentencesWithFix(), true, 10, 640, 70);
  printInt(gps.failedChecksum(), true, 9, 720, 70);

  Serial.println();
  smartDelay(1000);

  if (millis() > 5000 && gps.charsProcessed() < 10) {
    Serial.println(F("No GPS data received: check wiring"));
    tft.setCursor(0, 90);
    tft.println(F("No GPS data received: check wiring"));
  }
}

// Esta versión personalizada de delay() asegura que el objeto GPS esté siendo "alimentado".
static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do 
  {
    while (ss.available())
      gps.encode(ss.read());
  } while (millis() - start < ms);
}

static void printFloat(float val, bool valid, int len, int prec, int x, int y)
{
  tft.setCursor(x, y);
  if (!valid)
  {
    while (len-- > 1) {
      Serial.print('*');
      tft.print('*');
    }
    Serial.print(' ');
    tft.print(' ');
  }
  else
  {
    Serial.print(val, prec);
    tft.print(val, prec);
    int vi = abs((int)val);
    int flen = prec + (val < 0.0 ? 2 : 1); // . y -
    flen += vi >= 1000 ? 4 : vi >= 100 ? 3 : vi >= 10 ? 2 : 1;
    for (int i=flen; i<len; ++i) {
      Serial.print(' ');
      tft.print(' ');
    }
  }
  smartDelay(0);
}

static void printInt(unsigned long val, bool valid, int len, int x, int y)
{
  tft.setCursor(x, y);
  char sz[32] = "*****************";
  if (valid)
    sprintf(sz, "%ld", val);
  sz[len] = 0;
  for (int i=strlen(sz); i<len; ++i)
    sz[i] = ' ';
  if (len > 0) 
    sz[len-1] = ' ';
  Serial.print(sz);
  tft.print(sz);
  smartDelay(0);
}

static void printDateTime(TinyGPSDate &d, TinyGPSTime &t, int x, int y)
{
  tft.setCursor(x, y);
  if (!d.isValid())
  {
    Serial.print(F("********** "));
    tft.print(F("********** "));
  }
  else
  {
    char sz[32];
    sprintf(sz, "%02d/%02d/%02d ", d.month(), d.day(), d.year());
    Serial.print(sz);
    tft.print(sz);
  }
  
  if (!t.isValid())
  {
    Serial.print(F("******** "));
    tft.print(F("******** "));
  }
  else
  {
    char sz[32];
    sprintf(sz, "%02d:%02d:%02d ", t.hour(), t.minute(), t.second());
    Serial.print(sz);
    tft.print(sz);
  }

  printInt(d.age(), d.isValid(), 5, x + 60, y);
  smartDelay(0);
}

static void printStr(const char *str, int len, int x, int y)
{
  tft.setCursor(x, y);
  int slen = strlen(str);
  for (int i=0; i<len; ++i) {
    Serial.print(i<slen ? str[i] : ' ');
    tft.print(i<slen ? str[i] : ' ');
  }
  smartDelay(0);
}
*/