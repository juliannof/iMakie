#ifndef MPU6500_H
#define MPU6500_H

#include <MPU9250_asukiaaa.h>
#include "Config.h"

// Declaración de la clase MPU6500
class MPU6500 {
public:
    MPU6500(); // Constructor
    void setup(); // Inicializa el sensor
    void update(); // Actualiza los datos del sensor
    
    // Métodos para obtener datos
    float getAccelerationX() const;
    float getAccelerationY() const;
    float getAccelerationZ() const;
    float getGyroX() const;
    float getGyroY() const;
    float getGyroZ() const;

private:
    MPU9250_asukiaaa mySensor; // Instancia de MPU9250_asukiaaa
    float aX, aY, aZ; // Variables para la aceleración
    float gX, gY, gZ; // Variables para el giro
    unsigned long lastUpdateTime; // Para controlar la frecuencia de actualización
};

// Declaración externa del objeto MPU6500
extern MPU6500 mpu; // Solo declaración, no definición

#endif // MPU6500_H