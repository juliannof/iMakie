<img alt="ESP32 S3" src="https://naylampmechatronics.com/img/cms/001206/Pinout%20ESP32-S3-DevKitC-1.jpg">

| **GPIO** | **Función Asignada** | **Dispositivo** | **Estado** |
| :--- | :--- | :--- | :--- |
| **0** | (Libre) | - | Boot |
| **1** | ENABLE RS485| RS485 |   |
| **2** | - | - |   |
| **3** | - | - |   |
| **4** | - | - |   |
| **5** | - | - |   |
| **6** | - | - |   |
| **7** | - | - |   |
| **8** | SDA I²C (Trellis) | NeoTrellis |  |
| **9** | SCL I²C (Trellis) | NeoTrellis |  |
| **10** | **SCK Pantalla** | Pantalla | |
| **11** | **MOSI Pantalla** | Pantalla | |
| **12** | **DC Pantalla** | Pantalla | |
| **13** | **RST Pantalla** | Pantalla |  |
| **14** | **CS Pantalla** | Pantalla | |
| **15** | RS485 ────► GPIO 15 (RX) ESP envía, Pico recibe | RS485 |  |
| **16** | RS485 GPIO 16 (RX) ◄──── GPIO 0 (TX) Pico envía, ESP recibe | RS485 |  |
| **17** |  - | - |   |
| **18** | **Backlight** | Pantalla | **FIJO (Tu elección)** |
| **19** | USB D- | USB Nativo | Reservado |
| **20** | USB D+ | USB Nativo | Reservado |
| **21** | - | - |   |
| **33-37**| ⛔ **PROHIBIDO** | **PSRAM N16R8** | **NO TOCAR** |
| **38** | - | - |   |
| **39** | - | - |   |
| **40** | - | - |   |
| **41** | - | - |   |
| **42** | - | - |   |


***

## 🗺️ Mapa de Pines: Unidad de Control S3-2 (Xtender)

Este mapa de pines está optimizado para **ESP32-S3 (N16R8)**, evitando pines reservados por la PSRAM Octal (33-37), USB Nativo (19-20) y Strapping Pins críticos.

| **GPIO** | **Función Asignada** | **Dispositivo** | **Estado** |
| :--- | :--- | :--- | :--- |
| **0** | (Libre) | - | Boot |
| **1** | ENABLE RS485| - |   |
| **2** |  (Libre) | - |   |
| **3** | LED 1 TRANSPORTE | - |   |
| **4** | LED 2 TRANSPORTE | - |   |
| **5** | LED 3 TRANSPORTE | - |   |
| **6** | LED 4 TRANSPORTE | - |   |
| **7** | LED 5 TRANSPORTE | - |   |
| **8** | BOTON 1 TRANSPORTE  | - |  |
| **9** | BOTON 2 TRANSPORTE | - |  |
| **10** |  BOTON 3 TRANSPORTE | - | |
| **11** |   BOTON 4 TRANSPORTE | - | |
| **12** |   BOTON 5 TRANSPORTE | - | |
| **13** | ENCODER 1 PIN3 | | |
| **14** | ENCODER 1 PIN1 | | |
| **15** | RS485 ────► GPIO 15 (RX) ESP envía, Pico recibe | RP2040 |  |
| **16** | RS485 GPIO 16 (RX) ◄──── GPIO 0 (TX) Pico envía, ESP recibe | RP2040 |  |
| **17** |  - | - |   |
| **18** | **Backlight** | Pantalla | **FIJO** |
| **19** | USB D- | USB Nativo | Reservado |
| **20** | USB D+ | USB Nativo | Reservado |
| **21** | - | - |   |
| **33-37**| ⛔ **PROHIBIDO** | **PSRAM N16R8** | **NO TOCAR** |
| **38** | - | - |   |
| **39** | - | - |   |
| **40** | - | - |   |
| **41** | - | - |   |
| **42** | - | - |   |
| **43** | U0TXD | - |  **NO TOCAR** |
| **44** | U0RXD| - |  **NO TOCAR** |
| **45** | - | - |   |
| **46** | - | - |   |
| **47** | - | - |   |


