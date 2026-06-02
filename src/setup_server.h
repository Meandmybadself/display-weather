#pragma once

#include <Arduino.h>

namespace setup_server {
    // Starts mDNS + an HTTP server on the local wifi and serves the Stage-2 form
    // (lat/lon, OWM API key, optional WiGLE token, update interval). On successful
    // save it persists to NVS and reboots. Does not return.
    [[noreturn]] void run();
}
