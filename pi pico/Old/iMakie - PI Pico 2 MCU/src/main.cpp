#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <hardware/uart.h>
#include <pico/stdlib.h>

#define UART_ID uart0
#define UART_TX_PIN 6
#define UART_RX_PIN 7
#define UART_BAUDRATE 921600

#define USB_TO_ESP_START 0xAA
#define USB_TO_ESP_END   0xBB
#define ESP_TO_USB_START 0xCC
#define ESP_TO_USB_END   0xDD

//-----------------------------------------------
// Interface Config (2 dispositivos)
//-----------------------------------------------
Adafruit_USBD_MIDI usb_midi_pro;    // iMakie Pro (interface principal)
Adafruit_USBD_MIDI usb_midi_ext;    // iMakie Extender (interface extendida)

// UART Buffers (optimizado)
#define UART_BUFFER_SIZE 256
uint8_t uart_rx_buffer[UART_BUFFER_SIZE];
uint16_t uart_rx_index = 0;
uint8_t uart_current_iface = 0;     // Interface destino (0: Pro, 1: Ext)

// UART State Machine
enum {
  UART_STATE_WAIT_START,
  UART_STATE_WAIT_IFACE,
  UART_STATE_READ_DATA
} uart_state = UART_STATE_WAIT_START;

//================================ SETUP ================================
void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    // Inicialización de interfaces MIDI USB
    usb_midi_pro.setStringDescriptor("iMakie Pro");
    usb_midi_ext.setStringDescriptor("iMakie Extender");
    usb_midi_pro.begin();
    usb_midi_ext.begin();
    TinyUSB_Device_Init(0);
    
    // Configuración UART
    uart_init(UART_ID, UART_BAUDRATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_fifo_enabled(UART_ID, true);
    
    Serial.println(">> Pico MIDI Bridge DUAL Interface << READY");
}

//===================== USB -> UART ====================
void process_usb_to_uart(Adafruit_USBD_MIDI &interface, uint8_t iface_id) {
  if(!interface.available()) return;
  
  uint8_t midi_buffer[64];  // Buffer local (ajustable)
  uint16_t count = 0;
  
  // Leer todos los bytes disponibles
  while(interface.available() && count < sizeof(midi_buffer)) {
    int b = interface.read();
    if(b >= 0) midi_buffer[count++] = (uint8_t)b;
  }
  
  if(count > 0) {
    // Formato: [START][IFACE][DATA][END]
    uint8_t header[] = {USB_TO_ESP_START, iface_id};
    uart_write_blocking(UART_ID, header, 2);
    uart_write_blocking(UART_ID, midi_buffer, count);
    uart_write_blocking(UART_ID, &USB_TO_ESP_END, 1);
    
    Serial.printf("<< USB(%d) -> UART: %d bytes\n", iface_id, count);
  }
}

//===================== UART -> USB ====================
void process_uart_to_usb() {
  while(uart_is_readable(UART_ID)){
    uint8_t b = uart_getc(UART_ID);
    
    switch(uart_state) {
      case UART_STATE_WAIT_START:
        if(b == ESP_TO_USB_START) {
          uart_state = UART_STATE_WAIT_IFACE;
        }
        break;
        
      case UART_STATE_WAIT_IFACE:
        if(b < 2) {  // 0 o 1
          uart_current_iface = b;
          uart_rx_index = 0;
          uart_state = UART_STATE_READ_DATA;
        } else {
          uart_state = UART_STATE_WAIT_START;  // Reset on error
        }
        break;
        
      case UART_STATE_READ_DATA:
        if(b == ESP_TO_USB_END) {
          // Enviar a la interfaz correspondiente
          if(uart_rx_index > 0) {
            if(uart_current_iface == 0) {
              usb_midi_pro.write(uart_rx_buffer, uart_rx_index);
              usb_midi_pro.flush();
            } else {
              usb_midi_ext.write(uart_rx_buffer, uart_rx_index);
              usb_midi_ext.flush();
            }
            Serial.printf(">> UART -> USB(%d): %d bytes\n", uart_current_iface, uart_rx_index);
          }
          uart_state = UART_STATE_WAIT_START;
        } else if(uart_rx_index < UART_BUFFER_SIZE) {
          uart_rx_buffer[uart_rx_index++] = b;
        }
        break;
    }
  }
}

//================================ LOOP ================================
void loop() {
  // Procesar ambas interfaces USB
  process_usb_to_uart(usb_midi_pro, 0);  // Interface 0: Pro
  process_usb_to_uart(usb_midi_ext, 1);  // Interface 1: Ext
  
  // Procesar UART
  process_uart_to_usb();
}