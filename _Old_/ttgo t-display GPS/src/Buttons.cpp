#include "Buttons.h"
#include "Display.h"
#include <driver/rtc_io.h>
#include "Config.h"

Button2 btn1(BUTTON_1);
Button2 btn2(BUTTON_2);
bool btnClick = false;

void enterDeepSleep() {
    // Obtener las fuentes de despertador actualmente habilitadas
    esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
    
    // Desactivar la fuente de despertador del temporizador si está habilitada
    if (wakeupCause == ESP_SLEEP_WAKEUP_TIMER) {
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    }


    // Configurar el GPIO35 como fuente de despertador
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
    delay(200);

    // Entrar en modo de sueño profundo
    esp_deep_sleep_start();
}


void button_init() {
    btn1.setLongClickHandler([](Button2 &b) {
        btnClick = false;
        
    });

    btn1.setPressedHandler([](Button2 &b) {
        Serial.println("btn 1 presionado");
        btnClick = true;
        Serial.println("Entrando en modo de sueño profundo...");
        enterDeepSleep();
    });

    btn2.setPressedHandler([](Button2 &b) {
        btnClick = false;
        Serial.println("btn 2 presionado");
        
    });
}


