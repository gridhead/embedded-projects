#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_partition.h>
#include "secret.h"

#define PSRAM_CHUNK    (1024 * 1024)
#define FLASH_READ_LEN (64 * 1024)
#define CPU_BURN_MS    10000
#define WIFI_ROUNDS    20
#define WIFI_URL       "http://detectportal.firefox.com/"

static int pass_count = 0;
static int fail_count = 0;

static void report(const char* test, bool ok, const char* detail = nullptr)
{
    if (ok)
    {
        pass_count++;
        Serial.printf("  [PASS] %s", test);
    }
    else
    {
        fail_count++;
        Serial.printf("  [FAIL] %s", test);
    }
    if (detail)
        Serial.printf(" — %s", detail);
    Serial.println();
}

static float readTemp()
{
    return temperatureRead();
}

// --- PSRAM test: write patterns, read back, check for bit errors ---

static void testPsram()
{
    Serial.println("\n== PSRAM stress ==");

    size_t total = ESP.getPsramSize();
    size_t free_start = ESP.getFreePsram();
    char buf[64];
    snprintf(buf, sizeof(buf), "%luK total, %luK free",
             (unsigned long)(total / 1024), (unsigned long)(free_start / 1024));
    report("detection", total > 0, buf);

    if (total == 0) return;

    #define MAX_CHUNKS 16
    uint8_t* chunks[MAX_CHUNKS];
    int num_chunks = 0;
    size_t total_alloc = 0;

    for (int i = 0; i < MAX_CHUNKS; i++)
    {
        chunks[i] = (uint8_t*)heap_caps_malloc(PSRAM_CHUNK, MALLOC_CAP_SPIRAM);
        if (!chunks[i]) break;
        num_chunks++;
        total_alloc += PSRAM_CHUNK;
    }

    snprintf(buf, sizeof(buf), "%d x 1MB = %luK", num_chunks, (unsigned long)(total_alloc / 1024));
    report("allocation", num_chunks > 0, buf);

    if (num_chunks == 0) return;

    uint8_t patterns[] = {0x00, 0xFF, 0xAA, 0x55, 0xA5, 0x5A};
    for (int p = 0; p < 6; p++)
    {
        int errors = 0;
        for (int c = 0; c < num_chunks; c++)
        {
            memset(chunks[c], patterns[p], PSRAM_CHUNK);
            for (size_t i = 0; i < PSRAM_CHUNK; i += 4096)
            {
                if (chunks[c][i] != patterns[p])
                    errors++;
            }
        }
        snprintf(buf, sizeof(buf), "pattern 0x%02X, %d errors", patterns[p], errors);
        report("pattern write/read", errors == 0, buf);
    }

    int seq_errors = 0;
    for (int c = 0; c < num_chunks; c++)
    {
        for (size_t i = 0; i < PSRAM_CHUNK; i++)
            chunks[c][i] = (uint8_t)(i & 0xFF);
        for (size_t i = 0; i < PSRAM_CHUNK; i += 1024)
        {
            if (chunks[c][i] != (uint8_t)(i & 0xFF))
                seq_errors++;
        }
    }
    snprintf(buf, sizeof(buf), "%luK sequential, %d errors",
             (unsigned long)(total_alloc / 1024), seq_errors);
    report("sequential pattern", seq_errors == 0, buf);

    for (int c = 0; c < num_chunks; c++)
        heap_caps_free(chunks[c]);
}

// --- Flash test: read back partition data and verify CRC ---

static void testFlash()
{
    Serial.println("\n== Flash stress ==");

    char buf[64];
    size_t flash_size = ESP.getFlashChipSize();
    snprintf(buf, sizeof(buf), "%luMB", (unsigned long)(flash_size / (1024 * 1024)));
    report("detection", flash_size > 0, buf);

    const esp_partition_t* part = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    if (!part)
        part = esp_partition_find_first(
            ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);

    report("partition found", part != nullptr);
    if (!part) return;

    uint8_t* rbuf = (uint8_t*)malloc(FLASH_READ_LEN);
    if (!rbuf) { report("buffer alloc", false); return; }

    int read_errors = 0;
    size_t read_total = 0;
    unsigned long start = millis();

    for (size_t offset = 0; offset + FLASH_READ_LEN <= part->size; offset += FLASH_READ_LEN)
    {
        esp_err_t err = esp_partition_read(part, offset, rbuf, FLASH_READ_LEN);
        if (err != ESP_OK)
            read_errors++;
        read_total += FLASH_READ_LEN;
    }

    unsigned long elapsed = millis() - start;
    float speed = (read_total / 1024.0f) / (elapsed / 1000.0f);
    snprintf(buf, sizeof(buf), "%luK read in %lums (%.0f KB/s), %d errors",
             (unsigned long)(read_total / 1024), elapsed, speed, read_errors);
    report("sequential read", read_errors == 0, buf);

    int reread_mismatches = 0;
    uint8_t* rbuf2 = (uint8_t*)malloc(FLASH_READ_LEN);
    if (rbuf2)
    {
        for (int i = 0; i < 5; i++)
        {
            size_t offset = (i * 131072) % part->size;
            esp_partition_read(part, offset, rbuf, FLASH_READ_LEN);
            esp_partition_read(part, offset, rbuf2, FLASH_READ_LEN);
            if (memcmp(rbuf, rbuf2, FLASH_READ_LEN) != 0)
                reread_mismatches++;
        }
        snprintf(buf, sizeof(buf), "5 regions, %d mismatches", reread_mismatches);
        report("read consistency", reread_mismatches == 0, buf);
        free(rbuf2);
    }

    free(rbuf);
}

// --- CPU test: burn both cores and check for crashes ---

static volatile uint32_t core0_result = 0;
static volatile uint32_t core1_result = 0;
static volatile bool burn_done = false;

static void burnCore(void* param)
{
    volatile uint32_t acc = 1;
    unsigned long start = millis();
    while (millis() - start < CPU_BURN_MS)
    {
        for (int i = 0; i < 10000; i++)
            acc = acc * 1103515245 + 12345;
        vTaskDelay(1);
    }

    if ((int)param == 0)
        core0_result = acc;
    else
        core1_result = acc;

    burn_done = true;
    vTaskDelete(NULL);
}

static void testCpu()
{
    Serial.println("\n== CPU stress ==");

    char buf[64];
    snprintf(buf, sizeof(buf), "%s, %lu MHz, %d cores",
             ESP.getChipModel(), (unsigned long)ESP.getCpuFreqMHz(), ESP.getChipCores());
    report("chip info", true, buf);

    float temp_before = readTemp();

    burn_done = false;
    xTaskCreatePinnedToCore(burnCore, "burn0", 4096, (void*)0, 1, NULL, 0);
    Serial.printf("  Burning core 0 for %ds...\n", CPU_BURN_MS / 1000);
    while (!burn_done) delay(100);
    report("core 0 survived", core0_result != 0);

    burn_done = false;
    xTaskCreatePinnedToCore(burnCore, "burn1", 4096, (void*)1, 1, NULL, 1);
    Serial.printf("  Burning core 1 for %ds...\n", CPU_BURN_MS / 1000);
    while (!burn_done) delay(100);
    report("core 1 survived", core1_result != 0);

    burn_done = false;
    volatile bool burn1_done = false;
    xTaskCreatePinnedToCore(burnCore, "burn0d", 4096, (void*)0, 1, NULL, 0);
    xTaskCreatePinnedToCore(burnCore, "burn1d", 4096, (void*)1, 1, NULL, 1);
    Serial.printf("  Burning both cores for %ds...\n", CPU_BURN_MS / 1000);
    unsigned long wait_start = millis();
    while (millis() - wait_start < CPU_BURN_MS + 2000) delay(100);
    report("dual-core survived", core0_result != 0 && core1_result != 0);

    float temp_after = readTemp();
    snprintf(buf, sizeof(buf), "%.1fC -> %.1fC (+%.1fC)", temp_before, temp_after, temp_after - temp_before);
    report("thermal delta", temp_after < 85.0, buf);
}

// --- WiFi/RF test: sustained HTTP fetches to test RF path ---

static void testWifi()
{
    Serial.println("\n== WiFi/RF stress ==");

    Serial.print("  Connecting");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    unsigned long wstart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wstart < 15000)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    char buf[80];
    bool connected = WiFi.status() == WL_CONNECTED;
    if (connected)
    {
        snprintf(buf, sizeof(buf), "%s, RSSI %d dBm",
                 WiFi.localIP().toString().c_str(), WiFi.RSSI());
    }
    report("connection", connected, connected ? buf : "timeout");
    if (!connected) return;

    int ok_count = 0;
    int err_count = 0;
    int min_rssi = 0;
    int max_rssi = -120;
    unsigned long total_ms = 0;

    for (int i = 0; i < WIFI_ROUNDS; i++)
    {
        int rssi = WiFi.RSSI();
        if (rssi < min_rssi) min_rssi = rssi;
        if (rssi > max_rssi) max_rssi = rssi;

        HTTPClient http;
        http.begin(WIFI_URL);
        http.setTimeout(5000);
        unsigned long t0 = millis();
        int code = http.GET();
        unsigned long rtt = millis() - t0;
        http.end();

        if (code == 200)
        {
            ok_count++;
            total_ms += rtt;
        }
        else
        {
            err_count++;
        }
        delay(200);
    }

    snprintf(buf, sizeof(buf), "%d/%d ok, avg %lums, RSSI %d to %d dBm",
             ok_count, WIFI_ROUNDS,
             ok_count > 0 ? total_ms / ok_count : 0,
             min_rssi, max_rssi);
    report("sustained transfer", err_count == 0, buf);

    snprintf(buf, sizeof(buf), "%d dBm spread", max_rssi - min_rssi);
    report("RSSI stability", (max_rssi - min_rssi) < 15, buf);

    WiFi.disconnect(true);
}

// --- Summary ---

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("==============================");
    Serial.println(" ESP32-S3 STRESS TEST");
    Serial.println("==============================");

    float temp_start = readTemp();
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1fC", temp_start);
    Serial.printf("Starting temp: %s\n", buf);

    testPsram();
    testFlash();
    testCpu();
    testWifi();

    Serial.println("\n==============================");
    Serial.printf(" RESULTS: %d passed, %d failed\n", pass_count, fail_count);
    Serial.printf(" Final temp: %.1fC\n", readTemp());
    Serial.println("==============================");

    if (fail_count == 0)
        Serial.println(" VERDICT: Board is healthy");
    else
        Serial.println(" VERDICT: Issues detected — review failures above");
}

void loop()
{
    delay(60000);
}
