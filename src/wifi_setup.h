#pragma once

#include <Arduino.h>

namespace wifi_setup {
    // Blocking: brings up the captive-portal AP for wifi credentials only, then
    // reboots so the main flow picks up the connection. Does not return.
    [[noreturn]] void run_portal();

    // Wipes wifi credentials AND all stored weather config.
    void factory_reset();

    // Tries to connect to the saved network. Makes up to `attempts` association
    // attempts of `per_attempt_ms` each, re-issuing the join between tries (which
    // recovers from a stalled attempt on a marginal link). Returns true once connected.
    bool connect(uint32_t per_attempt_ms, uint8_t attempts);

    // True if ESP32 NVS already has wifi credentials we can use.
    bool has_saved_credentials();
}
