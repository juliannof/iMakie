#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <hardware/uart.h>
#include <pico/stdlib.h>
#include <hardware/irq.h>

// --- Configuración ---
#define UART_ID uart0
#define UART_TX_PIN 6
#define UART_RX_PIN 7
#define UART_BAUDRATE 921600

// --- Protocolo ---
#define USB_TO_ESP_START 0xAA
#define USB_TO_ESP_END   0xBB
#define ESP_TO_USB_START 0xCC
#define ESP_TO_USB_END   0xDD

// --- USB MIDI ---
Adafruit_USBD_MIDI usb_midi;

// --- Buffers ---
#define MIDI_BUFFER_SIZE 256
byte uart_rx_buffer[MIDI_BUFFER_SIZE];
uint16_t uart_rx_index = 0;
bool in_uart_frame = false;

void setup() {
    Serial.begin(115200);
    
    // Iniciar USB MIDI
    TinyUSB_Device_Init(0);
    usb_midi.begin();
    
    // Configurar UART a bajo nivel
    uart_init(UART_ID, UART_BAUDRATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_fifo_enabled(UART_ID, true);

    Serial.println("Pi Pico - Puente C++ (Lectura Byte a Byte) - READY");
}

// ---------------------------------------------------------------------------
// Procesar datos DE LOGIC (USB) y enviarlos al ESP32 (UART)
// ---------------------------------------------------------------------------
void process_usb_to_uart() {
  // Comprobar si hay bytes disponibles en el búfer de entrada del USB
  if (usb_midi.available()) {
    byte usb_data_block[128];
    uint16_t block_index = 0;

    // Leer todos los bytes disponibles, uno por uno, hasta un máximo
    while (usb_midi.available() && block_index < sizeof(usb_data_block)) {
        int byte_read = usb_midi.read(); // read() devuelve un solo byte como int, o -1
        if (byte_read != -1) {
            usb_data_block[block_index++] = (byte)byte_read;
        }
    }

    // Si hemos leído algún byte, lo enviamos como un solo bloque
    if (block_index > 0) {
        Serial.printf("[PICO] USB -> UART: Reenviando bloque de %d bytes.\n", block_index);

        uint8_t start_byte = USB_TO_ESP_START;
        uint8_t end_byte = USB_TO_ESP_END;
        
        uart_write_blocking(UART_ID, &start_byte, 1);
        uart_write_blocking(UART_ID, usb_data_block, block_index);
        uart_write_blocking(UART_ID, &end_byte, 1);
    }
  }
}

// ---------------------------------------------------------------------------
// Procesar datos DEL ESP32 (UART) y enviarlos a Logic (USB)
// ---------------------------------------------------------------------------
void process_uart_to_usb() {
    while (uart_is_readable(UART_ID)) {
        uint8_t byte = uart_getc(UART_ID);

        if (!in_uart_frame && byte == ESP_TO_USB_START) {
            in_uart_frame = true;
            uart_rx_index = 0;
        } else if (in_uart_frame) {
            if (byte == ESP_TO_USB_END) {
                if (uart_rx_index > 0) {
                    Serial.printf("[PICO] UART -> USB: Reenviando %d bytes a Logic.\n", uart_rx_index);
                    
                    // La función write puede manejar un buffer.
                    // TinyUSB es lo suficientemente inteligente para parsear SysEx vs NoteOn.
                    usb_midi.write(uart_rx_buffer, uart_rx_index);
                    usb_midi.flush();
                }
                in_uart_frame = false;
            } else if (uart_rx_index < sizeof(uart_rx_buffer)) {
                uart_rx_buffer[uart_rx_index++] = byte;
            }
        }
    }
}

// --- BUCLE PRINCIPAL ---
void loop() {
  process_usb_to_uart();
  process_uart_to_usb();
}