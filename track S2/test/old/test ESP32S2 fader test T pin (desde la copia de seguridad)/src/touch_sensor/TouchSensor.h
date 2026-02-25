#pragma once

#include <Arduino.h>



class TouchSensor {
public:
    // Constructor
    TouchSensor();
    
    // Inicialización
    void begin();
    
    // Calibración
    void calibrar();
    void calibrar(int nuevoUmbral);
    
    // Lectura y estado
    int leerValor();
    bool estaTocando();
    bool touchDetectado();
    
    // Diagnóstico
    void diagnosticar();
    bool sensorOk();
    
    // Getters
    int getValorBase() const { return _valorBase; }
    int getUmbral() const { return _umbral; }
    int getValorActual() const { return _valorActual; }
    bool getEstado() const { return _touchActivo; }
    bool conexionEstable();
    void verificarConexion();
    bool getEstadoConexion() const { return _conexionEstable; }

    // Nuevos métodos EEPROM
    bool cargarCalibracionEEPROM();
    void guardarCalibracionEEPROM();
    bool calibracionValida() const;
    void borrarCalibracionEEPROM();


    
private:
    // Variables miembro
    int _valorBase;
    int _umbral;
    int _valorActual;
    bool _touchActivo;
    bool _ultimoEstado;
    unsigned long _lastTouchTime;
    bool _conexionEstable;
    unsigned long _tiempoInestable;
    int _ultimoValorValido;

    int filtrarOutliers(int lecturas[], int total, int porcentajeOutliers);

    void inicializarEEPROM();
    bool _calibracionCargada;
    
    // Métodos privados (solo declaración)
    void actualizarEstado();
};
