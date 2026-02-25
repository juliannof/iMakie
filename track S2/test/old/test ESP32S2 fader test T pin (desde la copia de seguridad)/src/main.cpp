#include <Arduino.h>
#include "touch_sensor/TouchSensor.h"

// ★★★ DECLARACIONES DE FUNCIONES ★★★
void ejecutarModoNormal();
void ejecutarModoDiagnostico();
void procesarComando(char comando);

TouchSensor sensor;
enum ModoTest { MODO_NORMAL, MODO_DIAGNOSTICO, MODO_CALIBRACION };
ModoTest modoActual = MODO_NORMAL;

void setup() {
    Serial.begin(115200);
    delay(1000);  // Esperar a que se inicie el monitor serial
    Serial.println("\n🎛️  TEST MODULAR FADER MOTORIZADO - EEPROM");
    Serial.println("=============================================");
    
    // Inicializa el sensor (ahora con EEPROM automática)
    sensor.begin();
    
    Serial.println("\n💡 COMANDOS DISPONIBLES:");
    Serial.println("  'n' - Modo Normal");
    Serial.println("  'd' - Modo Diagnóstico");
    Serial.println("  'c' - Recalibrar y guardar en EEPROM");
    Serial.println("  'u X' - Umbral manual (ej: u 40)");
    Serial.println("  'r' - Forzar recalibración (ignora EEPROM)");
    Serial.println("  'e' - Estado de calibración EEPROM");
    Serial.println("=============================================\n");
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
            break;
    }
    
    delay(100);
}

void ejecutarModoNormal() {
    sensor.leerValor();
    
    // Verificar conexión estable (si implementaste esa función)
    // sensor.verificarConexion();
    
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
            modoActual = MODO_NORMAL;
            Serial.println("🔄 Cambiado a MODO NORMAL");
            break;
        case 'd':
            modoActual = MODO_DIAGNOSTICO;
            Serial.println("🔧 Cambiado a MODO DIAGNÓSTICO");
            break;
        case 'c':
            Serial.println("🔧 Calibrando y guardando en EEPROM...");
            sensor.calibrar();  // Esto ahora guarda automáticamente en EEPROM
            break;
        case 'r':
            Serial.println("🔄 Forzando recalibración...");
            // Aquí podrías añadir lógica para borrar EEPROM primero si quieres
            sensor.calibrar();
            break;
        case 'e':
            Serial.println("💾 ESTADO EEPROM:");
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
                // Nota: calibrar(umbral) también guarda en EEPROM
            }
            break;

        case 'v':  // Verificación detallada de calibración
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