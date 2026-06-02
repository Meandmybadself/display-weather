#pragma once

#include <Arduino.h>
#include "config.h"

struct WeatherConfig {
    float    lat;          // NAN if unset (will be resolved via WiGLE on connect)
    float    lon;          // NAN if unset
    String   api_key;
    String   wigle_token;  // optional; empty when lat/lon were entered manually
    uint16_t update_min;
};

namespace storage {
    void begin();
    bool load(WeatherConfig& out);
    void save(float lat, float lon, const String& api_key,
              const String& wigle_token, uint16_t update_min);
    // Used after a successful WiGLE lookup to persist the resolved coordinates.
    void save_location(float lat, float lon);
    void clear();
}
