#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "../config.h"

struct DatosCalibracion {
    int posicionMinimaADC = 0;
    int posicionMaximaADC = ADC_MAX_VALUE;
    bool calibrado = false;
    String mensajeError = "";
};

class CalibrationManager {
public:
    enum EstadoCalibracion {
        INACTIVO = -1,
        BUSCANDO_MAXIMO = 0,
        BUSCANDO_MINIMO = 1,
        COMPLETADO = 2,
        ERROR = 3
    };
    
    CalibrationManager();
    void comenzar();
    bool iniciarCalibracion();
    void actualizarCalibracion(int adcActual);
    void abortarCalibracion();
    bool estaCalibrando();
    bool validarCalibracion();
    void guardarCalibracion();
    bool cargarCalibracion();
    const DatosCalibracion& obtenerDatosCalibracion();
    
    // Getters para control de posición
    int obtenerMinimoADC();
    int obtenerMaximoADC();
    int porcentajeAADC(int porcentaje);
    int adcAPorcentaje(int adc);
    
    EstadoCalibracion obtenerEstado();
    unsigned long obtenerTiempoEstable();
    int obtenerUltimoValorEstable();
    
private:
    DatosCalibracion _datosCalibracion;
    Preferences _preferencias;
    EstadoCalibracion _estado;
    unsigned long _tiempoInicioCalibracion;
    unsigned long _ultimoTiempoEstable;
    int _ultimoValorEstable;
    
    void reiniciarEstadoCalibracion();
};