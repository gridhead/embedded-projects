#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "font.h"
#include "logo.h"

#define TFT_CS    4
#define TFT_DC    5
#define TFT_RST   6
#define TFT_MOSI  7
#define TFT_SCLK 15
#define TFT_MISO 16

#define FEDORA_BLUE 0x551B
#define BG          ST77XX_BLACK
#define FG          ST77XX_WHITE

#define INFO_X   104
#define START_Y  17
#define LINE_H   17

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 canvas(320, 240);

void drawLogo()
{
    int lx = (INFO_X - LOGO_W) / 2;
    int ly = (240 - LOGO_H) / 2;
    canvas.drawRGBBitmap(lx, ly, logo_bitmap, LOGO_W, LOGO_H);
}

void drawLabel(int line, const char* label)
{
    canvas.setFont(&font7pt7b);
    canvas.setTextSize(1);
    canvas.setCursor(INFO_X, START_Y + line * LINE_H);
    canvas.setTextColor(FEDORA_BLUE);
    canvas.print(label);
}

void drawValue(const char* value)
{
    canvas.setTextColor(FG);
    canvas.print(value);
}

void drawHeader()
{
    canvas.setFont(&font7pt7b);
    canvas.setTextSize(1);
    canvas.setCursor(INFO_X, START_Y);
    canvas.setTextColor(FEDORA_BLUE);
    canvas.print("gridhead");
    canvas.setTextColor(FG);
    canvas.print("@");
    canvas.setTextColor(FEDORA_BLUE);
    canvas.print("esp32s3-wroom2");

    canvas.setCursor(INFO_X, START_Y + LINE_H);
    canvas.setTextColor(FG);
    canvas.print("---------------");
}

void drawStatic()
{
    char buf[40];

    drawLabel(2, "OS: ");
    drawValue("ESP-IDF (Arduino)");

    snprintf(buf, sizeof(buf), "%s WROOM-2", ESP.getChipModel());
    drawLabel(3, "Host: ");
    drawValue(buf);

    drawLabel(4, "Kernel: ");
    drawValue(ESP.getSdkVersion());

    snprintf(buf, sizeof(buf), "Xtensa LX7 @ %luMHz x%d",
             (unsigned long)ESP.getCpuFreqMHz(), ESP.getChipCores());
    drawLabel(6, "CPU: ");
    drawValue(buf);

    snprintf(buf, sizeof(buf), "%luK / %luK",
             (unsigned long)(ESP.getSketchSize() / 1024),
             (unsigned long)(ESP.getFlashChipSize() / 1024));
    drawLabel(9, "Flash: ");
    drawValue(buf);

    uint64_t mac = ESP.getEfuseMac();
    uint8_t* m = (uint8_t*)&mac;
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             m[0], m[1], m[2], m[3], m[4], m[5]);
    drawLabel(11, "MAC: ");
    drawValue(buf);
}

void drawDynamic()
{
    char buf[40];

    unsigned long sec = millis() / 1000;
    snprintf(buf, sizeof(buf), "%luh %lum %lus",
             sec / 3600, (sec % 3600) / 60, sec % 60);
    drawLabel(5, "Uptime: ");
    drawValue(buf);

    snprintf(buf, sizeof(buf), "%luK / %luK",
             (unsigned long)(ESP.getFreeHeap() / 1024),
             (unsigned long)(ESP.getHeapSize() / 1024));
    drawLabel(7, "Memory: ");
    drawValue(buf);

    snprintf(buf, sizeof(buf), "%luK / %luK",
             (unsigned long)(ESP.getFreePsram() / 1024),
             (unsigned long)(ESP.getPsramSize() / 1024));
    drawLabel(8, "PSRAM: ");
    drawValue(buf);

    snprintf(buf, sizeof(buf), "%.1fC", temperatureRead());
    drawLabel(10, "Temperature: ");
    drawValue(buf);
}

void drawPalette()
{
    uint16_t colors[] = {
        0x4208, ST77XX_RED, ST77XX_GREEN, ST77XX_YELLOW,
        ST77XX_BLUE, ST77XX_MAGENTA, ST77XX_CYAN, ST77XX_WHITE,
    };
    int y = START_Y + 12 * LINE_H + 4;
    for (int i = 0; i < 8; i++)
        canvas.fillRect(INFO_X + i * 24, y, 20, 12, colors[i]);
}

void render()
{
    canvas.fillScreen(BG);
    drawLogo();
    drawHeader();
    drawStatic();
    drawDynamic();
    drawPalette();
    tft.drawRGBBitmap(0, 0, canvas.getBuffer(), 320, 240);
}

void setup()
{
    Serial.begin(115200);

    SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI);
    tft.init(240, 320);
    tft.invertDisplay(false);
    tft.setRotation(3);

    render();

    Serial.println("Neofetch TUI initialized");
}

void loop()
{
    render();
    delay(500);
}
