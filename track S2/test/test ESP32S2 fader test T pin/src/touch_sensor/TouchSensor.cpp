#include "TouchSensor.h"
#include "../config.h"

// ★★★ DEFINIR CONSTANTES PARA PREFERENCES ★★★
const char* TouchSensor::NAMESPACE = "touch_sensor";
const char* TouchSensor::KEY_BASE = "base";
const char* TouchSensor::KEY_UMBRAL = "umbral";
const char* TouchSensor::KEY_SIGNATURE = "signature";

TouchSensor::TouchSensor() 
    : _valorBase(0), _umbral(0), _valorActual(0), 
      _touchActivo(false), _ultimoEstado(false), _lastTouchTime(0),
      _conexionEstable(false), _tiempoInestable(0), _ultimoValorValido(0),
      _esperandoBoton(false), _modoCalibracion(false), _inicioCalibracion(0),
      _muestrasCalibracion(0), _sumaCalibracion(0), _ultimaPulsacionBoton(0),
      _estadoBotonAnterior(HIGH), _calibracionCargada(false) {
}

// ★★★ INICIALIZACIÓN ★★★
void TouchSensor::begin() {
    pinMode(PIN_LED_BUILTIN, OUTPUT);
    digitalWrite(PIN_LED_BUILTIN, LOW);
    
    // Configurar botón
    pinMode(PIN_BOTON_CALIBRACION, INPUT_PULLUP);
    Serial.printf("[Touch] Botón calibración en pin %d\n", PIN_BOTON_CALIBRACION);
    
    delay(500);
    
    // ★★★ DEBUG: Ver valores directamente de Preferences ★★★
    Serial.println("\n[DEBUG] === VERIFICACIÓN DIRECTA PREFERENCES ===");
    if (!_preferences.begin(NAMESPACE, true)) {
        Serial.println("❌ No se pudo abrir Preferences");
    } else {
        uint8_t firma = _preferences.getUChar(KEY_SIGNATURE, 0);
        int base = _preferences.getInt(KEY_BASE, 0);
        int umbral = _preferences.getInt(KEY_UMBRAL, 0);
        _preferences.end();
        
        Serial.printf("   Firma: 0x%02X\n", firma);
        Serial.printf("   Base: %d\n", base);
        Serial.printf("   Umbral: %d\n", umbral);
        Serial.printf("   Diferencia: %d\n", base - umbral);
        Serial.println("========================================\n");
    }
    
    // Intentar cargar calibración existente
    if (cargarCalibracion()) {
        Serial.println("✅ Calibración cargada y válida");
    } else {
        Serial.println("⚠️  No hay calibración válida guardada");
        Serial.println("💡 Pulse el botón en PIN 37 para calibrar");
        _esperandoBoton = true;
    }
}

// ★★★ CARGAR CALIBRACIÓN DESDE PREFERENCES ★★★
bool TouchSensor::cargarCalibracion() {
    if (!_preferences.begin(NAMESPACE, true)) {
        Serial.println("❌ Error al abrir Preferences");
        return false;
    }
    
    // Verificar firma
    uint8_t firma = _preferences.getUChar(KEY_SIGNATURE, 0);
    Serial.printf("[Preferences] Firma leída: 0x%02X (esperado: 0x%02X)\n", 
                  firma, SIGNATURE_VALID);
    
    if (firma != SIGNATURE_VALID) {
        Serial.println("📭 No hay calibración guardada (firma inválida)");
        _preferences.end();
        return false;
    }
    
    // Leer valores
    _valorBase = _preferences.getInt(KEY_BASE, 0);
    _umbral = _preferences.getInt(KEY_UMBRAL, 0);
    
    Serial.printf("📖 Preferences - Valor base: %d, Umbral: %d\n", _valorBase, _umbral);
    
    _preferences.end();
    
    _calibracionCargada = true;
    
    // Validar
    bool valida = calibracionValida();
    
    if (!valida) {
        Serial.println("⚠️  Calibración en Preferences no pasa validación");
        _calibracionCargada = false;
    } else {
        Serial.println("✅ Calibración cargada y validada desde Preferences");
    }
    
    return valida;
}

// ★★★ GUARDAR CALIBRACIÓN EN PREFERENCES ★★★
void TouchSensor::guardarCalibracion() {
    if (!_preferences.begin(NAMESPACE, false)) {  // Modo escritura
        Serial.println("❌ Error al abrir Preferences para escritura");
        return;
    }
    
    _preferences.putInt(KEY_BASE, _valorBase);
    _preferences.putInt(KEY_UMBRAL, _umbral);
    _preferences.putUChar(KEY_SIGNATURE, SIGNATURE_VALID);
    
    // ★★★ IMPORTANTE: Preferences NO necesita commit(), se guarda automáticamente ★★★
    _preferences.end();
    
    Serial.println("💾 Calibración guardada en Preferences");
    Serial.printf("   Valor base: %d, Umbral: %d\n", _valorBase, _umbral);
    _calibracionCargada = true;
}

// ★★★ BORRAR CALIBRACIÓN ★★★
void TouchSensor::borrarCalibracion() {
    if (!_preferences.begin(NAMESPACE, false)) {
        return;
    }
    
    _preferences.clear();  // Borra todo el namespace
    _preferences.end();
    
    _calibracionCargada = false;
    _valorBase = 0;
    _umbral = 0;
    Serial.println("🗑️  Calibración borrada de Preferences");
}

bool TouchSensor::calibracionValida() const {
    // Verificar que tenemos valores
    if (!_calibracionCargada || _valorBase == 0 || _umbral == 0) {
        Serial.println("[Validación] ❌ Calibración no cargada o valores cero");
        return false;
    }
    
    // ★★★ CORRECCIÓN: Usar DIFERENCIA real, no absoluta ★★★
    const int DIFERENCIA_MINIMA_CALIBRACION = 150;
    int diferencia = _valorBase - _umbral;  // NO abs()
    
    Serial.println("\n[Validación] === VERIFICANDO CALIBRACIÓN ===");
    Serial.printf("   Valor base: %d\n", _valorBase);
    Serial.printf("   Umbral: %d\n", _umbral);
    Serial.printf("   Diferencia: %d (mínimo: %d)\n", diferencia, DIFERENCIA_MINIMA_CALIBRACION);
    Serial.printf("   Base > Umbral: %s\n", (_valorBase > _umbral) ? "SI" : "NO");
    
    // Condiciones de validación
    bool valoresRazonables = (_valorBase > 20000 && _valorBase < 60000) &&
                            (_umbral > 20000 && _umbral < 60000);
    
    bool diferenciaSuficiente = (diferencia > DIFERENCIA_MINIMA_CALIBRACION);
    bool relacionCorrecta = (_valorBase > _umbral);  // Base debe ser MAYOR que umbral
    
    bool valida = valoresRazonables && diferenciaSuficiente && relacionCorrecta;
    
    // Diagnóstico detallado
    Serial.printf("   Rango base OK (20k-60k): %s\n", (_valorBase > 20000 && _valorBase < 60000) ? "SI" : "NO");
    Serial.printf("   Rango umbral OK (20k-60k): %s\n", (_umbral > 20000 && _umbral < 60000) ? "SI" : "NO");
    Serial.printf("   Diferencia suficiente (>%d): %s\n", DIFERENCIA_MINIMA_CALIBRACION, diferenciaSuficiente ? "SI" : "NO");
    Serial.printf("   Relación correcta (base>umbral): %s\n", relacionCorrecta ? "SI" : "NO");
    Serial.printf("   ✅ Calibración válida: %s\n", valida ? "SI" : "NO");
    Serial.println("=======================================\n");
    
    return valida;
}

// ★★★ LEER BOTÓN DE CALIBRACIÓN ★★★
bool TouchSensor::leerBotonCalibracion() {
    bool estadoActual = digitalRead(PIN_BOTON_CALIBRACION);
    unsigned long ahora = millis();
    
    // Debounce
    if (ahora - _ultimaPulsacionBoton < DEBOUNCE_TIME_MS) {
        return false;
    }
    
    // Detectar flanco descendente (botón presionado)
    if (_estadoBotonAnterior == HIGH && estadoActual == LOW) {
        _ultimaPulsacionBoton = ahora;
        _estadoBotonAnterior = estadoActual;
        Serial.println("[Boton] ✅ Pulsado!");
        return true;
    }
    
    _estadoBotonAnterior = estadoActual;
    return false;
}

// ★★★ PROCESAR BOTÓN ★★★
bool TouchSensor::procesarBoton() {
    if (!_esperandoBoton) return false;
    
    if (leerBotonCalibracion()) {
        _esperandoBoton = false;
        iniciarCalibracionAutomatica();
        Serial.println("\n[Boton] 🔧 INICIANDO CALIBRACIÓN...");
        return true;
    }
    
    return false;
}

// ★★★ INICIAR CALIBRACIÓN AUTOMÁTICA ★★★
void TouchSensor::iniciarCalibracionAutomatica() {
    Serial.println("🔧 CALIBRACIÓN AUTOMÁTICA INICIADA");
    Serial.println("===================================");
    
    _modoCalibracion = true;
    _inicioCalibracion = millis();
    _muestrasCalibracion = 0;
    _sumaCalibracion = 0;
    
    // FASE 1: Sin contacto
    Serial.println("1️⃣  FASE 1: Mano LEJOS del fader");
    Serial.println("    Tomando 100 muestras...");
    digitalWrite(PIN_LED_BUILTIN, LOW);
}

// ★★★ ACTUALIZAR CALIBRACIÓN (PARA LLAMAR EN LOOP) ★★★
void TouchSensor::actualizarCalibracion() {
    if (!_modoCalibracion) return;
    
    unsigned long ahora = millis();
    unsigned long tiempo = ahora - _inicioCalibracion;
    
    // Si han pasado menos de 3 segundos, estamos en fase 1 (sin tocar)
    if (tiempo < 3000) {
        // Seguir tomando muestras sin tocar
        if (_muestrasCalibracion < CALIBRATION_SAMPLES) {
            int lectura = touchRead(PIN_TOUCH_T);
            _sumaCalibracion += lectura;
            _muestrasCalibracion++;
            delay(CALIBRATION_SAMPLE_DELAY);
        }
        
        // Mostrar progreso
        static unsigned long ultimoReporte = 0;
        if (ahora - ultimoReporte > 1000) {
            ultimoReporte = ahora;
            int segs = 3 - (tiempo / 1000);
            int porcentaje = (_muestrasCalibracion * 100) / CALIBRATION_SAMPLES;
            Serial.printf("    [%d%%] Muestras %d/%d | Restante: %ds\n", 
                         porcentaje, _muestrasCalibracion, CALIBRATION_SAMPLES, segs);
        }
        
        // Si terminamos fase 1, pasar a fase 2
        if (_muestrasCalibracion >= CALIBRATION_SAMPLES) {
            Serial.println("2️⃣  FASE 2: Mano TOCANDO el fader");
            Serial.println("    Tomando 100 muestras...");
            digitalWrite(PIN_LED_BUILTIN, HIGH);
            // Reiniciar contadores para fase 2
            _muestrasCalibracion = 0;
            _sumaCalibracion = 0;
            _inicioCalibracion = millis(); // Resetear tiempo
        }
    } else {
        // Fase 2: con toque
        if (_muestrasCalibracion < CALIBRATION_SAMPLES) {
            int lectura = touchRead(PIN_TOUCH_T);
            _sumaCalibracion += lectura;
            _muestrasCalibracion++;
            delay(CALIBRATION_SAMPLE_DELAY);
        } else {
            // Terminar calibración
            finalizarCalibracionAutomatica();
        }
    }
}

// ★★★ FINALIZAR CALIBRACIÓN AUTOMÁTICA ★★★
void TouchSensor::finalizarCalibracionAutomatica() {
    digitalWrite(PIN_LED_BUILTIN, LOW);
    _modoCalibracion = false;
    
    // Llamar a la calibración completa
    calibrar();
}

// ★★★ CALIBRACIÓN COMPLETA (TU CÓDIGO ORIGINAL ADAPTADO) ★★★
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
    const int DIFERENCIA_MINIMA_CALIBRACION = 150;
    int diferencia = _valorBase - valorTouchFiltrado;
    if (diferencia < DIFERENCIA_MINIMA_CALIBRACION) {
        Serial.printf("⚠️  Advertencia: Diferencia pequeña (%d puntos)\n", diferencia);
        Serial.println("   Considera repetir la calibración con más presión");
    } else {
        Serial.printf("✅ Diferencia excelente: %d puntos\n", diferencia);  
    }
    
    // Guardar en Preferences
    guardarCalibracion();
    
    Serial.println("🔧 CALIBRACIÓN ROBUSTA COMPLETADA");
}

// ★★★ CALIBRACIÓN MANUAL CON UMBRAL ESPECÍFICO ★★★
void TouchSensor::calibrar(int nuevoUmbral) {
    _umbral = nuevoUmbral;
    
    // Leer valor actual para establecer base
    _valorActual = touchRead(PIN_TOUCH_T);
    _valorBase = _valorActual;  // Asumir valor actual como base
    
    Serial.printf("🎚️  Umbral manual establecido: %d\n", _umbral);
    Serial.printf("   Valor base actual: %d\n", _valorBase);
    
    guardarCalibracion();
}

// ★★★ LEER VALOR DEL SENSOR ★★★
int TouchSensor::leerValor() {
    _valorActual = touchRead(PIN_TOUCH_T);
    actualizarEstado();
    return _valorActual;
}

// ★★★ ACTUALIZAR ESTADO DEL TOUCH ★★★
void TouchSensor::actualizarEstado() {
    bool nuevoEstado = (_valorActual < _umbral);
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

// ★★★ DETECTAR TOUCH (CON DEBOUNCE) ★★★
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

// ★★★ VERIFICAR SI SE ESTÁ TOCANDO ★★★
bool TouchSensor::estaTocando() {
    leerValor();
    return _touchActivo;
}

// ★★★ DIAGNÓSTICO ★★★
void TouchSensor::diagnosticar() {
    leerValor();
    
    Serial.println("\n📊 DIAGNÓSTICO SENSOR TACTIL");
    Serial.println("============================");
    Serial.printf("Valor actual: %d\n", _valorActual);
    Serial.printf("Conexión: %s\n", _conexionEstable ? "ESTABLE" : "INESTABLE");
    Serial.printf("Valor base: %d\n", _valorBase);
    Serial.printf("Umbral: %d\n", _umbral);
    Serial.printf("Estado: %s\n", _touchActivo ? "TOCANDO" : "LIBRE");
    Serial.printf("Sensor OK: %s\n", sensorOk() ? "SI" : "NO");
    Serial.printf("Margen: %d%%\n", ((_valorBase - _valorActual) * 100) / _valorBase);
    Serial.printf("Modo: %s\n", _modoCalibracion ? "CALIBRANDO" : 
                                 _esperandoBoton ? "ESPERANDO_BOTON" : 
                                 "NORMAL");
    Serial.printf("Botón PIN 37: %s\n", digitalRead(PIN_BOTON_CALIBRACION) ? "HIGH" : "LOW");
    Serial.println("============================\n");
}

// ★★★ VERIFICAR SI EL SENSOR ESTÁ OK ★★★
bool TouchSensor::sensorOk() {
    // Valores típicos ESP32-S2 touch
    const int VALOR_MINIMO = 10;
    const int VALOR_MAXIMO = 150;
    return (_valorActual >= VALOR_MINIMO && _valorActual <= VALOR_MAXIMO);
}

// ★★★ VERIFICAR CONEXIÓN ESTABLE ★★★
bool TouchSensor::conexionEstable() {
    // Valores típicos para ESP32-S2 touch
    const int VALOR_MINIMO_NORMAL = 30000;
    const int VALOR_MAXIMO_NORMAL = 40000;
    const int UMBRAL_CAMBIO_BRUSCO = 5000;
    const int TIEMPO_ESTABILIZACION = 2000;
    
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

// ★★★ VERIFICAR CONEXIÓN ★★★
void TouchSensor::verificarConexion() {
    if (!conexionEstable()) {
        Serial.println("🚨 ADVERTENCIA: Posible mala conexión o pin desconectado");
        const int VALOR_MINIMO_NORMAL = 30000;
        const int VALOR_MAXIMO_NORMAL = 40000;
        Serial.printf("   Valor actual: %d (esperado: %d-%d)\n", 
                     _valorActual, VALOR_MINIMO_NORMAL, VALOR_MAXIMO_NORMAL);
    }
}

// ★★★ FILTRAR OUTLIERS (TU CÓDIGO ORIGINAL) ★★★
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