#include "QMC5883L.h"
#include <Wire.h>

QMC5883L::QMC5883L() : azimuth(0) {
    // Constructor vacío, puedes inicializar variables aquí si es necesario
}

bool QMC5883L::begin() {
    Wire.begin();
    compass.init();  // No retorna valor
    return true;  // Asumimos que la inicialización es exitosa
}

void QMC5883L::update() {
    compass.read();
    azimuth = compass.getAzimuth();
    //compass.getDirection(direction, azimuth);
    
    //Serial.println(direction);
}

int QMC5883L::getAzimuth() const {
    Serial.println(azimuth);
    return azimuth;
}

