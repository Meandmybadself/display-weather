#pragma once

#include <Arduino.h>
#include "config.h"

struct WeatherSnapshot {
    float    temp_f;
    String   conditions;     // e.g., "Partly cloudy"
    float    high_f;         // today's high
    float    low_f;          // today's low
    float    pop;            // 0.0 - 1.0 chance of precipitation today
    bool     valid;          // true once we've successfully fetched at least once
    bool     stale;          // true if last successful fetch is older than threshold
    uint32_t fetched_at;     // millis() of last successful fetch
};

namespace weather_fetcher {
    void begin(float lat, float lon, const String& api_key);

    // Fires one HTTP request and updates the snapshot. Returns true on success.
    // Bounded by HTTP_TIMEOUT_MS so it can safely run inside loop().
    bool refresh();

    const WeatherSnapshot& snapshot();

    void update_staleness(uint32_t stale_threshold_ms);
}
