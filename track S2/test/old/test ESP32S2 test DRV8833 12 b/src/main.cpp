#include <Arduino.h>

const int MOTOR_IN1 = 18;
const int MOTOR_IN2 = 16;
const int ENABLE_PIN = 34;

const int PWM_FREQ = 20000;      // Mantenemos 20kHz
const int PWM_RESOLUTION = 10;   // ✅ 10-bit (0-1023) - podría funcionar
const int PWM_CHANNEL_IN1 = 0;
const int PWM_CHANNEL_IN2 = 1;

void setup() {
  Serial.begin(115200);
  
  pinMode(ENABLE_PIN, OUTPUT);
  
  
  ledcSetup(PWM_CHANNEL_IN1, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(MOTOR_IN1, PWM_CHANNEL_IN1);
  ledcSetup(PWM_CHANNEL_IN2, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(MOTOR_IN2, PWM_CHANNEL_IN2);

  Serial.println("Probando 10 bits a 20kHz...");
  
  // Convertir 157 (8-bit) a 10-bit: 157/255 = x/1023 → x ≈ 630
  ledcWrite(PWM_CHANNEL_IN1, 630);
  ledcWrite(PWM_CHANNEL_IN2, 0);
  digitalWrite(ENABLE_PIN, HIGH);
}

void loop() {
  delay(100);
}