# iMakie
A Mackie Control interface with ESP32

// === PINES ===
// Driver Motor (DRV8833)
#define MOTOR_IN1 18     // PWM (Software)
#define MOTOR_IN2 16     // Dirección Opuesta (LOW)
#define DRV_ENABLE 33    // Habilitación del Driver (HIGH)

// Encoder Incremental (Posición Actual)
#define ENCODER_A 16     // Interrupción (INT A)
#define ENCODER_B 17     // Pin B (Dirección)

// === CONTROL Y CONSTANTES ===
const int PWM_FREQ_HZ = 200; 
const int PWM_PERIOD_US = (1000000 / PWM_FREQ_HZ); 

const float KP = 0.5;   
const int PWM_MAX = 255;
const int DEAD_BAND = 10; 
const long FIXED_TARGET = 500; 

