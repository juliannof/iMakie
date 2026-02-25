#ifndef USER_SETUP_H
#define USER_SETUP_H

#include <TFT_eSPI.h>

#define ST7796_DRIVER      // Define additional parameters below for this display
//#define ST7735_DRIVER      // Define additional parameters below for this display
#define TFT_WIDTH  320
#define TFT_HEIGHT 480
//#define ST7789_DRIVER      // Define additional parameters below for this display

//#define TFT_WIDTH  240
//#define TFT_HEIGHT 320

// Usando el bus HSPI
//#define TFT_SPI_PORT    HSPI

#define TFT_SCLK 39  // SCK/SCL
#define TFT_MOSI 37  // SDA
#define TFT_RST  35  // RST     Reset pin (could connect to RST pin) - 
#define TFT_DC   33  // RS  Data Command control pin
#define TFT_CS   18 // CS      Chip select control pin
#define TFT_BL   16  // LED/BLK LED back-light (required for M5Stack)

#define TFT_BACKLIGHT_ON HIGH
//#define TFT_BACKLIGHT_ON LOW

//#define TOUCH_CS -1

#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:-.
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

#define SMOOTH_FONT
#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000

#endif  // USER_SETUP_H