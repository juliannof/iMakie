#include <Arduino.h>
#include <Button2.h>

// --- Pines (par LED/BTN por botón) ---
#define LED_1  12
#define BTN_1  11
#define LED_2  10
#define BTN_2   9
#define LED_3   8
#define BTN_3   7
#define LED_4   6
#define BTN_4   5
#define LED_5   4
#define BTN_5   3

const int LEDS[] = { LED_1, LED_2, LED_3, LED_4, LED_5 };

Button2 btn1(BTN_1);
Button2 btn2(BTN_2);
Button2 btn3(BTN_3);
Button2 btn4(BTN_4);
Button2 btn5(BTN_5);

bool ledState[5] = { false, false, false, false, false };

void setLed(int pin, bool on) {
    digitalWrite(pin, on ? LOW : HIGH); // ánodo común 3.3V
}

void setup() {
    Serial.begin(115200);

    for (int i = 0; i < 5; i++) {
        pinMode(LEDS[i], OUTPUT);
        setLed(LEDS[i], false);
    }

    // Secuencia izquierda → derecha sin delays bloqueantes
    for (int i = 0; i < 5; i++) {
        setLed(LEDS[i], true);
        delay(150);
        setLed(LEDS[i], false);
    }

    btn1.setPressedHandler([](Button2& b) { ledState[0] = !ledState[0]; setLed(LED_1, ledState[0]); });
    btn2.setPressedHandler([](Button2& b) { ledState[1] = !ledState[1]; setLed(LED_2, ledState[1]); });
    btn3.setPressedHandler([](Button2& b) { ledState[2] = !ledState[2]; setLed(LED_3, ledState[2]); });
    btn4.setPressedHandler([](Button2& b) { ledState[3] = !ledState[3]; setLed(LED_4, ledState[3]); });
    btn5.setPressedHandler([](Button2& b) { ledState[4] = !ledState[4]; setLed(LED_5, ledState[4]); });
}

void loop() {
    btn1.loop();
    btn2.loop();
    btn3.loop();
    btn4.loop();
    btn5.loop();
}