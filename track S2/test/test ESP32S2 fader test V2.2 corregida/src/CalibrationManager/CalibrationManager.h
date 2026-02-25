#pragma once

#include <Arduino.h>
#include <Preferences.h> // Para guardar en NVS del ESP32
#include "../config.h"   // Parámetros de configuración
  
// Estructura para almacenar los datos de calibración
struct DatosCalibracion {
    int posicionMinimaADC = 0;
    int posicionMaximaADC = ADC_MAX_VALUE;
    bool calibrado = false;
    String mensajeError = ""; // Para almacenar mensajes de error específicos
};
  
class CalibrationManager {
public:
    // Enumeración de los estados de la calibración
    enum EstadoCalibracion {
        INACTIVO = -1,         // No hay calibración en curso
        BUSCANDO_MAXIMO = 0,   // Buscando el límite superior
        BUSCANDO_MINIMO = 1,   // Buscando el límite inferior
        COMPLETADO = 2,        // Calibración finalizada con éxito
        ERROR = 3              // Calibración finalizada por error (timeout, invalidez)
    };
      
    CalibrationManager();
    void comenzar();               // Inicializador para preferencias NVS
    bool iniciarCalibracion();     // Inicia el proceso de calibración
    void actualizarCalibracion(int adcActual); // Actualiza el estado de la calibración
    void abortarCalibracion();     // Aborta una calibración en curso
    bool estaCalibrando() const;   // Devuelve true si la calibración está activa
    bool validarCalibracion();     // Verifica la validez de los límites encontrados
    void guardarCalibracion();     // Guarda los datos de calibración en NVS
    bool cargarCalibracion();      // Carga los datos de calibración desde NVS
    const DatosCalibracion& obtenerDatosCalibracion() const; // Devuelve los datos actuales
      
    // Getters para el estado interno
    EstadoCalibracion obtenerEstado() const;
    unsigned long obtenerTiempoEstable() const;
    int obtenerUltimoValorEstable() const;
      
private:
    DatosCalibracion _datosCalibracion;  // Estructura con los límites e estado
    Preferences _preferencias;           // Objeto para almacenamiento NVS
    EstadoCalibracion _estado;           // Estado actual de la calibración
    unsigned long _tiempoInicioCalibracion; // Tiempo de inicio del proceso total
    unsigned long _ultimoTiempoEstable;     // Tiempo en que el valor ADC fue estable por última vez
    int _ultimoValorEstable;                // Valor ADC que se consideró estable
      
    void reiniciarEstadoCalibracion(); // Restablece variables internas de la calibración
};