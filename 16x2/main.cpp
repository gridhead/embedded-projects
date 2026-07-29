#include <Arduino.h>
#include <LiquidCrystal.h>

#define LCD_RS 4
#define LCD_EN 5
#define LCD_D4 6
#define LCD_D5 7
#define LCD_D6 15
#define LCD_D7 16

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

void setup()
{
    Serial.begin(115200);
    delay(500);

    lcd.begin(16, 2);
    delay(100);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Hello from");
    lcd.setCursor(0, 1);
    lcd.print("ESP32-S3!");

    Serial.println("LCD initialized");
}

void loop()
{
    lcd.setCursor(0, 1);
    lcd.print("Up: ");
    lcd.print(millis() / 1000);
    lcd.print("s   ");
    delay(1000);
}