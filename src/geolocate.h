#pragma once

#include <Arduino.h>

namespace geolocate {
    // Scans nearby networks, queries WiGLE for the top-N strongest BSSIDs, averages
    // the returned trilateration coordinates. Returns true on success.
    // Blocks for up to ~scan time + MAX_BSSID_QUERY * HTTP_TIMEOUT_MS — only safe to
    // call from setup() or other contexts that aren't under the loopTask WDT.
    // wigle_token is the "Encoded for use" string from the WiGLE API token page.
    bool resolve(const String& wigle_token, float& out_lat, float& out_lon);
}
