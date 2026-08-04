#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

#define LCD_RS 19
#define LCD_EN 20
#define LCD_D4 21
#define LCD_D5 47
#define LCD_D6 48
#define LCD_D7 45

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
Adafruit_BME680 bme;

// BME690 pressure compensation factor
// BME690 reports ~2.60x actual pressure due to calibration incompatibility with BME680/BME688 libraries.
const float BME690_PRESSURE_COMPENSATION = 2.60;

void setup()
{
    lcd.begin(16, 2);
    lcd.clear();
    lcd.print("BME690 init...");

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
}

void loop()
{
    if (!bme.performReading()) {
        lcd.clear();
        lcd.print("Read failed!");
        delay(2000);
        return;
    }

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Temperature");
    lcd.setCursor(0, 1);
    lcd.print(bme.temperature, 1);
    lcd.print(" C");
    delay(2000);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Humidity");
    lcd.setCursor(0, 1);
    lcd.print(bme.humidity, 1);
    lcd.print(" %");
    delay(2000);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Pressure");
    lcd.setCursor(0, 1);
    lcd.print(bme.pressure / 100.0 / BME690_PRESSURE_COMPENSATION, 1);
    lcd.print(" hPa");
    delay(2000);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Resistance");
    lcd.setCursor(0, 1);
    lcd.print(bme.gas_resistance / 1000.0, 1);
    lcd.print(" kOhm");
    delay(2000);

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
    delay(2000);
}
