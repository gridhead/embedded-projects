#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

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
    tft.init(240, 320);
    tft.invertDisplay(false);
    tft.setRotation(1);
    tft.fillScreen(ST77XX_BLACK);

    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(3);
    tft.setCursor(40, 60);
    tft.print("Hello from");
    tft.setCursor(40, 100);
    tft.print("ESP32-S3!");

    tft.setTextColor(ST77XX_GREEN);
    tft.setTextSize(2);
    tft.setCursor(40, 160);
    tft.print("ST7789 240x320");

    Serial.println("TFT initialized");
}

void loop()
{
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setCursor(40, 200);
    tft.print("Up: ");
    tft.print(millis() / 1000);
    tft.print("s    ");
    delay(1000);
}