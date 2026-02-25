#include <Arduino.h>
#include "Battery.h"
#include "MPU6050.h"
#include "BLE.h"
#include <BLEUtils.h>
#include <BLEServer.h>
#include "GPS.h"
extern GPS gps; // Declaración de la instancia global
float fakeLat = 36.555999;  // Latitud Madrid


// Variables will change:
extern int ledState;  // ledState used to set the LED
extern int ledPin;

// Definir las variables globales
bool deviceConnected = false;
bool oldDeviceConnected = false;
BLEServer *pServer = NULL;

// Características BLE
BLECharacteristic *pCharacteristicGPS;
BLECharacteristic *pCharacteristicBattery;

// UUIDs para el servicio y la característica de GPS
#define SERVICE_UUID "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "00002902-0000-1000-8000-00805f9b34fb"

// Callback para manejar las conexiones y desconexiones
class MyServerCallbacks : public BLEServerCallbacks {
public:
    void onConnect(BLEServer* pServer) override {
        
        deviceConnected = true;
        sendData();
        Serial.println("Dispositivo conectado");

    }

    void onDisconnect(BLEServer* pServer) override {
        deviceConnected = false;
        Serial.println("Dispositivo desconectado");
    }
};

// Función para inicializar BLE
void setupBLE() {
    BLEDevice::init("iMoto");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    
    // Crear y configurar el servicio de GPS
    BLEService *pServiceGPS = pServer->createService(BLEUUID(SERVICE_UUID));
    pCharacteristicGPS = pServiceGPS->createCharacteristic(
        BLEUUID(CHARACTERISTIC_UUID), 
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pServiceGPS->start();


    startAdvertising();
}

// Función para comenzar la publicidad BLE
void startAdvertising() {
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->start();
    Serial.println("Esperando conexión");
}

// Función para enviar datos de GPS y batería
void sendData() {
    Serial.println("********** podemos mandar datos *************");
    
    if (deviceConnected) {
        // Obtener el nivel de batería usando la instancia
        float batteryLevel = battery.getBatteryLevel();
        float batteryVoltage = battery.getVoltage();

        // Obtener los datos del GPS
        int year = gps.getYear();
        int month = gps.getMonth();
        int day = gps.getDay();
        int hour = gps.getHour();
        int minute = gps.getMinute();
        int second = gps.getSecond();
        int satellites = gps.getSatellites();
        float latitude = gps.getLatitude();
        float longitude = gps.getLongitude();
        float altitude = gps.getAltitude();
        float speed = gps.getSpeed();
        float avgSpeed = gps.getAvgSpeed();
        float maxSpeed = gps.getMaxSpeed();
        char course = gps.getCourse();

        // Crear una cadena con los datos de GPS
        String gpsData = String(latitude, 6) + 
                         "," + String(longitude, 6) + 
                         "," + String(speed, 0) + 
                         "," + String(altitude, 0) + 
                         "," + String(avgSpeed, 0) + 
                         "," + String(maxSpeed, 0) + 
                         "," + String(satellites) + 
                         "," + String(course) +
                         "," + String(year) + 
                         "," + String(month) + 
                         "," + String(day) + 
                         "," + String(hour) + 
                         "," + String(minute) + 
                         "," + String(second) +
                         "," + String(mpu.getAccelerationX(), 2) + 
                         "," + String(mpu.getAccelerationY(), 2) + 
                         "," + String(mpu.getAccelerationZ(), 2) + 
                         "," + String(mpu.getInclinationX(), 0) + 
                         "," + String(mpu.getInclinationY(), 0) + 
                         "," + String(batteryVoltage, 2) + 
                         "," + String(batteryLevel, 0);

        // Establecer el valor de la característica de GPS y notificar
        pCharacteristicGPS->setValue(gpsData.c_str());
        pCharacteristicGPS->notify();

        // Imprimir datos en el monitor serial para depuración
        Serial.println(gpsData);
        Serial.print("Battery Level: ");
        Serial.println(batteryLevel);
        Serial.print("Battery Voltage: ");
        Serial.println(batteryVoltage);

        // delay(1000); // Enviar datos cada segundo (si es necesario)
    }

    Serial.println("********** hemos mandado datos *************");
}

