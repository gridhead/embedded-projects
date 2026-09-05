#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <TM1638plus.h>
#include "esp_netif.h"
#include "secret.h"

#define ETH_SCLK  15
#define ETH_MOSI  6
#define ETH_MISO  5
#define ETH_CS    4
#define ETH_RST   7
#define ETH_INT   16

#define LCD_SDA   8
#define LCD_SCL   3

#define BME_SDA   41
#define BME_SCL   42

#define TM_STB    12
#define TM_CLK    13
#define TM_DIO    14

const long gmtOffset = 19800;
const int dstOffset = 0;

const float BME690_PRESSURE_COMPENSATION = 2.60;

LiquidCrystal_I2C lcd(0x27, 20, 4);
Adafruit_BME680 bme(&Wire1);
TM1638plus panel(TM_STB, TM_CLK, TM_DIO, false);

static volatile bool ethUp = false;
static volatile bool wifiUp = false;

volatile float envTemp = 0;
volatile float envHumidity = 0;
volatile float envPressure = 0;
volatile float envGas = 0;
volatile bool envReady = false;

uint8_t tmScreen = 0;

// --- TM1638 ---

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

// --- LCD + BME690 (core 0) ---

void lcdPrintPadded(const String &str)
{
    lcd.print(str);
    for (int i = str.length(); i < 20; i++)
        lcd.print(' ');
}

void onEvent(arduino_event_id_t event)
{
    switch (event)
    {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            wifiUp = true;
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            wifiUp = false;
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            ethUp = true;
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            ethUp = false;
            break;
        default:
            break;
    }
}

void i2cTask(void *param)
{
    Wire.begin(LCD_SDA, LCD_SCL);
    Wire1.begin(BME_SDA, BME_SCL);

    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.print("Initializing...");

    bool sensorOk = bme.begin(0x76);
    if (sensorOk)
    {
        bme.setTemperatureOversampling(BME680_OS_8X);
        bme.setHumidityOversampling(BME680_OS_2X);
        bme.setPressureOversampling(BME680_OS_4X);
        bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
        bme.setGasHeater(320, 150);
        lcd.clear();
    }
    else
    {
        lcd.clear();
        lcd.print("Sensor error!");
    }

    for (;;)
    {
        if (sensorOk && bme.performReading())
        {
            envTemp = bme.temperature;
            envHumidity = bme.humidity;
            envPressure = bme.pressure / 100.0 / BME690_PRESSURE_COMPENSATION;
            envGas = bme.gas_resistance / 1000.0;
            envReady = true;

            lcd.setCursor(0, 0);
            lcdPrintPadded("Temp: " + String(envTemp, 1) + " C");
            lcd.setCursor(0, 1);
            lcdPrintPadded("Humd: " + String(envHumidity, 1) + " %");
            lcd.setCursor(0, 2);
            lcdPrintPadded("Pres: " + String(envPressure, 1) + " hPa");
            lcd.setCursor(0, 3);
            lcdPrintPadded("Gres: " + String(envGas, 1) + " kOhm");
        }
        delay(2000);
    }
}

// --- Main (core 1) ---

void setup()
{
    Serial.begin(115200);
    delay(1000);

    WiFi.onEvent(onEvent);

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (!wifiUp)
        delay(500);

    configTime(gmtOffset, dstOffset, "pool.ntp.org");
    struct tm t;
    while (!getLocalTime(&t))
        delay(500);

    ETH.begin(ETH_PHY_W5500, 1, ETH_CS, ETH_INT, ETH_RST, SPI2_HOST, ETH_SCLK, ETH_MISO, ETH_MOSI);
    ETH.config(IPAddress(10, 0, 0, 6), IPAddress(0, 0, 0, 0), IPAddress(255, 255, 255, 0));

    while (!ethUp)
        delay(100);

    esp_netif_t *wifi_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_t *eth_netif = esp_netif_get_handle_from_ifkey("ETH_DEF");

    esp_netif_set_default_netif(wifi_netif);
    esp_netif_napt_enable(eth_netif);

    panel.displayBegin();

    xTaskCreatePinnedToCore(i2cTask, "i2c", 4096, NULL, 1, NULL, 0);
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

    switch (tmScreen)
    {
        case 0: tmShowDate(); break;
        case 1: tmShowTime(); break;
        case 2: tmShowUptime(); break;
        case 3: tmShowTimezone(); break;
    }

    static unsigned long last = 0;
    if (millis() - last >= 10000)
    {
        last = millis();
        unsigned long sec = millis() / 1000;
        Serial.printf("[%luh %lum %lus] \t WiFi: %s (%s) | ETH: %s (%s) | Temp: %.1fC\n",
                      sec / 3600, (sec % 3600) / 60, sec % 60,
                      WiFi.localIP().toString().c_str(),
                      wifiUp ? "UP" : "DN",
                      ETH.localIP().toString().c_str(),
                      ethUp ? "UP" : "DN",
                      temperatureRead());
    }

    delay(10);
}