#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>

#define W5500_CS   40
#define W5500_MOSI 41
#define W5500_MISO 42
#define W5500_SCLK 1
#define W5500_RST  2

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 0, 0, 6);
IPAddress gateway(10, 0, 0, 1);
IPAddress subnet(255, 255, 255, 0);

void printNetworkInfo()
{
    Serial.println("\n== Network Configuration ==");
    Serial.printf("   MAC:     %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.printf("   IP:      %s\n", Ethernet.localIP().toString().c_str());
    Serial.printf("   Gateway: %s\n", Ethernet.gatewayIP().toString().c_str());
    Serial.printf("   Subnet:  %s\n", Ethernet.subnetMask().toString().c_str());
}

void printHardwareStatus()
{
    Serial.println("\n== Hardware Status ==");

    switch (Ethernet.hardwareStatus())
    {
        case EthernetNoHardware:
            Serial.println("   [FAIL] No Ethernet hardware detected");
            return;
        case EthernetW5100:
            Serial.println("   [INFO] W5100 detected");
            break;
        case EthernetW5200:
            Serial.println("   [INFO] W5200 detected");
            break;
        case EthernetW5500:
            Serial.println("   [PASS] W5500 detected");
            break;
        default:
            Serial.println("   [FAIL] Unknown hardware");
            return;
    }

    switch (Ethernet.linkStatus())
    {
        case LinkON:
            Serial.println("   [PASS] Link is UP");
            break;
        case LinkOFF:
            Serial.println("   [FAIL] Link is DOWN — check cable");
            break;
        default:
            Serial.println("   [WARN] Link status unknown");
            break;
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n== W5500 Ethernet Connectivity Diagnostics ==");

    pinMode(W5500_RST, OUTPUT);
    digitalWrite(W5500_RST, LOW);
    delay(50);
    digitalWrite(W5500_RST, HIGH);
    delay(200);

    SPI.begin(W5500_SCLK, W5500_MISO, W5500_MOSI, W5500_CS);
    Ethernet.init(W5500_CS);
    Ethernet.begin(mac, ip, IPAddress(0, 0, 0, 0), gateway, subnet);

    printHardwareStatus();
    printNetworkInfo();

    Serial.println("\n== Board Info ==");
    Serial.printf("   Chip:    %s\n", ESP.getChipModel());
    Serial.printf("   CPU:     %lu MHz x%d cores\n",
                  (unsigned long)ESP.getCpuFreqMHz(), ESP.getChipCores());
    Serial.printf("   Heap:    %luK free / %luK total\n",
                  (unsigned long)(ESP.getFreeHeap() / 1024),
                  (unsigned long)(ESP.getHeapSize() / 1024));
    Serial.printf("   PSRAM:   %luK free / %luK total\n",
                  (unsigned long)(ESP.getFreePsram() / 1024),
                  (unsigned long)(ESP.getPsramSize() / 1024));
    Serial.printf("   Temp:    %.1fC\n", temperatureRead());

    Serial.println("\n== Setup complete ==\n");
}

void loop()
{
    static unsigned long last = 0;

    if (millis() - last >= 5000)
    {
        last = millis();
        unsigned long sec = millis() / 1000;
        Serial.printf("[%luh %lum %lus] \t Link: %s | IP: %s | Heap: %luK | Temp: %.1fC\n",
                      sec / 3600, (sec % 3600) / 60, sec % 60,
                      Ethernet.linkStatus() == LinkON ? "UP" : "DOWN",
                      Ethernet.localIP().toString().c_str(),
                      (unsigned long)(ESP.getFreeHeap() / 1024),
                      temperatureRead());
    }
}