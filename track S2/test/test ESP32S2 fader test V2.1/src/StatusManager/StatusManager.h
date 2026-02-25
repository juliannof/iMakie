#pragma once

#include <Arduino.h>
#include "../config.h" // Se asume que config.h está un nivel arriba
  
class StatusManager {
public:
    StatusManager();
    void begin();
      
    // Estados del sistema
    void setEstadoSistema(bool sistemaListo);      // Indica si el sistema está operativo
    void indicarCalibracionEnCurso();              // LED parpadeando para calibración
    void indicarCalibracionCompletada();           // Patrón de LED para calibración OK
    void indicarError();                           // Patrón de LED para error
    void indicarEmergencia();                      // Patrón de LED para parada de emergencia
      
    // Para patrones continuos (debe llamarse en el loop principal)
    void actualizar();
      
private:
    bool _sistemaListo;         // Estado general de listo/no listo
    bool _calibracionEnCurso;   // Flag para el parpadeo de calibración
    unsigned long _ultimoTiempoParpadeo; // Para controlar el ritmo del parpadeo
    bool _estadoLED;            // Estado actual del LED (ON/OFF)
    int _contadorParpadeo;      // Utilizado para patrones de parpadeo específicos
      
    void configurarLED();       // Configura el pin del LED
    void parpadearLED(int veces, int duracion); // Patrón de parpadeo bloqueante
};