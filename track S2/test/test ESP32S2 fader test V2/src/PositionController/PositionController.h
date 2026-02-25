#pragma once

#include <Arduino.h>
#include "../config.h"

class PositionController {
public:
    PositionController();
    void comenzar();
    
    void moverAPosicion(int posicionObjetivoADC);
    void actualizar(int posicionActualADC);
    void detener();
    bool estaEnPosicion();
    bool estaMoviendo();
    int obtenerObjetivo();
    int obtenerSalidaPWM();
    
private:
    int _objetivo;
    bool _moviendo;
    int _salidaPWM;
    
    // Para control por pasos
    unsigned long _ultimoPulso;
    unsigned long _tiempoParada;
    int _duracionPulso;
    
    void controlPorPasos(int posicionActualADC);
};