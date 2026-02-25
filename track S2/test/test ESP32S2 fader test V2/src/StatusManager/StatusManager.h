#pragma once

#include <Arduino.h>
#include "../config.h"

class StatusManager {
public:
    StatusManager();
    void begin();
    
    void setEstadoSistema(bool sistemaListo);
    void indicarCalibracionEnCurso();
    void indicarCalibracionCompletada();
    void indicarError();
    void indicarEmergencia();
    void actualizar();
    
private:
    bool _sistemaListo;
    bool _calibracionEnCurso;
    unsigned long _ultimoTiempoParpadeo;
    bool _estadoLED;
    
    void configurarLED();
    void parpadearLED(int veces, int duracion);
};