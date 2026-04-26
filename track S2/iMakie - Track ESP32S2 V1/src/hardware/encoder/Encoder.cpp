#include "Encoder.h"
#include "../../config.h"

volatile long        Encoder::_counter      = 0;
long                 Encoder::_lastReported = 0;
int                  Encoder::currentVPotLevel = 0;

// Estado para lógica SAT (la única fuente de verdad)
static volatile unsigned long _lastDebounceTime = 0;
static volatile int           _lastA            = 0;
static volatile int           _lastB            = 0;

void IRAM_ATTR Encoder::_isr() {
    // Lógica idéntica a SAT _tickTestEncoder
    unsigned long now = millis();
    
    int A = (GPIO.in >> ENCODER_PIN_A) & 1;
    int B = (GPIO.in >> ENCODER_PIN_B) & 1;
    
    if (A != _lastA) {
        if (now - _lastDebounceTime > 3) {
            _lastDebounceTime = now;
            if (A == 0) {  // A LOW
                Encoder::_counter += (B == 1) ? -1 : 1;  // B HIGH -> +1, else -1
            }
        }
        _lastA = A;
    }
    if (B != _lastB) _lastB = B;
}

void Encoder::begin() {
    pinMode(ENCODER_PIN_A, INPUT);
    pinMode(ENCODER_PIN_B, INPUT);

    // Leer estado inicial
    uint32_t reg = GPIO.in;
    _lastA = (reg >> ENCODER_PIN_A) & 1;
    _lastB = (reg >> ENCODER_PIN_B) & 1;
    _lastDebounceTime = millis();

    // Interrupts on both pins (SAT reacciona a cambios en A, pero lee B constantemente)
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), _isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), _isr, CHANGE);
}

void Encoder::update() { /* ISR lo maneja */ }

long Encoder::getCount() {
    noInterrupts();
    long v = _counter;
    interrupts();
    return v;
}

bool Encoder::hasChanged() {
    long cur = getCount();
    if (cur != _lastReported) {
        _lastReported = cur;
        return true;
    }
    return false;
}

void Encoder::reset() {
    noInterrupts();
    _counter      = 0;
    interrupts();
    _lastReported = 0;
}
