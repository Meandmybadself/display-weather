#include <Arduino.h>
#include <WiFi.h>
#include <cmath>

#include "config.h"
#include "storage.h"
#include "buttons.h"
#include "display.h"
#include "wifi_setup.h"
#include "weather_fetcher.h"
#include "geolocate.h"
#include "setup_server.h"

namespace {
    WeatherConfig cfg;

    uint32_t refresh_interval_ms = 0;
    uint32_t stale_threshold_ms  = 0;
    uint32_t last_refresh_ms     = 0;
    uint32_t last_indicator_ms   = 0;
    uint32_t last_reconnect_ms   = 0;

    bool refresh_pending = true;     // run a refresh immediately at boot
    bool have_data       = false;
    bool last_fetch_ok   = false;
    bool last_wifi_ok    = false;

    void redraw() {
        if (have_data) display::draw_weather(weather_fetcher::snapshot());
        else           display::draw_no_data();
        display::draw_wifi_indicator(WiFi.status() == WL_CONNECTED);
    }

    // Returns true if the user held D0 long enough to trigger a factory reset.
    bool check_boot_reset_trigger() {
        if (digitalRead(PIN_BTN_D0) != LOW) return false;
        for (uint8_t s = (BOOT_HOLD_MS / 1000); s >= 1; --s) {
            display::show_factory_reset_countdown(s);
            uint32_t t0 = millis();
            while (millis() - t0 < 1000) {
                if (digitalRead(PIN_BTN_D0) != LOW) return false;
                delay(20);
            }
        }
        display::show_boot_message("Resetting...", "Releasing setup mode");
        wifi_setup::factory_reset();
        delay(500);
        return true;
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);

    buttons::begin();
    display::begin();
    storage::begin();

    display::show_boot_message("WeatherDisplay", "booting...");
    delay(300);

    check_boot_reset_trigger();

    bool cfg_ok = storage::load(cfg);
    Serial.printf("[main] storage::load=%d  api_key.len=%u  lat=%f lon=%f  wigle.len=%u  upmin=%u\n",
                  (int)cfg_ok, (unsigned)cfg.api_key.length(),
                  cfg.lat, cfg.lon, (unsigned)cfg.wigle_token.length(),
                  (unsigned)cfg.update_min);

    // Stage 1: no wifi credentials → captive portal.
    if (!wifi_setup::has_saved_credentials()) {
        Serial.println("[main] no saved wifi creds -> Stage 1 portal");
        wifi_setup::run_portal();  // does not return
    }

    // We have saved credentials. Try hard to connect before doing anything drastic.
    display::show_centered_message("Connecting wifi");
    bool wifi_ok = wifi_setup::connect(WIFI_CONNECT_ATTEMPT_MS, WIFI_CONNECT_ATTEMPTS);

    bool have_coords = !std::isnan(cfg.lat) && !std::isnan(cfg.lon);
    bool config_complete = cfg.api_key.length() > 0
                           && (have_coords || cfg.wigle_token.length() > 0);
    Serial.printf("[main] wifi_ok=%d config_complete=%d have_coords=%d\n",
                  (int)wifi_ok, (int)config_complete, (int)have_coords);

    if (!wifi_ok) {
        // No usable config yet → the user still has to onboard; the portal is the
        // only way forward.
        if (!config_complete) {
            display::show_boot_message("Wifi failed", "Entering setup");
            delay(1500);
            wifi_setup::run_portal();  // does not return
        }
        // Config is complete but coordinates still need a WiGLE lookup, which needs
        // wifi — nothing to display, so reboot and keep retrying (self-healing)
        // instead of dead-ending in the portal over a transient outage.
        if (!have_coords) {
            display::show_boot_message("Wifi unavailable", "retrying...");
            delay(3000);
            ESP.restart();
        }
        // Complete config AND known coordinates: a wifi outage here is almost
        // certainly transient. Boot into the normal loop showing "no data" and let
        // loop() keep reconnecting — never force re-onboarding for a dropout.
        Serial.println("[main] wifi down but config complete -> run, retry in loop()");
    }

    // Stage 2: wifi up but weather config incomplete → local setup server.
    if (wifi_ok && !config_complete) {
        Serial.println("[main] -> Stage 2 setup server");
        setup_server::run();  // does not return
    }

    refresh_interval_ms = (uint32_t)cfg.update_min * 60UL * 1000UL;
    stale_threshold_ms  = refresh_interval_ms * 2;

    // Resolve lat/lon via WiGLE if the user didn't enter coords directly.
    if (!have_coords) {
        display::show_boot_message("Locating...", "scanning nearby wifi");
        float lat = 0.0f, lon = 0.0f;
        if (geolocate::resolve(cfg.wigle_token, lat, lon)) {
            cfg.lat = lat;
            cfg.lon = lon;
            storage::save_location(lat, lon);
        } else {
            display::show_boot_message("Locate failed", "Re-enter setup");
            delay(2000);
            setup_server::run();  // does not return
        }
    }

    display::show_boot_message("Loading weather", nullptr);
    weather_fetcher::begin(cfg.lat, cfg.lon, cfg.api_key);

    display::draw_no_data();
    last_refresh_ms = millis();
}

void loop() {
    uint32_t now = millis();

    // Schedule a refresh. Until a fetch succeeds (and again after any failure) retry
    // on the short interval, so a boot-time or temporary outage doesn't leave us
    // blank/stale for up to update_min.
    uint32_t due = last_fetch_ok ? refresh_interval_ms : WEATHER_RETRY_MS;
    if (!refresh_pending && now - last_refresh_ms >= due) {
        refresh_pending = true;
    }

    // Run the refresh (one HTTP request, bounded by HTTP_TIMEOUT_MS < loopTask WDT).
    if (refresh_pending) {
        bool ok = weather_fetcher::refresh();
        last_refresh_ms = now;
        refresh_pending = false;
        last_fetch_ok   = ok;
        if (ok) have_data = true;
        if (have_data) redraw();   // fresh success, or keep last data with stale marker
    }

    weather_fetcher::update_staleness(stale_threshold_ms);

    // Buttons. D0 forces a refresh; D1/D2 are unused.
    ButtonEvent ev = buttons::poll();
    if (ev == BTN_D0_PRESSED) {
        refresh_pending = true;
    }

    // Keep wifi up: when disconnected, re-issue the join periodically (the ESP32's
    // own auto-reconnect can stall after a longer outage).
    bool wifi_ok = (WiFi.status() == WL_CONNECTED);
    if (!wifi_ok && now - last_reconnect_ms >= WIFI_RECONNECT_MS) {
        Serial.println("[wifi] disconnected -> re-issuing join");
        WiFi.disconnect(/*wifioff=*/false, /*eraseap=*/false);
        WiFi.begin();
        last_reconnect_ms = now;
    }

    // Wifi indicator tick (1Hz). Only repaint on state change so we don't flicker.
    if (now - last_indicator_ms >= 1000) {
        if (wifi_ok != last_wifi_ok) {
            display::draw_wifi_indicator(wifi_ok);
            last_wifi_ok = wifi_ok;
        }
        last_indicator_ms = now;
    }

    delay(10);
}
