#include <Arduino.h>
#include "config.h"
#include "MotorController/MotorController.h"
#include "FaderSensor/FaderSensor.h"
#include "CalibrationManager/CalibrationManager.h"
#include "StatusManager/StatusManager.h"
#include "PositionController/PositionController.h"

// Instancias globales
MotorController motor;
FaderSensor sensor;
CalibrationManager calibrador;
StatusManager indicador;
PositionController controladorPosicion;

// Variables de estado
unsigned long ultimoDisplayMs = 0;
bool sistemaListo = false;

// Declaraciones de funciones
void manejarComandosSerial();
void manejarCalibracion();
void actualizarDisplay();

void setup() {
    Serial.begin(115200);
    
    // Inicializar módulos
    motor.begin();
    sensor.begin();
    calibrador.comenzar();
    indicador.begin();
    controladorPosicion.comenzar();
    
    Serial.println("\n=== SISTEMA FADER MOTORIZADO ===");
    
    // Verificar calibración existente
    if (calibrador.obtenerDatosCalibracion().calibrado) {
        Serial.println("✅ Calibración cargada desde memoria");
        sistemaListo = true;
        indicador.setEstadoSistema(true);
    } else {
        Serial.println("⚠️  Sistema no calibrado - Usar comando 'c'");
        indicador.setEstadoSistema(false);
    }
    
    Serial.println("\n💡 COMANDOS DISPONIBLES:");
    Serial.println("   'c' - Iniciar calibración");
    Serial.println("   'x' - Parada de emergencia");
    Serial.println("   'e' - Habilitar motor");
    Serial.println("   'd' - Deshabilitar motor");
    Serial.println("   'f' - Mover adelante");
    Serial.println("   'b' - Mover atrás");
    Serial.println("   's' - Parar motor");
    Serial.println("   'p X' - Mover a porcentaje X (0-100%)");
    Serial.println("   'a X' - Mover a ADC X");
    Serial.println("   'm' - Detener movimiento posicional");
    Serial.println("   '?' - Estado del sistema");
}

void loop() {
    manejarComandosSerial();
    manejarCalibracion();
    actualizarDisplay();
    indicador.actualizar();
    
    // Actualizar control de posición
    if (controladorPosicion.estaMoviendo()) {
        int adcActual = sensor.leerSuavizado();
        controladorPosicion.actualizar(adcActual);
        motor.mover(controladorPosicion.obtenerSalidaPWM());
    }
    
    delay(10);
}

void manejarComandosSerial() {
    if (!Serial.available()) return;
    
    char comando = Serial.read();
    
    switch (comando) {
        case 'c': // Iniciar calibración
            if (calibrador.iniciarCalibracion()) {
                motor.habilitar();
                motor.mover(MIN_PWM_DRIVE);
                indicador.indicarCalibracionEnCurso();
            }
            break;
            
        case 'x': // Parada de emergencia
            motor.paradaEmergencia();
            calibrador.abortarCalibracion();
            controladorPosicion.detener();
            indicador.indicarEmergencia();
            break;
            
        case 'e': // Habilitar motor
            motor.habilitar();
            break;
            
        case 'd': // Deshabilitar motor
            motor.deshabilitar();
            break;
            
        case 'f': // Adelante
            motor.mover(MIN_PWM_DRIVE);
            break;
            
        case 'b': // Atrás
            motor.mover(-MIN_PWM_DRIVE);
            break;
            
        case 's': // Parar
            motor.mover(0);
            break;
            
        case 'p': { // Mover a porcentaje
            int porcentaje = Serial.parseInt();
            if (calibrador.obtenerDatosCalibracion().calibrado) {
                int adcObjetivo = calibrador.porcentajeAADC(porcentaje);
                controladorPosicion.moverAPosicion(adcObjetivo);
                motor.habilitar();
                Serial.printf("🎯 Moviendo a %d%% (ADC: %d)\n", porcentaje, adcObjetivo);
            } else {
                Serial.println("❌ Sistema no calibrado");
            }
            break;
        }
        
        case 'a': { // Mover a ADC específico
            int adcObjetivo = Serial.parseInt();
            controladorPosicion.moverAPosicion(adcObjetivo);
            motor.habilitar();
            Serial.printf("🎯 Moviendo a ADC: %d\n", adcObjetivo);
            break;
        }
        
        case 'm': // Detener movimiento posicional
            controladorPosicion.detener();
            motor.mover(0);
            break;
            
        case '?': // Estado del sistema
            Serial.println("\n=== ESTADO DEL SISTEMA ===");
            Serial.printf("   Motor: %s\n", motor.estaHabilitado() ? "HABILITADO" : "DESHABILITADO");
            Serial.printf("   Moviendo: %s\n", motor.estaMoviendo() ? "SI" : "NO");
            Serial.printf("   Calibrado: %s\n", calibrador.obtenerDatosCalibracion().calibrado ? "SI" : "NO");
            Serial.printf("   Calibrando: %s\n", calibrador.estaCalibrando() ? "SI" : "NO");
            Serial.printf("   Movimiento posicional: %s\n", controladorPosicion.estaMoviendo() ? "SI" : "NO");
            Serial.printf("   ADC actual: %d\n", sensor.leerSuavizado());
            break;
    }
}

void manejarCalibracion() {
    if (calibrador.estaCalibrando()) {
        int adcActual = sensor.leerSuavizado();
        calibrador.actualizarCalibracion(adcActual);
        
        auto estado = calibrador.obtenerEstado();
        if (estado == CalibrationManager::BUSCANDO_MAXIMO) {
            motor.mover(MIN_PWM_DRIVE);
        } else if (estado == CalibrationManager::BUSCANDO_MINIMO) {
            motor.mover(-MIN_PWM_DRIVE);
        } else if (estado == CalibrationManager::COMPLETADO) {
            motor.mover(0);
            motor.deshabilitar();
            sistemaListo = true;
            indicador.indicarCalibracionCompletada();
        } else if (estado == CalibrationManager::ERROR) {
            motor.mover(0);
            motor.deshabilitar();
            indicador.indicarError();
        }
    }
}

void actualizarDisplay() {
    unsigned long ahora = millis();
    
    if (ahora - ultimoDisplayMs >= 1000 && sistemaListo) {
        ultimoDisplayMs = ahora;
        
        int adcActual = sensor.leerSuavizado();
        auto datosCalib = calibrador.obtenerDatosCalibracion();
        
        int porcentaje = map(adcActual, datosCalib.posicionMinimaADC, datosCalib.posicionMaximaADC, 0, 100);
        porcentaje = constrain(porcentaje, 0, 100);
        
        Serial.printf("📊 Posición: %d%% (ADC: %d)\n", porcentaje, adcActual);
    }
}