#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <TM1638plus.h>
#include "secret.h"

#define I2C_SDA 10
#define I2C_SCL 9

#define TM_STB 4
#define TM_CLK 5
#define TM_DIO 6

const long gmtOffset = 19800;
const int dstOffset = 0;

LiquidCrystal_I2C lcd(0x27, 20, 4);
Adafruit_BME680 bme;
TM1638plus panel(TM_STB, TM_CLK, TM_DIO, false);

// BME690 reports ~2.60x actual pressure due to calibration incompatibility with BME680/BME688 libraries.
const float BME690_PRESSURE_COMPENSATION = 2.60;

uint8_t tmScreen = 0;

void tmDigit(uint8_t pos, uint8_t val, bool dot)
{
    panel.displayASCII(pos, '0' + val, dot ? CommonData::DecPointOn : CommonData::DecPointOff);
}

void tmShowDate()
{
    struct tm t;
    if (!getLocalTime(&t)) return;

    int d = t.tm_mday, m = t.tm_mon + 1, y = t.tm_year + 1900;

    tmDigit(0, d / 10, false);
    tmDigit(1, d % 10, true);
    tmDigit(2, m / 10, false);
    tmDigit(3, m % 10, true);
    tmDigit(4, y / 1000, false);
    tmDigit(5, (y / 100) % 10, false);
    tmDigit(6, (y / 10) % 10, false);
    tmDigit(7, y % 10, false);
}

void tmShowTime()
{
    struct tm t;
    if (!getLocalTime(&t)) return;

    int cs = (millis() % 1000) / 10;

    tmDigit(0, t.tm_hour / 10, false);
    tmDigit(1, t.tm_hour % 10, true);
    tmDigit(2, t.tm_min / 10, false);
    tmDigit(3, t.tm_min % 10, true);
    tmDigit(4, t.tm_sec / 10, false);
    tmDigit(5, t.tm_sec % 10, true);
    tmDigit(6, cs / 10, false);
    tmDigit(7, cs % 10, false);
}

void tmShowUptime()
{
    unsigned long total = millis() / 1000;
    int dd = total / 86400;
    int hh = (total % 86400) / 3600;
    int mm = (total % 3600) / 60;
    int ss = total % 60;

    tmDigit(0, dd / 10, false);
    tmDigit(1, dd % 10, true);
    tmDigit(2, hh / 10, false);
    tmDigit(3, hh % 10, true);
    tmDigit(4, mm / 10, false);
    tmDigit(5, mm % 10, true);
    tmDigit(6, ss / 10, false);
    tmDigit(7, ss % 10, false);
}

void tmShowTimezone()
{
    panel.displayASCII(0, ' ', CommonData::DecPointOff);
    panel.displayASCII(1, ' ', CommonData::DecPointOff);
    panel.displayASCII(2, ' ', CommonData::DecPointOff);
    panel.displayASCII(3, ' ', CommonData::DecPointOff);
    tmDigit(4, 0, false);
    tmDigit(5, 5, true);
    tmDigit(6, 3, false);
    tmDigit(7, 0, false);
}

void lcdPrintPadded(const String &str)
{
    lcd.print(str);
    for (int i = str.length(); i < 20; i++)
        lcd.print(' ');
}

void lcdShowRow(uint8_t row)
{
    lcd.setCursor(0, row);
    switch (row) {
        case 0: lcdPrintPadded("Temp: " + String(bme.temperature, 1) + " C"); break;
        case 1: lcdPrintPadded("Humd: " + String(bme.humidity, 1) + " %"); break;
        case 2: lcdPrintPadded("Pres: " + String(bme.pressure / 100.0 / BME690_PRESSURE_COMPENSATION, 1) + " hPa"); break;
        case 3: lcdPrintPadded("Gres: " + String(bme.gas_resistance / 1000.0, 1) + " kOhm"); break;
    }
}

void lcdTask(void *param)
{
    Wire.begin(I2C_SDA, I2C_SCL);

    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.print("Initializing...");

    if (!bme.begin(0x76)) {
        lcd.clear();
        lcd.print("Sensor error!");
        while (1) delay(10);
    }

    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED)
        delay(500);

    configTime(gmtOffset, dstOffset, "pool.ntp.org");

    struct tm t;
    while (!getLocalTime(&t))
        delay(500);

    lcd.clear();
    lcd.print("Ready");
    delay(500);

    for (;;) {
        if (bme.performReading()) {
            for (uint8_t row = 0; row < 4; row++)
                lcdShowRow(row);
        } else {
            lcd.clear();
            lcd.print("Read failed!");
        }
        delay(2000);
    }
}

void setup()
{
    panel.displayBegin();

    xTaskCreatePinnedToCore(lcdTask, "lcd", 4096, NULL, 1, NULL, 0);
}

void loop()
{
    uint8_t buttons = panel.readButtons();

    if (buttons & 0x01) tmScreen = 0;
    if (buttons & 0x02) tmScreen = 1;
    if (buttons & 0x04) tmScreen = 2;
    if (buttons & 0x08) tmScreen = 3;

    uint16_t leds = (1 << tmScreen) | (1 << (tmScreen + 4));
    panel.setLEDs(leds);

    switch (tmScreen) {
        case 0: tmShowDate(); break;
        case 1: tmShowTime(); break;
        case 2: tmShowUptime(); break;
        case 3: tmShowTimezone(); break;
    }

    delay(10);
}
