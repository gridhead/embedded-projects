#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <SPI.h>
#include "esp_netif.h"
#include "secret.h"

#define ETH_SCLK  15
#define ETH_MOSI  6
#define ETH_MISO  5
#define ETH_CS    4
#define ETH_RST   7
#define ETH_INT   16

static volatile bool ethUp = false;
static volatile bool wifiUp = false;
static int apClients = 0;

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
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            apClients++;
            break;
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            if (apClients > 0) apClients--;
            break;
        default:
            break;
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    WiFi.onEvent(onEvent);

    WiFi.mode(WIFI_AP_STA);

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (!wifiUp)
        delay(500);

    WiFi.softAPConfig(IPAddress(10, 1, 1, 1), IPAddress(10, 1, 1, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP(SPOT_SSID, SPOT_PASS);

    ETH.begin(ETH_PHY_W5500, 1, ETH_CS, ETH_INT, ETH_RST, SPI2_HOST, ETH_SCLK, ETH_MISO, ETH_MOSI);
    ETH.config(IPAddress(10, 0, 0, 6), IPAddress(0, 0, 0, 0), IPAddress(255, 255, 255, 0));

    while (!ethUp)
        delay(100);

    esp_netif_t *wifi_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_t *eth_netif = esp_netif_get_handle_from_ifkey("ETH_DEF");
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

    esp_netif_set_default_netif(wifi_netif);
    esp_netif_napt_enable(eth_netif);
    esp_netif_napt_enable(ap_netif);

}

void loop()
{
    static unsigned long last = 0;

    if (millis() - last >= 10000)
    {
        last = millis();
        unsigned long sec = millis() / 1000;
        Serial.printf("[%luh %lum %lus] \t STA: %s (%s) | AP: %s (%d device(s)) | ETH: %s (%s) | Temp: %.1fC\n",
                      sec / 3600, (sec % 3600) / 60, sec % 60,
                      WiFi.localIP().toString().c_str(),
                      wifiUp ? "UP" : "DN",
                      WiFi.softAPIP().toString().c_str(),
                      apClients,
                      ETH.localIP().toString().c_str(),
                      ethUp ? "UP" : "DN",
                      temperatureRead());
    }
}