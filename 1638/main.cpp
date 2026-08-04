#include <Arduino.h>
#include <WiFi.h>
#include <TM1638plus.h>
#include "secret.h"

#define STB 4
#define CLK 5
#define DIO 6

const long gmtOffset = 19800; // IST = UTC+5:30
const int dstOffset = 0;

TM1638plus panel(STB, CLK, DIO, false);
uint8_t screen = 0;

void digit(uint8_t pos, uint8_t val, bool dot)
{
    panel.displayASCII(pos, '0' + val, dot ? CommonData::DecPointOn : CommonData::DecPointOff);
}

void showDate()
{
    struct tm t;
    if (!getLocalTime(&t)) return;

    int d = t.tm_mday, m = t.tm_mon + 1, y = t.tm_year + 1900;

    digit(0, d / 10, false);
    digit(1, d % 10, true);
    digit(2, m / 10, false);
    digit(3, m % 10, true);
    digit(4, y / 1000, false);
    digit(5, (y / 100) % 10, false);
    digit(6, (y / 10) % 10, false);
    digit(7, y % 10, false);
}

void showTime()
{
    struct tm t;
    if (!getLocalTime(&t)) return;

    int cs = (millis() % 1000) / 10;

    digit(0, t.tm_hour / 10, false);
    digit(1, t.tm_hour % 10, true);
    digit(2, t.tm_min / 10, false);
    digit(3, t.tm_min % 10, true);
    digit(4, t.tm_sec / 10, false);
    digit(5, t.tm_sec % 10, true);
    digit(6, cs / 10, false);
    digit(7, cs % 10, false);
}

void showUptime()
{
    unsigned long total = millis() / 1000;
    int dd = total / 86400;
    int hh = (total % 86400) / 3600;
    int mm = (total % 3600) / 60;
    int ss = total % 60;

    digit(0, dd / 10, false);
    digit(1, dd % 10, true);
    digit(2, hh / 10, false);
    digit(3, hh % 10, true);
    digit(4, mm / 10, false);
    digit(5, mm % 10, true);
    digit(6, ss / 10, false);
    digit(7, ss % 10, false);
}

void showTimezone()
{
    panel.displayASCII(0, ' ', CommonData::DecPointOff);
    panel.displayASCII(1, ' ', CommonData::DecPointOff);
    panel.displayASCII(2, ' ', CommonData::DecPointOff);
    panel.displayASCII(3, ' ', CommonData::DecPointOff);
    digit(4, 0, false);
    digit(5, 5, true);
    digit(6, 3, false);
    digit(7, 0, false);
}

void setup()
{
    panel.displayBegin();

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED)
        delay(500);

    configTime(gmtOffset, dstOffset, "pool.ntp.org");

    struct tm t;
    while (!getLocalTime(&t))
        delay(500);
}

void loop()
{
    uint8_t buttons = panel.readButtons();

    if (buttons & 0x01) screen = 0;
    if (buttons & 0x02) screen = 1;
    if (buttons & 0x04) screen = 2;
    if (buttons & 0x08) screen = 3;

    uint16_t leds = (1 << screen) | (1 << (screen + 4));
    panel.setLEDs(leds);

    switch (screen) {
        case 0: showDate(); break;
        case 1: showTime(); break;
        case 2: showUptime(); break;
        case 3: showTimezone(); break;
    }

    delay(50);
}
