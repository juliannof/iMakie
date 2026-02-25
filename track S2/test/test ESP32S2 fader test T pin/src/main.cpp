#include <Arduino.h>
#include "touch_sensor/TouchSensor.h"

// ★★★ DECLARACIONES DE FUNCIONES ★★★
void ejecutarModoNormal();
void ejecutarModoDiagnostico();
void ejecutarModoCalibracion();  // ★★★ NUEVO ★★★
void procesarComando(char comando);

TouchSensor sensor;

enum ModoTest { MODO_NORMAL, MODO_DIAGNOSTICO, MODO_CALIBRACION, MODO_ESPERA_BOTON };
ModoTest modoActual = MODO_ESPERA_BOTON;  // ★★★ CAMBIAR INICIO ★★★

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n🎛️  TEST MODULAR FADER MOTORIZADO - PREFERENCES");
    Serial.println("================================================");
    
    // Inicializa el sensor (ahora con Preferences)
    sensor.begin();
    
    // ★★★ DETERMINAR MODO INICIAL ★★★
    if (sensor.calibracionValida()) {
        modoActual = MODO_NORMAL;
        Serial.println("✅ Sensor calibrado - Modo Normal");
    } else if (sensor.enModoCalibracion()) {
        modoActual = MODO_CALIBRACION;
        Serial.println("🔧 Modo Calibración activado");
    } else {
        modoActual = MODO_ESPERA_BOTON;
        Serial.println("⏳ Esperando botón para calibrar...");
    }
    
    Serial.println("\n💡 COMANDOS DISPONIBLES:");
    Serial.println("  'n' - Modo Normal");
    Serial.println("  'd' - Modo Diagnóstico");
    Serial.println("  'c' - Recalibrar manualmente");
    Serial.println("  'u X' - Umbral manual (ej: u 40)");
    Serial.println("  'r' - Forzar recalibración");
    Serial.println("  'e' - Estado de calibración");
    Serial.println("  'b' - Borrar calibración");
    Serial.println("===============================================\n");
}

void loop() {
    if (Serial.available()) {
        procesarComando(Serial.read());
    }
    
    switch(modoActual) {
        case MODO_NORMAL:
            ejecutarModoNormal();
            break;
        case MODO_DIAGNOSTICO:
            ejecutarModoDiagnostico();
            break;
        case MODO_CALIBRACION:
            ejecutarModoCalibracion();
            break;
        case MODO_ESPERA_BOTON:
            // ★★★ ESPERAR BOTÓN ★★★
            if (sensor.procesarBoton()) {
                modoActual = MODO_CALIBRACION;
                Serial.println("🔄 Cambiando a Modo Calibración");
            }
            delay(100);
            break;
    }
    
    //delay(100);
}

void ejecutarModoCalibracion() {
    if (sensor.enModoCalibracion()) {
        sensor.actualizarCalibracion();
    } else {
        // Calibración completada
        if (sensor.calibracionValida()) {
            Serial.println("✅ Calibración exitosa - Cambiando a Modo Normal");
            modoActual = MODO_NORMAL;
        } else {
            Serial.println("❌ Calibración fallida - Esperando botón");
            modoActual = MODO_ESPERA_BOTON;
        }
    }
}

void ejecutarModoNormal() {
    sensor.leerValor();
    
    if (sensor.touchDetectado()) {
        Serial.println("🎯 EVENTO: Touch detectado!");
    }
    
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug > 3000) {
        lastDebug = millis();
        Serial.printf("📈 Valor: %d | Estado: %s | Calib: %s\n", 
                      sensor.getValorActual(), 
                      sensor.getEstado() ? "ON" : "OFF",
                      sensor.calibracionValida() ? "OK" : "NOK");
    }
}

void ejecutarModoDiagnostico() {
    sensor.leerValor();
    sensor.diagnosticar();
    delay(2000);
}

void procesarComando(char comando) {
    switch(comando) {
        case 'n':
            if (sensor.calibracionValida()) {
                modoActual = MODO_NORMAL;
                Serial.println("🔄 Cambiado a MODO NORMAL");
            } else {
                Serial.println("❌ No hay calibración válida. Calibre primero.");
            }
            break;
        case 'd':
            modoActual = MODO_DIAGNOSTICO;
            Serial.println("🔧 Cambiado a MODO DIAGNÓSTICO");
            break;
        case 'c':
            Serial.println("🔧 Calibrando y guardando en Preferences...");
            sensor.calibrar();  // ★★★ AHORA USA PREFERENCES ★★★
            modoActual = MODO_CALIBRACION;
            break;
        case 'r':
            Serial.println("🔄 Forzando recalibración...");
            sensor.calibrar();
            modoActual = MODO_CALIBRACION;
            break;
        case 'e':
            Serial.println("💾 ESTADO PREFERENCES:");
            Serial.printf("   Calibración válida: %s\n", 
                         sensor.calibracionValida() ? "SI" : "NO");
            Serial.printf("   Valor base: %d\n", sensor.getValorBase());
            Serial.printf("   Umbral: %d\n", sensor.getUmbral());
            break;
        case 'u':
            delay(100);
            if (Serial.available()) {
                int umbral = Serial.parseInt();
                sensor.calibrar(umbral);
                // Nota: calibrar(umbral) también guarda en Preferences
            }
            break;
        case 'b':
            Serial.println("🗑️  Borrando calibración...");
            sensor.borrarCalibracion();
            modoActual = MODO_ESPERA_BOTON;
            break;
        case 'v':
            Serial.println("🔍 VERIFICACIÓN DETALLADA CALIBRACIÓN:");
            Serial.printf("   Valor base: %d\n", sensor.getValorBase());
            Serial.printf("   Umbral: %d\n", sensor.getUmbral());
            Serial.printf("   Diferencia: %d\n", abs(sensor.getValorBase() - sensor.getUmbral()));
            Serial.printf("   Calibración válida: %s\n", sensor.calibracionValida() ? "SI" : "NO");
            break;
        default:
            Serial.println("❌ Comando no reconocido");
            break;
    }
}