# iMakie
A Mackie Control interface with ESP32

¡Por supuesto! Es totalmente posible hacer esos cambios. Ambos pines, GPIO 35 para el Potenciómetro (es una excelente opción de ADC) y el GPIO 16 para el PWM, son funcionales en el ESP32-S2.
Sin embargo, dado que el GPIO 16 estaba asignado al Encoder A, debemos mover el encoder a otros pines disponibles para evitar un conflicto.
Aquí tienes el pinout completo y corregido, optimizado para tus requisitos:
📌 Pinout Corregido para ESP32-S2
| Componente | Función | Definición | Pin GPIO | Notas de Compatibilidad |
|---|---|---|---|---|
| Fader (Pot.) | Posición Deseada | FADER_POT | 35 | ✅ ADC. Ideal, ya que es un pin de solo entrada. |
| --- | --- | --- | --- | --- |
| Motor PWM (IN1) | Control PWM | MOTOR_IN1 | 18 | Pin de PWM válido. |
| Motor PWM (IN2) | Control PWM | MOTOR_IN2 | 16 | Pin de PWM válido. (Reemplaza la función anterior del Encoder A) |
| Driver Enable (EEP) | Habilitación (HIGH) | DRV_ENABLE | 33 | Activación permanente del DRV8833. |
| --- | --- | --- | --- | --- |
| Encoder A (INT) | Interrupción | ENCODER_A | 14 | Movido (El GPIO 16 lo usa el PWM). |
| Encoder B (DIR) | Lectura de Dirección | ENCODER_B | 19 | Movido para tener continuidad cerca del A. |
| Botón Encoder (SW) | Pulsador | ENCODER_BUTTON | 2 | Movido a un pin GPIO disponible. |
| --- | --- | --- | --- | --- |
| Fader Touch | Detección Capacitiva | FADER_TOUCH | 4 | Pin Touch T0 dedicado. |
| --- | --- | --- | --- | --- |
| TFT - SCLK | Reloj SPI | TFT_SCLK | 5 | Comunicación de alta velocidad. |
| TFT - MOSI | Datos SPI | TFT_MOSI | 6 | Comunicación de alta velocidad. |
| TFT - CS | Chip Select | TFT_CS | 7 |  |
| TFT - DC | Data/Command | TFT_DC | 8 |  |
| TFT - RST | Reset | TFT_RST | 9 |  |
Resumen de Cambios:
 * Fader Potenciómetro: Movido del GPIO 34 a GPIO 35 (compatible con ADC).
 * Motor PWM (IN2): Movido del GPIO 15 a GPIO 16 (compatible con PWM).
 * Encoder: Los pines 16 y 17 fueron reasignados, por lo que el Encoder A/B y el botón se movieron a GPIO 14, 19 y 2.
