#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <TM1638plus.h>
#include "secret.h"

#define LCD_RS 19
#define LCD_EN 20
#define LCD_D4 21
#define LCD_D5 47
#define LCD_D6 48
#define LCD_D7 45

#define TM_STB 4
#define TM_CLK 5
#define TM_DIO 6

const long gmtOffset = 19800;
const int dstOffset = 0;

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
Adafruit_BME680 bme;
TM1638plus panel(TM_STB, TM_CLK, TM_DIO, false);

// BME690 pressure compensation factor
// BME690 reports ~2.60x actual pressure due to calibration incompatibility with BME680/BME688 libraries.
const float BME690_PRESSURE_COMPENSATION = 2.60;

uint8_t tmScreen = 0;
uint8_t lcdScreen = 0;
unsigned long lcdLastSwitch = 0;
const unsigned long lcdInterval = 2000;
bool sensorReady = false;
bool readingInProgress = false;

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

void lcdShowTemperature()
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Temperature");
    lcd.setCursor(0, 1);
    lcd.print(bme.temperature, 1);
    lcd.print(" C");
}

void lcdShowHumidity()
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Humidity");
    lcd.setCursor(0, 1);
    lcd.print(bme.humidity, 1);
    lcd.print(" %");
}

void lcdShowPressure()
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Pressure");
    lcd.setCursor(0, 1);
    lcd.print(bme.pressure / 100.0 / BME690_PRESSURE_COMPENSATION, 1);
    lcd.print(" hPa");
}

void lcdShowResistance()
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Resistance");
    lcd.setCursor(0, 1);
    lcd.print(bme.gas_resistance / 1000.0, 1);
    lcd.print(" kOhm");
}

void lcdShowUptime()
{
    unsigned long total_secs = millis() / 1000;
    unsigned long days = total_secs / 86400;
    unsigned long hours = (total_secs % 86400) / 3600;
    unsigned long mins = (total_secs % 3600) / 60;
    unsigned long secs = total_secs % 60;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Uptime");
    lcd.setCursor(0, 1);
    if (days > 0) {
        lcd.print(days);
        lcd.print("d ");
    }
    lcd.print(hours);
    lcd.print("h ");
    lcd.print(mins);
    lcd.print("m ");
    lcd.print(secs);
    lcd.print("s");
}

void setup()
{
    lcd.begin(16, 2);
    lcd.clear();
    lcd.print("Initializing...");

    panel.displayBegin();

    Wire.begin(10, 9);

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

    readingInProgress = (bme.beginReading() != 0);
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

    if (readingInProgress && bme.remainingReadingMillis() <= 0) {
        sensorReady = bme.endReading();
        readingInProgress = false;
    }

    unsigned long now = millis();
    if (now - lcdLastSwitch >= lcdInterval) {
        lcdLastSwitch = now;

        if (!sensorReady) {
            lcd.clear();
            lcd.print("Read failed!");
        } else {
            switch (lcdScreen) {
                case 0: lcdShowTemperature(); break;
                case 1: lcdShowHumidity(); break;
                case 2: lcdShowPressure(); break;
                case 3: lcdShowResistance(); break;
                case 4: lcdShowUptime(); break;
            }
        }

        lcdScreen = (lcdScreen + 1) % 5;

        if (lcdScreen == 0 && !readingInProgress) {
            readingInProgress = (bme.beginReading() != 0);
            if (!readingInProgress) sensorReady = false;
        }
    }

    delay(10);
}
