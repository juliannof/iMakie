#include <Arduino.h>
#include "config.h"
#include "MotorController/MotorController.h"
#include "FaderSensor/FaderSensor.h"
#include "CalibrationManager/CalibrationManager.h"
#include "PositionController/PositionController.h"

// DECLARA LOS PUNTEROS GLOBLALES, PERO NO INSTANTES LOS OBJETOS AÚN
MotorController* motor;
FaderSensor* sensor;
CalibrationManager* calibrador;
PositionController* positionController;

// Variables de estado global
unsigned long ultimoDisplayMs = 0;
bool sistemaListo = false;         // Bandera que indica si el sistema está inicializado y calibrado

// --- Prototipos de funciones auxiliares ---
void manejarComandosSerial();
void manejarCalibracion();
void manejarControlPosicion(); // Nueva función para integrar PositionController
void actualizarDisplay();

void setup() {
    Serial.begin(115200); // Inicializar comunicación serial
    Serial.println("\n=== INICIO PROGRAMA - DEBUG CON INSTANCIAS DINÁMICAS ===");
    delay(100);

    // AHORA INSTANCIA LOS OBJETOS DINÁMICAMENTE (CON 'new')
    motor = new MotorController();
    Serial.println(">>> PASO 1: MotorController creado."); // Debug point
    delay(100);
    motor->begin(); // Llama al begin() a través del puntero
    Serial.println(">>> PASO 1.1: MotorController inicializado."); // Debug point
    delay(100);

    sensor = new FaderSensor();
    Serial.println(">>> PASO 2: FaderSensor creado."); // Debug point
    delay(100);
    sensor->begin();
    Serial.println(">>> PASO 2.1: FaderSensor inicializado."); // Debug point
    delay(100);

    calibrador = new CalibrationManager();
    Serial.println(">>> PASO 3: CalibrationManager creado."); // Debug point
    delay(100);
    calibrador->comenzar(); // Llama a comenzar() a través del puntero
    Serial.println(">>> PASO 3.1: CalibrationManager inicializado."); // Debug point
    delay(100);


    // --- INSTANCIAR PositionController pasándole el puntero a 'sensor' ---
    positionController = new PositionController(sensor); // <-- MODIFICACIÓN AQUÍ
    Serial.println(">>> PASO 5: PositionController creado."); // Debug point
    delay(100);

    Serial.println("\n=== SISTEMA FADER MOTORIZADO ===");

    if (calibrador->obtenerDatosCalibracion().calibrado) { 
        Serial.println("✅ Calibración cargada desde memoria.");
        sistemaListo = true;
        //indicador->setEstadoSistema(true); 
    } else {
        Serial.println("⚠️  Sistema NO calibrado. Ejecutar comando 'c' para calibrar.");
        //indicador->setEstadoSistema(false); 
    }

    Serial.println("\n💡 COMANDOS DISPONIBLES:");
    Serial.println("   'c'        - Iniciar calibración");
    Serial.println("   'p<num>'   - Mover a porcentaje (ej: p50 -> mueve al 50%)");
    Serial.println("   's'        - Detener movimiento actual (calibración o posición)");
    Serial.println("   'e'        - Habilitar motor (solo si no está habilitado)");
    Serial.println("   'd'        - Deshabilitar motor (detiene y apaga el driver)");
    Serial.println("   'x'        - PARADA DE EMERGENCIA");
    Serial.println("   'u<num>'   - Establecer duración de pulso (ej: u60)");
    Serial.println("   'i<num>'   - Establecer intervalo de pulso (ej: i150)");
    Serial.println("   '?'        - Mostrar estado del sistema");

    Serial.println("=== FIN DE SETUP ==="); 
}

void loop() {
    manejarComandosSerial();    
    manejarCalibracion();      
     static unsigned long lastADCPrintTime = 0;
    if (millis() - lastADCPrintTime > 100) { // Imprimir cada 100ms
        Serial.printf("ADC Live: %d\n", sensor->readSmoothed());
        lastADCPrintTime = millis();
    }

    // ... el resto de tu loop() que llama a positionController.update y motor.drive ...
    if (sistemaListo && calibrador->obtenerDatosCalibracion().calibrado && !calibrador->estaCalibrando()) {
        int currentADC = sensor->readSmoothed(); 
        int pwmOutput = positionController->update(currentADC);
        motor->drive(pwmOutput);
    } 
    manejarControlPosicion();   
    actualizarDisplay();        
    //indicador->actualizar();    

    delay(10); 
}

// --- Resto de las funciones de main.cpp sin cambios en esta ronda ---
void manejarComandosSerial() {
    if (!Serial.available()) return; 

    char comandoChar = Serial.read(); 
    String inputNum = "";
    bool hasNum = false;
    while (Serial.available() && isDigit(Serial.peek())) {
        inputNum += (char)Serial.read();
        hasNum = true;
    }
    int value = hasNum ? inputNum.toInt() : -1; 

    switch (comandoChar) {
        case '+': // Mover motor hacia adelante con PWM_MIN_MOTION_THRESHOLD
        if (motor->isEnabled()) {
            motor->drive(PWM_MIN_MOTION_THRESHOLD);
            Serial.printf("⬆️  Motor Forward: %d PWM\n", PWM_MIN_MOTION_THRESHOLD);
        } else {
            Serial.println("⚠️  Motor deshabilitado. No se puede mover.");
        }
        break;
    case '-': // Mover motor hacia atrás con -PWM_MIN_MOTION_THRESHOLD
        if (motor->isEnabled()) {
            motor->drive(-PWM_MIN_MOTION_THRESHOLD);
            Serial.printf("⬇️  Motor Reverse: %d PWM\n", PWM_MIN_MOTION_THRESHOLD);
        } else {
            Serial.println("⚠️  Motor deshabilitado. No se puede mover.");
        }
        break;
    case '0': // Detener motor (PWM 0)
        motor->drive(0);
        Serial.println("🛑 Motor parado (PWM 0).");
        break;
    case 'w': // Setear un valor de PWM específico (ej. w700)
        if (value >= -PWM_MAX_USABLE_DRIVE && value <= PWM_MAX_USABLE_DRIVE) { // Limitar a rango usable
            motor->drive(value);
            Serial.printf("🕹️  Motor set PWM: %d\n", value);
        } else {
            Serial.printf("⚠️  Uso: w<PWM_value> (ej: w700, w-650). Rango usable: +/- %d\n", PWM_MAX_USABLE_DRIVE);
        }
        break;
        case 'c': // Iniciar calibración  
            if (!motor->isEnabled()) motor->enable(); 
            positionController->resetControllerState(); 
            positionController->setTargetPosition(sensor->readSmoothed()); 
            motor->drive(0); 
            
            if (calibrador->iniciarCalibracion()) { 
                //indicador->indicarCalibracionEnCurso(); 
            } else {
                Serial.println("⚠️  No se pudo iniciar la calibración.");
            }
            break;

        case 'p': // Mover a una posición en porcentaje (requiere un número)
            if (sistemaListo && calibrador->obtenerDatosCalibracion().calibrado && !calibrador->estaCalibrando()) { 
                if (value >= 0 && value <= 100) {
                    motor->enable(); 
                    auto calibData = calibrador->obtenerDatosCalibracion(); 
                    positionController->setTargetPositionPercent(value, calibData.posicionMinimaADC, calibData.posicionMaximaADC); 
                } else {
                    Serial.println("⚠️  Uso: p<0-100> (ej: p50)");
                }
            } else {
                Serial.println("⚠️  Sistema no listo o no calibrado para movimiento a posición.");
            }
            break;

        case 's': // Detener cualquier movimiento actual
            if (positionController->isMoving() || positionController->isHolding()) { 
                positionController->resetControllerState(); 
                positionController->setTargetPosition(sensor->readSmoothed()); 
                motor->drive(0); 
                Serial.println("🛑 Movimiento a posición detenido.");
            }
            if (calibrador->estaCalibrando()) { 
                calibrador->abortarCalibracion(); 
                motor->drive(0); 
                motor->disable(); 
                Serial.println("🛑 Calibración abortada.");
            }
            break;

        case 'e': // Habilitar el driver del motor
            motor->enable(); 
            break;

        case 'd': // Deshabilitar el driver del motor
            motor->disable(); 
            positionController->resetControllerState(); 
            positionController->setTargetPosition(sensor->readSmoothed()); 
            break;

        case 'x': // PARADA DE EMERGENCIA
            motor->emergencyStop(); 
            calibrador->abortarCalibracion(); 
            positionController->resetControllerState(); 
            positionController->setTargetPosition(sensor->readSmoothed()); 
            //indicador->indicarEmergencia(); 
            sistemaListo = false; 
            break;
            
        case 'u': // Establecer duración de pulso para PositionController
            if (value >= 0) {
               // positionController->setPulseDuration(value); 
            } else {
                Serial.println("⚠️  Uso: u<ms> (ej: u60)");
            }
            break;

        case 'i': // Establecer intervalo de pulso para PositionController
            if (value >= 0) {
                //positionController->setPulseInterval(value); 
            } else {
                Serial.println("⚠️  Uso: i<ms> (ej: i150)");
            }
            break;

        case '?': // Mostrar estado actual del sistema
            Serial.println("\n=== ESTADO DEL SISTEMA ===");
            Serial.printf("   Sistema listo: %s\n", sistemaListo ? "SI" : "NO");
            Serial.printf("   Motor: %s\n", motor->isEnabled() ? "HABILITADO" : "DESHABILITADO"); 
            Serial.printf("   Moviendo (Motor): %s\n", motor->isMoving() ? "SI" : "NO"); 
            Serial.printf("   Calibrado: %s\n", calibrador->obtenerDatosCalibracion().calibrado ? "SI" : "NO"); 

            // Estado de calibración
            switch (calibrador->obtenerEstado()) { 
                case CalibrationManager::INACTIVO:    Serial.println("   Calibración: INACTIVA"); break;
                case CalibrationManager::BUSCANDO_MAXIMO: Serial.printf("   Calibración: BUSCANDO MAX (%d ADC)\n", sensor->readSmoothed()); break; 
                case CalibrationManager::BUSCANDO_MINIMO: Serial.printf("   Calibración: BUSCANDO MIN (%d ADC)\n", sensor->readSmoothed()); break; 
                case CalibrationManager::COMPLETADO:  Serial.println("   Calibración: COMPLETADA"); break;
                case CalibrationManager::ERROR:       Serial.printf("   Calibración: ERROR (%s)\n", calibrador->obtenerDatosCalibracion().mensajeError.c_str()); break; 
            }
            
            // Estado del controlador de posición
            Serial.printf("   Control posición activo: %s\n", positionController->isMoving() ? "SI" : "NO"); 
            Serial.printf("   Manteniendo posición: %s\n", positionController->isHolding() ? "SI" : "NO"); 
            Serial.printf("   Objetivo de posición ADC: %d\n", positionController->getCurrentTarget()); 
            Serial.printf("   ADC actual (suavizado): %d\n", sensor->readSmoothed()); 
            break;

        default:
            Serial.printf("⚠️  Comando '%c' no reconocido.\n", comandoChar);
            break;
    }
}

// === Modificación a manejarCalibracion ===
void manejarCalibracion() {
    if (calibrador->estaCalibrando()) {
        if (!motor->isEnabled()) { 
            motor->enable();
        }
        int adcActual = sensor->readSmoothed(); 
        calibrador->actualizarCalibracion(adcActual); 
        auto estadoCalib = calibrador->obtenerEstado();
        if (estadoCalib == CalibrationManager::BUSCANDO_MAXIMO) {
            motor->drive(PWM_MIN_MOTION_THRESHOLD); // <-- CAMBIO A LA NUEVA CONSTANTE
            //indicador->indicarCalibracionEnCurso(); 
        } else if (estadoCalib == CalibrationManager::BUSCANDO_MINIMO) {
            motor->drive(-PWM_MIN_MOTION_THRESHOLD); // <-- CAMBIO A LA NUEVA CONSTANTE
            //indicador->indicarCalibracionEnCurso(); 
        }
    } else { 
        static bool calibrationActionDone = false; 
        if (calibrador->obtenerEstado() == CalibrationManager::COMPLETADO) {
            if (!calibrationActionDone) { 
                motor->drive(0); 
                sistemaListo = true;
                //indicador->indicarCalibracionCompletada(); 
                Serial.println(">>> DEBUG: manejada Calibracion COMPLETADA (Acción Única)"); 
                calibrationActionDone = true; 
            }
        } else if (calibrador->obtenerEstado() == CalibrationManager::ERROR) {
            if (!calibrationActionDone) { 
                motor->drive(0);
                motor->disable();
                //indicador->indicarError();
                sistemaListo = false;
                Serial.println(">>> DEBUG: manejada Calibracion ERROR (Acción Única)"); 
                calibrationActionDone = true;
            }
        } else if (calibrador->obtenerEstado() == CalibrationManager::INACTIVO) {
            calibrationActionDone = false; 
        }
    }
}

// === Modificación a manejarControlPosicion ===
void manejarControlPosicion() {
    if (calibrador->estaCalibrando()) {
        if (positionController->isMoving() || positionController->isHolding()) {
            Serial.println(">>> DEBUG: manejandoControlPosicion está ignorando porque calibrando. Se resetea PositionController.");
            positionController->resetControllerState(); 
            motor->drive(0); 
        }
        return; 
    }

    if (sistemaListo && motor->isEnabled()) { 
        int currentADC = sensor->readSmoothed();
        int pwmOutput = positionController->update(currentADC); 
        motor->drive(pwmOutput);
    } else if (positionController->isMoving() || positionController->isHolding()) {
        positionController->resetControllerState(); 
        positionController->setTargetPosition(sensor->readSmoothed()); 
        motor->drive(0);
    }
}

void actualizarDisplay() {
    unsigned long ahora = millis();

    if (ahora - ultimoDisplayMs >= 1000 && sistemaListo) {
        ultimoDisplayMs = ahora;

        int adcActual = sensor->readSmoothed();
        auto datosCalib = calibrador->obtenerDatosCalibracion();

        int minVal = datosCalib.posicionMinimaADC;
        int maxVal = datosCalib.posicionMaximaADC;
        
        if (minVal == maxVal || (abs(maxVal - minVal) < 100)) {
             Serial.printf("📊 Actual: ADC %d (Calibración inválida para %)\n", adcActual);
        } else {
            int porcentaje = map(adcActual, minVal, maxVal, 0, 100);
            porcentaje = constrain(porcentaje, 0, 100);

            Serial.printf("📊 Posición: %d%% (ADC: %d) Objetivo: %d ADC Activo:%s Holding:%s\n", 
                          porcentaje, adcActual, positionController->getCurrentTarget(),
                          positionController->isMoving() ? "SI" : "NO",
                          positionController->isHolding() ? "SI" : "NO");
        }
    }
}