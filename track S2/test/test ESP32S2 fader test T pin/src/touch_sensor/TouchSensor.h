#pragma once

#include <Arduino.h>
#include <Preferences.h>  // ★★★ CAMBIAR EEPROM por Preferences ★★★
#include "../config.h"

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
    
    // ★★★ NUEVOS MÉTODOS PARA PREFERENCES ★★★
    bool cargarCalibracion();
    void guardarCalibracion();
    bool calibracionValida() const;
    void borrarCalibracion();
    
    // ★★★ NUEVOS MÉTODOS PARA BOTÓN ★★★
    bool esperandoBoton() const { return _esperandoBoton; }
    bool procesarBoton();
    bool enModoCalibracion() const { return _modoCalibracion; }
    void actualizarCalibracion();
    
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
    
    // ★★★ NUEVAS VARIABLES PARA BOTÓN Y MODO ★★★
    bool _esperandoBoton;
    bool _modoCalibracion;
    unsigned long _inicioCalibracion;
    int _muestrasCalibracion;
    int _sumaCalibracion;
    unsigned long _ultimaPulsacionBoton;
    bool _estadoBotonAnterior;
    
    // ★★★ REEMPLAZAR EEPROM CON PREFERENCES ★★★
    Preferences _preferences;
    static const char* NAMESPACE;
    static const char* KEY_BASE;
    static const char* KEY_UMBRAL;
    static const char* KEY_SIGNATURE;
    static const uint8_t SIGNATURE_VALID = 0xAA;
    
    bool _calibracionCargada;
    
    // Métodos privados
    int filtrarOutliers(int lecturas[], int total, int porcentajeOutliers);
    void actualizarEstado();
    bool leerBotonCalibracion();  // ★★★ NUEVO ★★★
    void iniciarCalibracionAutomatica();  // ★★★ NUEVO ★★★
    void finalizarCalibracionAutomatica();  // ★★★ NUEVO ★★★
};