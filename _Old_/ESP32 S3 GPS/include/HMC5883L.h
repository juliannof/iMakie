#ifndef QMC5883L_H
#define QMC5883L_H

#include <QMC5883LCompass.h>

class QMC5883L {
public:
    QMC5883L();
    bool begin();
    void update();
    int getAzimuth() const;  // Añadir const

private:
    QMC5883LCompass compass;
    int RawAzimuth;
    int azimuth;
};

#endif // QMC5883L_H