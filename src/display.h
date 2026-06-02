#pragma once

#include <Arduino.h>

struct WeatherSnapshot;  // fwd

namespace display {
    void begin();

    // Boot/portal screens
    void show_boot_message(const char* line1, const char* line2 = nullptr);
    // A single line centered on both axes.
    void show_centered_message(const char* text);
    void show_portal_instructions(const char* ssid, const IPAddress& ip);
    void show_factory_reset_countdown(uint8_t seconds_remaining);

    // Main display
    void draw_weather(const WeatherSnapshot& s);
    void draw_no_data();
    void draw_wifi_indicator(bool wifi_ok);

    // Stage-2 onboarding screen — shown after wifi connects but before config is complete.
    void show_setup_url(const IPAddress& ip, const char* mdns_host);
}
