#pragma once

#include <Arduino.h>
#include <Adafruit_ST77xx.h>

// --- Board pins (Adafruit Feather ESP32-S2 Reverse TFT) ---
constexpr uint8_t PIN_TFT_BACKLIGHT  = TFT_BACKLITE;
constexpr uint8_t PIN_TFT_I2C_POWER  = TFT_I2C_POWER;
constexpr uint8_t PIN_BTN_D0         = 0;  // active LOW (BOOT button, internal pull-up)
constexpr uint8_t PIN_BTN_D1         = 1;  // active HIGH
constexpr uint8_t PIN_BTN_D2         = 2;  // active HIGH

// --- Display geometry (240x135 landscape rotation 3) ---
constexpr uint16_t TFT_WIDTH         = 240;
constexpr uint16_t TFT_HEIGHT        = 135;
constexpr uint8_t  TFT_ROTATION      = 3;

// --- Captive portal ---
constexpr const char* PORTAL_AP_SSID = "WeatherDisplay-Setup";
constexpr uint16_t PORTAL_TIMEOUT_S  = 600;  // 10 min then reboot to retry

// --- Defaults the portal pre-fills ---
constexpr const char* DEFAULT_LAT         = "";
constexpr const char* DEFAULT_LON         = "";
constexpr const char* DEFAULT_API_KEY     = "";
constexpr const char* DEFAULT_WIGLE_TOKEN = "";
constexpr const char* DEFAULT_UPDATE_MIN  = "30";

// --- Bounds ---
constexpr uint16_t MIN_UPDATE_MIN = 5;
constexpr uint16_t MAX_UPDATE_MIN = 240;

// --- Network / data ---
// HTTP timeout MUST stay below the loopTask WDT (~5s) since fetch is called from loop().
constexpr uint32_t HTTP_TIMEOUT_MS = 4000;
constexpr const char* OWM_HOST     = "api.openweathermap.org";
constexpr const char* OWM_PATH_FMT =
    "/data/3.0/onecall?lat=%.6f&lon=%.6f&units=imperial&exclude=minutely,hourly&appid=%s";
constexpr const char* USER_AGENT   = "Mozilla/5.0 (WeatherDisplayESP32)";

// --- WiGLE (optional auto-geolocation from nearby BSSIDs) ---
constexpr const char* WIGLE_HOST       = "api.wigle.net";
constexpr const char* WIGLE_SEARCH_FMT = "/api/v2/network/search?netid=%s";
constexpr uint8_t     MAX_BSSID_QUERY  = 5;   // top-N strongest nearby networks to query

// --- Stage-2 setup server ---
constexpr const char* MDNS_HOSTNAME = "weatherdisplay";   // → weatherdisplay.local

// --- Reset-trigger detection ---
constexpr uint32_t BOOT_HOLD_MS = 2000;

// --- Connection robustness ---
// A wifi failure when we already have valid credentials + config is treated as
// transient (weak signal, AP rebooting): we retry rather than forcing re-onboarding.
constexpr uint32_t WIFI_CONNECT_ATTEMPT_MS = 8000;   // per association attempt at boot
constexpr uint8_t  WIFI_CONNECT_ATTEMPTS   = 4;      // boot attempts before falling back
constexpr uint32_t WIFI_RECONNECT_MS       = 15000;  // re-issue join while disconnected in loop()
constexpr uint32_t WEATHER_RETRY_MS        = 30000;  // refetch sooner until a fetch succeeds

// --- Colors (RGB565) ---
constexpr uint16_t COLOR_BG     = 0x0000;   // black
constexpr uint16_t COLOR_FG     = 0xFFFF;   // white
constexpr uint16_t COLOR_DIM    = 0x7BEF;   // gray
constexpr uint16_t COLOR_ACCENT = 0x07FF;   // cyan
constexpr uint16_t COLOR_WARN   = 0xFD20;   // amber
