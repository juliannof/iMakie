#include "TouchSensor.h"
#include "../config.h"  // ← ESTO DEBERÍA FUNCIONAR
#include <EEPROM.h>  // ← Añadir esta librería

TouchSensor::TouchSensor() 
    : _valorBase(0), _umbral(0), _valorActual(0), 
      _touchActivo(false), _ultimoEstado(false), _lastTouchTime(0),
      _calibracionCargada(false) {
}

void TouchSensor::inicializarEEPROM() {
    if (!EEPROM.begin(EEPROM_SIZE)) {
        Serial.println("❌ Error al inicializar EEPROM");
        return;
    }
    Serial.println("✅ EEPROM inicializada");
}

void TouchSensor::begin() {
    pinMode(PIN_LED_BUILTIN, OUTPUT);
    digitalWrite(PIN_LED_BUILTIN, LOW);
    delay(500);
    
    inicializarEEPROM();
    
    // Intentar cargar calibración existente
    if (cargarCalibracionEEPROM()) {
        Serial.println("✅ Calibración cargada desde EEPROM");
        Serial.printf("   Valor base: %d, Umbral: %d\n", _valorBase, _umbral);
    } else {
        Serial.println("⚠️  No hay calibración guardada en EEPROM");
        Serial.println("💡 Usa el comando 'c' para calibrar manualmente");
        
        // Valores por defecto basados en tu calibración exitosa
        _valorBase = 36925;
        _umbral = 36723;
        _calibracionCargada = true;
        
        Serial.printf("🎯 Usando valores por defecto: Base=%d, Umbral=%d\n", _valorBase, _umbral);
    }
}

bool TouchSensor::cargarCalibracionEEPROM() {
    // Verificar firma
    uint8_t firma = EEPROM.read(EEPROM_ADDR_SIGNATURE);
    if (firma != EEPROM_SIGNATURE) {
        Serial.println("📭 No hay calibración guardada en EEPROM");
        return false;
    }
    
    // Leer valores
    EEPROM.get(EEPROM_ADDR_VALOR_BASE, _valorBase);
    EEPROM.get(EEPROM_ADDR_UMBRAL, _umbral);
    
    Serial.printf("📖 Leyendo EEPROM - Valor base: %d, Umbral: %d\n", _valorBase, _umbral);
    
    // Validación mejorada
    if (!calibracionValida()) {
        Serial.println("⚠️  Calibración en EEPROM no válida");
        Serial.printf("   Valor base: %d, Umbral: %d\n", _valorBase, _umbral);
        Serial.printf("   Diferencia: %d\n", abs(_valorBase - _umbral));
        return false;
    }
    
    _calibracionCargada = true;
    Serial.println("✅ Calibración cargada y validada desde EEPROM");
    return true;
}

void TouchSensor::guardarCalibracionEEPROM() {
    EEPROM.put(EEPROM_ADDR_VALOR_BASE, _valorBase);
    EEPROM.put(EEPROM_ADDR_UMBRAL, _umbral);
    EEPROM.write(EEPROM_ADDR_SIGNATURE, EEPROM_SIGNATURE);
    
    if (EEPROM.commit()) {
        Serial.println("💾 Calibración guardada en EEPROM");
        Serial.printf("   Valor base: %d, Umbral: %d\n", _valorBase, _umbral);
    } else {
        Serial.println("❌ Error al guardar en EEPROM");
    }
}




bool TouchSensor::calibracionValida() const {
    // Condiciones más flexibles pero seguras
    bool valoresRazonables = (_valorBase > 20000 && _valorBase < 60000) &&
                            (_umbral > 20000 && _umbral < 60000);
    
    bool diferenciaAceptable = (abs(_valorBase - _umbral) > DIFERENCIA_MINIMA_CALIBRACION);
    bool relacionLogica = (_valorBase > _umbral);
    
    return _calibracionCargada && valoresRazonables && diferenciaAceptable && relacionLogica;
}


void TouchSensor::calibrar() {
    Serial.println("🔧 CALIBRACIÓN ROBUSTA INICIADA");
    Serial.println("================================");
    
    // FASE 1: Sin contacto - con filtrado de outliers
    Serial.println("1️⃣  FASE 1: Mano LEJOS del fader");
    Serial.println("    Tomando 100 muestras...");
    digitalWrite(PIN_LED_BUILTIN, LOW);
    delay(3000);
    
    long sumaBase = 0;
    int lecturasBase[CALIBRATION_SAMPLES];
    int minBase = 50000, maxBase = 0;
    
    for(int i = 0; i < CALIBRATION_SAMPLES; i++) {
        int lectura = touchRead(PIN_TOUCH_T);
        lecturasBase[i] = lectura;
        sumaBase += lectura;
        
        if (lectura < minBase) minBase = lectura;
        if (lectura > maxBase) maxBase = lectura;
        
        // Feedback visual progresivo
        if (i % 25 == 0) {
            Serial.printf("    [%d%%] Muestra %d/%d\n", (i * 100) / CALIBRATION_SAMPLES, i + 1, CALIBRATION_SAMPLES);
        }
        delay(CALIBRATION_SAMPLE_DELAY);
    }
    
    // ✅ FILTRADO: Eliminar outliers (10% más altos y 10% más bajos)
    int valorBaseFiltrado = filtrarOutliers(lecturasBase, CALIBRATION_SAMPLES, 10);
    _valorBase = valorBaseFiltrado;
    
    Serial.printf("   ✅ Valor base (filtrado): %d\n", _valorBase);
    Serial.printf("   📊 Rango: %d - %d | Promedio simple: %d\n", 
                  minBase, maxBase, sumaBase / CALIBRATION_SAMPLES);

    // FASE 2: Con contacto - con filtrado de outliers
    Serial.println("2️⃣  FASE 2: Mano TOCANDO el fader");
    Serial.println("    Tomando 100 muestras...");
    digitalWrite(PIN_LED_BUILTIN, HIGH);  // LED encendido durante contacto
    delay(3000);
    
    long sumaTouch = 0;
    int lecturasTouch[CALIBRATION_SAMPLES];
    int minTouch = 50000, maxTouch = 0;
    
    for(int i = 0; i < CALIBRATION_SAMPLES; i++) {
        int lectura = touchRead(PIN_TOUCH_T);
        lecturasTouch[i] = lectura;
        sumaTouch += lectura;
        
        if (lectura < minTouch) minTouch = lectura;
        if (lectura > maxTouch) maxTouch = lectura;
        
        // Feedback visual progresivo
        if (i % 25 == 0) {
            Serial.printf("    [%d%%] Muestra %d/%d\n", (i * 100) / CALIBRATION_SAMPLES, i + 1, CALIBRATION_SAMPLES);
        }
        delay(CALIBRATION_SAMPLE_DELAY);
    }
    
    // ✅ FILTRADO: Eliminar outliers
    int valorTouchFiltrado = filtrarOutliers(lecturasTouch, CALIBRATION_SAMPLES, 10);
    digitalWrite(PIN_LED_BUILTIN, LOW);  // LED apagado al finalizar
    
    // 🔄 CORRECCIÓN AUTOMÁTICA: Asegurar relación correcta
    if (_valorBase < valorTouchFiltrado) {
        Serial.println("🔄 Valores invertidos detectados - corrigiendo automáticamente");
        int temp = _valorBase;
        _valorBase = valorTouchFiltrado;
        valorTouchFiltrado = temp;
    }
    
    // Cálculo de umbral (punto medio)
    _umbral = (_valorBase + valorTouchFiltrado) / 2;
    
    Serial.println("================================");
    Serial.printf("🎯 RESULTADOS CALIBRACIÓN:\n");
    Serial.printf("   Valor base (sin contacto): %d\n", _valorBase);
    Serial.printf("   Valor con contacto: %d\n", valorTouchFiltrado);
    Serial.printf("   Umbral calculado: %d\n", _umbral);
    Serial.printf("   📏 Diferencia: %d puntos\n", _valorBase - valorTouchFiltrado);
    Serial.printf("   📊 Rango contacto: %d - %d\n", minTouch, maxTouch);
    
    // Validación de calidad
    int diferencia = _valorBase - valorTouchFiltrado;
    if (diferencia < DIFERENCIA_MINIMA_CALIBRACION) {
        Serial.printf("⚠️  Advertencia: Diferencia pequeña (%d puntos)\n", diferencia);
        Serial.println("   Considera repetir la calibración con más presión");
    } else {
        Serial.printf("✅ Diferencia excelente: %d puntos\n", diferencia);  
    }
    
    // Guardar en EEPROM
    guardarCalibracionEEPROM();
    _calibracionCargada = true;
    
    Serial.println("🔧 CALIBRACIÓN ROBUSTA COMPLETADA");
}

void TouchSensor::actualizarEstado() {
    bool nuevoEstado = (_valorActual < _umbral);  // Usar _umbral, no _umbralActivacion

    if (nuevoEstado != _touchActivo) {
        if (millis() - _lastTouchTime > DEBOUNCE_TIME_MS) {
            _touchActivo = nuevoEstado;
            _lastTouchTime = millis();

            digitalWrite(PIN_LED_BUILTIN, !_touchActivo);  // Invertir la lógica

            if (_touchActivo) {
                Serial.printf("🟢 TOUCH - Valor: %d\n", _valorActual);
            } else {
                Serial.printf("⚪ LIBERADO - Valor: %d\n", _valorActual);
            }
        }
    }
}


void TouchSensor::diagnosticar() {
    Serial.println("\n📊 DIAGNÓSTICO SENSOR TACTIL");
    Serial.println("============================");
    Serial.printf("Valor actual: %d\n", _valorActual);
    Serial.printf("Conexión: %s\n", _conexionEstable ? "ESTABLE" : "INESTABLE");

    Serial.printf("Valor base: %d\n", _valorBase);
    Serial.printf("Umbral: %d\n", _umbral);
    Serial.printf("Estado: %s\n", _touchActivo ? "TOCANDO" : "LIBRE");
    Serial.printf("Sensor OK: %s\n", sensorOk() ? "SI" : "NO");
    Serial.printf("Margen: %d%%\n", ((_valorBase - _valorActual) * 100) / _valorBase);
    Serial.println("============================\n");
}

bool TouchSensor::sensorOk() {
    return (_valorActual >= VALOR_MINIMO && _valorActual <= VALOR_MAXIMO);
}


bool TouchSensor::conexionEstable() {
    // Verifica si el valor está en rango normal
    bool enRango = (_valorActual >= VALOR_MINIMO_NORMAL && 
                    _valorActual <= VALOR_MAXIMO_NORMAL);
    
    // Verifica cambio brusco
    int diferencia = abs(_valorActual - _ultimoValorValido);
    bool cambioSuave = (diferencia < UMBRAL_CAMBIO_BRUSCO);
    
    if (enRango && cambioSuave) {
        _ultimoValorValido = _valorActual;
        _conexionEstable = true;
        _tiempoInestable = 0;
    } else {
        if (_tiempoInestable == 0) {
            _tiempoInestable = millis();
        }
        // Si lleva más de 2 segundos inestable, considera mala conexión
        _conexionEstable = (millis() - _tiempoInestable < TIEMPO_ESTABILIZACION);
    }
    
    return _conexionEstable;
}

void TouchSensor::verificarConexion() {
    if (!conexionEstable()) {
        Serial.println("🚨 ADVERTENCIA: Posible mala conexión o pin desconectado");
        Serial.printf("   Valor actual: %d (esperado: %d-%d)\n", 
                     _valorActual, VALOR_MINIMO_NORMAL, VALOR_MAXIMO_NORMAL);
    }
}

// Nueva función privada para filtrar outliers
int TouchSensor::filtrarOutliers(int lecturas[], int total, int porcentajeOutliers) {
    // Ordenar las lecturas (usando bubble sort simple)
    for(int i = 0; i < total - 1; i++) {
        for(int j = i + 1; j < total; j++) {
            if(lecturas[j] < lecturas[i]) {
                int temp = lecturas[i];
                lecturas[i] = lecturas[j];
                lecturas[j] = temp;
            }
        }
    }
    
    // Eliminar outliers (porcentaje especificado de cada extremo)
    int outliersACortar = (total * porcentajeOutliers) / 100;
    int inicio = outliersACortar;
    int fin = total - outliersACortar;
    int muestrasValidas = fin - inicio;
    
    // Calcular promedio de las muestras válidas
    long suma = 0;
    for(int i = inicio; i < fin; i++) {
        suma += lecturas[i];
    }
    
    Serial.printf("      Filtrado: %d outliers removidos (%d muestras válidas)\n", 
                  outliersACortar * 2, muestrasValidas);
    
    return suma / muestrasValidas;
}


int TouchSensor::leerValor() {
    _valorActual = touchRead(PIN_TOUCH_T);
    actualizarEstado();
    return _valorActual;
}

bool TouchSensor::touchDetectado() {
    leerValor();
    
    if (_touchActivo && !_ultimoEstado) {
        _ultimoEstado = true;
        return true;
    }
    
    if (!_touchActivo && _ultimoEstado) {
        _ultimoEstado = false;
    }
    
    return false;
}

void TouchSensor::calibrar(int nuevoUmbral) {
    _umbral = nuevoUmbral;
    Serial.printf("🎚️  Umbral manual establecido: %d\n", _umbral);
    guardarCalibracionEEPROM(); // Guardar también en EEPROM
}

bool TouchSensor::estaTocando() {
    leerValor();
    return _touchActivo;
}

void TouchSensor::borrarCalibracionEEPROM() {
    EEPROM.write(EEPROM_ADDR_SIGNATURE, 0); // Borrar firma
    EEPROM.commit();
    _calibracionCargada = false;
    Serial.println("🗑️ Calibración borrada de EEPROM");
}