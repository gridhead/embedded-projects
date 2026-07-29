#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define I2C_SDA 4
#define I2C_SCL 5

LiquidCrystal_I2C lcd(0x27, 20, 4);

void scanI2C()
{
    Serial.println("Scanning I2C bus...");
    int found = 0;
    for (byte addr = 1; addr < 127; addr++)
    {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0)
        {
            Serial.print("  Found device at 0x");
            if (addr < 16) Serial.print("0");
            Serial.println(addr, HEX);
            found++;
        }
    }
    if (found == 0)
        Serial.println("  No devices found!");
    else
        Serial.println("Scan complete.");
}

void setup()
{
    Serial.begin(115200);
    Wire.begin(I2C_SDA, I2C_SCL);
    delay(100);

    scanI2C();

    lcd.init();
    lcd.backlight();

    lcd.setCursor(0, 0);
    lcd.print("Hello from");
    lcd.setCursor(0, 1);
    lcd.print("ESP32-S3!");
    lcd.setCursor(0, 2);
    lcd.print("20x4 LCD via I2C");
    lcd.setCursor(0, 3);
    lcd.print("Addr: 0x27");

    Serial.println("LCD initialized");
}

void loop()
{
    lcd.setCursor(0, 3);
    lcd.print("Up: ");
    lcd.print(millis() / 1000);
    lcd.print("s   ");
    delay(1000);
}