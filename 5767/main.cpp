#include <Arduino.h>
#include <Wire.h>

float currentFreq = 98.3;

void writeFrequency(float freq)
{
    uint16_t pll = (4 * (freq * 1000000UL + 225000UL)) / 32768UL;

    byte data[5];

    data[0] = (pll >> 8) & 0x3F;
    data[1] = pll & 0xFF;
    data[2] = 0xB0;
    data[3] = 0x10;
    data[4] = 0x00;

    Wire.beginTransmission(0x60);
    Wire.write(data, 5);

    byte err = Wire.endTransmission();

    if (err != 0)
    {
        Serial.print("I2C Error: ");
        Serial.println(err);
    }

    delay(100);

    currentFreq = freq;
}

void readStatus()
{
    byte data[5];

    Wire.requestFrom(0x60, 5);

    for (int i = 0; i < 5; i++)
        data[i] = Wire.read();

    bool ready = data[0] & 0x80;
    bool bandLimit = data[0] & 0x40;
    bool stereo = data[2] & 0x80;

    byte adc = data[3] >> 4;

    Serial.println();
    Serial.println("------ STATUS ------");

    Serial.print("Frequency : ");
    Serial.print(currentFreq, 1);
    Serial.println(" MHz");

    Serial.print("Stereo    : ");
    Serial.println(stereo ? "YES" : "NO");

    Serial.print("Ready     : ");
    Serial.println(ready ? "YES" : "NO");

    Serial.print("BandLimit : ");
    Serial.println(bandLimit ? "YES" : "NO");

    Serial.print("Signal    : ");

    for (int i = 0; i < 15; i++)
    {
        if (i < adc)
            Serial.print("#");
        else
            Serial.print(".");
    }

    Serial.print(" (");
    Serial.print(adc);
    Serial.println("/15)");

    Serial.println("--------------------");
    Serial.println();
}

void setup()
{
    Serial.begin(115200);

    // ESP32-S3 I2C pins
    Wire.begin(4, 5);

    delay(100);

    writeFrequency(currentFreq);

    Serial.println();
    Serial.println("TEA5767 Console");
    Serial.println("----------------");
    Serial.println("+ : Tune Up");
    Serial.println("- : Tune Down");
    Serial.println("s : Read Status");
    Serial.println("Or type a frequency (e.g. 93.5)");
    Serial.println();
}

void loop()
{
    if (!Serial.available())
        return;

    char c = Serial.peek();

    if (c == '+')
    {
        Serial.read();

        currentFreq += 0.1;

        if (currentFreq > 108.0)
            currentFreq = 108.0;

        writeFrequency(currentFreq);
        readStatus();
    }
    else if (c == '-')
    {
        Serial.read();

        currentFreq -= 0.1;

        if (currentFreq < 87.5)
            currentFreq = 87.5;

        writeFrequency(currentFreq);
        readStatus();
    }
    else if (c == 's')
    {
        Serial.read();
        readStatus();
    }
    else
    {
        float f = Serial.parseFloat();

        if (f >= 87.5 && f <= 108.0)
        {
            writeFrequency(f);
            readStatus();
        }
    }

    while (Serial.available())
        Serial.read();
}
