#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <lvgl.h>
#include "secret.h"

#define TFT_CS    4
#define TFT_DC    5
#define TFT_RST   6
#define TFT_MOSI  7
#define TFT_SCLK 15
#define TFT_MISO 16

#define SCREEN_W   320
#define SCREEN_H   240
#define BUF_ROWS   20
#define NUM_SCREENS 3
#define CYCLE_MS   5000
#define REFRESH_MS 60000

static const char* API_URL =
    "https://api.open-meteo.com/v1/forecast"
    "?latitude=18.52&longitude=73.86"
    "&current=temperature_2m,apparent_temperature,"
    "relative_humidity_2m,weather_code,"
    "wind_speed_10m,wind_direction_10m,"
    "cloud_cover,pressure_msl,surface_pressure,"
    "precipitation,rain,snowfall"
    "&timezone=auto";

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf1;
static lv_color_t* buf2;

static lv_obj_t* screens[NUM_SCREENS];
static int cur_screen = 0;

static lv_obj_t* lbl_temp;
static lv_obj_t* lbl_feels;
static lv_obj_t* lbl_condition;
static lv_obj_t* lbl_humidity;
static lv_obj_t* lbl_wind_speed;
static lv_obj_t* lbl_wind_dir;
static lv_obj_t* lbl_pressure_sfc;
static lv_obj_t* lbl_pressure_msl;
static lv_obj_t* lbl_cloud;
static lv_obj_t* lbl_precip;
static lv_obj_t* lbl_rain;
static lv_obj_t* lbl_snow;

static unsigned long last_fetch = 0;

static const char* wmoDescription(int code)
{
    switch (code)
    {
        case 0:  return "Clear sky";
        case 1:  return "Mainly clear";
        case 2:  return "Partly cloudy";
        case 3:  return "Overcast";
        case 45: return "Fog";
        case 48: return "Depositing rime fog";
        case 51: return "Light drizzle";
        case 53: return "Moderate drizzle";
        case 55: return "Dense drizzle";
        case 56: return "Light freezing drizzle";
        case 57: return "Dense freezing drizzle";
        case 61: return "Slight rain";
        case 63: return "Moderate rain";
        case 65: return "Heavy rain";
        case 66: return "Light freezing rain";
        case 67: return "Heavy freezing rain";
        case 71: return "Slight snowfall";
        case 73: return "Moderate snowfall";
        case 75: return "Heavy snowfall";
        case 77: return "Snow grains";
        case 80: return "Slight rain showers";
        case 81: return "Moderate rain showers";
        case 82: return "Violent rain showers";
        case 85: return "Slight snow showers";
        case 86: return "Heavy snow showers";
        case 95: return "Thunderstorm";
        case 96: return "Thunderstorm, slight hail";
        case 99: return "Thunderstorm, heavy hail";
        default: return "Unknown";
    }
}

static const char* windCompass(float deg)
{
    if (deg < 22.5 || deg >= 337.5)  return "N";
    if (deg < 67.5)   return "NE";
    if (deg < 112.5)  return "E";
    if (deg < 157.5)  return "SE";
    if (deg < 202.5)  return "S";
    if (deg < 247.5)  return "SW";
    if (deg < 292.5)  return "W";
    return "NW";
}

static void flushDisplay(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.writePixels((uint16_t*)color_p, w * h);
    tft.endWrite();
    lv_disp_flush_ready(drv);
}

static void addDots(lv_obj_t* scr, int active)
{
    lv_obj_t* row = lv_obj_create(scr);
    lv_obj_set_size(row, SCREEN_W, 16);
    lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_column(row, 8, 0);

    for (int i = 0; i < NUM_SCREENS; i++)
    {
        lv_obj_t* dot = lv_obj_create(row);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        if (i == active)
            lv_obj_set_style_bg_color(dot, lv_color_white(), 0);
        else
            lv_obj_set_style_bg_color(dot, lv_palette_darken(LV_PALETTE_GREY, 2), 0);
    }
}

static lv_obj_t* addTitle(lv_obj_t* scr, const char* text)
{
    lv_obj_t* lbl = lv_label_create(scr);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lv_palette_main(LV_PALETTE_LIGHT_BLUE), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 8);
    return lbl;
}

static lv_obj_t* addField(lv_obj_t* parent, const char* label, int y, const lv_font_t* font)
{
    lv_obj_t* key = lv_label_create(parent);
    lv_label_set_text(key, label);
    lv_obj_set_style_text_color(key, lv_palette_main(LV_PALETTE_LIGHT_BLUE), 0);
    lv_obj_set_style_text_font(key, &lv_font_montserrat_14, 0);
    lv_obj_align(key, LV_ALIGN_TOP_LEFT, 12, y);

    lv_obj_t* val = lv_label_create(parent);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_color(val, lv_color_white(), 0);
    lv_obj_set_style_text_font(val, font, 0);
    lv_obj_align(val, LV_ALIGN_TOP_LEFT, 140, y);

    return val;
}

static void buildScreens()
{
    screens[0] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[0], lv_color_black(), 0);
    addTitle(screens[0], "Pune, India");

    lbl_temp = lv_label_create(screens[0]);
    lv_label_set_text(lbl_temp, "--.-");
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_temp, lv_color_white(), 0);
    lv_obj_align(lbl_temp, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t* unit = lv_label_create(screens[0]);
    lv_label_set_text(unit, LV_SYMBOL_DUMMY "C");
    lv_obj_set_style_text_font(unit, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(unit, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align_to(unit, lbl_temp, LV_ALIGN_OUT_RIGHT_TOP, 4, 0);

    lbl_feels = lv_label_create(screens[0]);
    lv_label_set_text(lbl_feels, "Feels like --.-");
    lv_obj_set_style_text_font(lbl_feels, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_feels, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(lbl_feels, LV_ALIGN_TOP_MID, 0, 80);

    lbl_condition = lv_label_create(screens[0]);
    lv_label_set_text(lbl_condition, "--");
    lv_obj_set_style_text_font(lbl_condition, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_condition, lv_palette_main(LV_PALETTE_AMBER), 0);
    lv_obj_align(lbl_condition, LV_ALIGN_TOP_MID, 0, 120);

    addDots(screens[0], 0);

    screens[1] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[1], lv_color_black(), 0);
    addTitle(screens[1], "Wind & Atmosphere");

    lbl_humidity     = addField(screens[1], "Humidity",  35, &lv_font_montserrat_20);
    lbl_wind_speed   = addField(screens[1], "Wind",      70, &lv_font_montserrat_20);
    lbl_wind_dir     = addField(screens[1], "Direction", 105, &lv_font_montserrat_20);
    lbl_pressure_sfc = addField(screens[1], "Sfc Press", 140, &lv_font_montserrat_20);
    lbl_pressure_msl = addField(screens[1], "MSL Press", 175, &lv_font_montserrat_20);

    addDots(screens[1], 1);

    screens[2] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[2], lv_color_black(), 0);
    addTitle(screens[2], "Precipitation");

    lbl_cloud  = addField(screens[2], "Cloud",   50, &lv_font_montserrat_20);
    lbl_precip = addField(screens[2], "Precip",  90, &lv_font_montserrat_20);
    lbl_rain   = addField(screens[2], "Rain",   130, &lv_font_montserrat_20);
    lbl_snow   = addField(screens[2], "Snow",   170, &lv_font_montserrat_20);

    addDots(screens[2], 2);
}

static void cycleScreen(lv_timer_t* timer)
{
    cur_screen = (cur_screen + 1) % NUM_SCREENS;
    lv_scr_load_anim(screens[cur_screen], LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
}

static void fetchWeather()
{
    HTTPClient http;
    http.begin(API_URL);
    int code = http.GET();

    if (code != 200)
    {
        Serial.printf("HTTP error: %d\n", code);
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err)
    {
        Serial.printf("JSON error: %s\n", err.c_str());
        return;
    }

    JsonObject cur = doc["current"];
    char buf[32];

    float temp = cur["temperature_2m"];
    snprintf(buf, sizeof(buf), "%.1f", temp);
    lv_label_set_text(lbl_temp, buf);

    float feels = cur["apparent_temperature"];
    snprintf(buf, sizeof(buf), "Feels like %.1f C", feels);
    lv_label_set_text(lbl_feels, buf);

    int wmo = cur["weather_code"];
    lv_label_set_text(lbl_condition, wmoDescription(wmo));

    int humidity = cur["relative_humidity_2m"];
    snprintf(buf, sizeof(buf), "%d%%", humidity);
    lv_label_set_text(lbl_humidity, buf);

    float wspeed = cur["wind_speed_10m"];
    snprintf(buf, sizeof(buf), "%.1f km/h", wspeed);
    lv_label_set_text(lbl_wind_speed, buf);

    float wdir = cur["wind_direction_10m"];
    snprintf(buf, sizeof(buf), "%s (%.0f)", windCompass(wdir), wdir);
    lv_label_set_text(lbl_wind_dir, buf);

    float psfc = cur["surface_pressure"];
    snprintf(buf, sizeof(buf), "%.0f hPa", psfc);
    lv_label_set_text(lbl_pressure_sfc, buf);

    float pmsl = cur["pressure_msl"];
    snprintf(buf, sizeof(buf), "%.0f hPa", pmsl);
    lv_label_set_text(lbl_pressure_msl, buf);

    int cloud = cur["cloud_cover"];
    snprintf(buf, sizeof(buf), "%d%%", cloud);
    lv_label_set_text(lbl_cloud, buf);

    float precip = cur["precipitation"];
    snprintf(buf, sizeof(buf), "%.1f mm", precip);
    lv_label_set_text(lbl_precip, buf);

    float rain = cur["rain"];
    snprintf(buf, sizeof(buf), "%.1f mm", rain);
    lv_label_set_text(lbl_rain, buf);

    float snow = cur["snowfall"];
    snprintf(buf, sizeof(buf), "%.1f cm", snow);
    lv_label_set_text(lbl_snow, buf);

    Serial.printf("Weather: %.1fC, %s\n", temp, wmoDescription(wmo));
}

void setup()
{
    Serial.begin(115200);

    SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI);
    tft.init(240, 320);
    tft.invertDisplay(false);
    tft.setRotation(3);
    tft.fillScreen(ST77XX_BLACK);

    lv_init();

    size_t buf_size = SCREEN_W * BUF_ROWS * sizeof(lv_color_t);
    buf1 = (lv_color_t*)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t*)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    if (!buf1 || !buf2)
    {
        buf1 = (lv_color_t*)malloc(buf_size);
        buf2 = (lv_color_t*)malloc(buf_size);
    }
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, SCREEN_W * BUF_ROWS);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_W;
    disp_drv.ver_res = SCREEN_H;
    disp_drv.flush_cb = flushDisplay;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    buildScreens();
    lv_scr_load(screens[0]);
    lv_timer_create(cycleScreen, CYCLE_MS, NULL);

    Serial.print("Connecting to WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        lv_timer_handler();
    }
    Serial.printf("\nConnected: %s\n", WiFi.localIP().toString().c_str());

    fetchWeather();
    last_fetch = millis();
}

void loop()
{
    lv_timer_handler();
    delay(5);

    if (millis() - last_fetch >= REFRESH_MS)
    {
        fetchWeather();
        last_fetch = millis();
    }
}
