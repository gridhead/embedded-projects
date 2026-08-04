#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

Adafruit_BME680 bme;

// BME690 pressure compensation factor
// BME690 reports ~2.60x actual pressure due to calibration incompatibility with BME680/BME688 libraries.
const float BME690_PRESSURE_COMPENSATION = 2.60;

void setup()
{
    Serial.begin(115200);
    while (!Serial) delay(10);

    Wire.begin(10, 9);

    if (!bme.begin(0x76)) {
        Serial.println("BME690 not found");
        while (1) delay(10);
    }

    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);

    Serial.println("BME690 initialized");
}

void loop()
{
    if (!bme.performReading()) {
        Serial.println("Reading failed");
        delay(2000);
        return;
    }

    Serial.print("Temp: ");
    Serial.print(bme.temperature);
    Serial.println(" C");

    Serial.print("Humidity: ");
    Serial.print(bme.humidity);
    Serial.println(" %");

    Serial.print("Pressure: ");
    Serial.print(bme.pressure / 100.0 / BME690_PRESSURE_COMPENSATION);
    Serial.println(" hPa");

    Serial.print("Gas: ");
    Serial.print(bme.gas_resistance / 1000.0);
    Serial.println(" kOhm");

    Serial.println("---");
    delay(2000);
}
