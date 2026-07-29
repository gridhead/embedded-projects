#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "data.h"

#define TFT_CS    4
#define TFT_DC    5
#define TFT_RST   6
#define TFT_MOSI  7
#define TFT_SCLK 15
#define TFT_MISO 16

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

void setup()
{
    Serial.begin(115200);

    SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI);
    tft.init(240, 320, SPI_MODE0);
    tft.invertDisplay(false);
    tft.setRotation(3);
    tft.setSPISpeed(80000000);
    tft.fillScreen(ST77XX_BLACK);

    Serial.println("Nyan cat initialized");
}

void loop()
{
    for (int i = 0; i < FRAME_COUNT; i++)
    {
        const uint16_t *frame = (const uint16_t *)pgm_read_ptr(&frames[i]);
        tft.startWrite();
        tft.setAddrWindow(0, 0, FRAME_W, FRAME_H);
        tft.writePixels((uint16_t *)frame, FRAME_W * FRAME_H);
        tft.endWrite();
        delay(27);
    }
}