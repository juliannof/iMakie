#ifndef USER_SETUP_H
#define USER_SETUP_H

#include <TFT_eSPI.h>

// Define the display driver chip type
#define ST7796_DRIVER
//#define ST7789_DRIVER      // Define additional parameters below for this display

// #define TFT_WIDTH  240
//#define TFT_HEIGHT 240

//#define TFT_RGB_ORDER TFT_BGR    //   Red-Green-Blue
#define USE_HSPI_PORT


#define TFT_WIDTH  320 // ST7789 240 x 240 and 240 x 320
#define TFT_HEIGHT 480 // ST7789 240 x 320


#define TFT_SCLK 13 // SCK/SCL
#define TFT_MOSI 12  // SDA
#define TFT_RST  11 // RST     Reset pin (could connect to RST pin) - 
#define TFT_DC   10 // RS  Data Command control pin
#define TFT_CS    7  // CS      Chip select control pin
#define TOUCH_CS -1
#define TFT_BL   6  // LED/BLK LED back-light (required for M5Stack)


#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:-.
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
//#define LOAD_FONT8N // Font 8. Alternative to Font 8 above, slightly narrower, so 3 digits fit a 160 pixel TFT
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:-.
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

#define SMOOTH_FONT
#define SPI_FREQUENCY  20000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000

#endif  // USER_SETUP_H