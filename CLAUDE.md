# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ESP32-S2 Arduino firmware (PlatformIO) for an [Adafruit Feather ESP32-S2 Reverse TFT](https://www.adafruit.com/product/5345). It shows current weather (temp, conditions, today's high/low, chance of precip) from OpenWeatherMap on the 240×135 ST7789 TFT. The git history shows this was forked from a stock-ticker; the `stock_fetcher.*` files are deleted and the project is mid-conversion to weather.

## Commands

```sh
pio run                 # build only (compile check — use this to verify changes)
scripts/flash.sh        # wait for /dev/cu.usbmodem*, then build + upload
scripts/monitor.sh      # wait for the port, then open serial monitor @115200
```

There are no host-side unit tests; "testing" means flashing to hardware and watching the serial monitor (every module logs liberally over `Serial`). `pio run` is the only feedback loop available without a board attached — always run it after edits.

**Fonts must be generated before the first build.** The UI uses Helvetica Neue as Adafruit-GFX bitmap fonts, but `HelveticaNeue.ttc` and the generated `src/fonts/HelveticaNeue*pt7b.h` headers are gitignored (proprietary font). A fresh clone won't compile until you run `scripts/gen_fonts.sh` (needs `HelveticaNeue.ttc` in the repo root, `brew install freetype`, `pip3 install fonttools`, and `.pio/libdeps` populated by one `pio run`). Only `src/fonts/helvetica_neue.h` (the aggregator) is tracked. See the README "Fonts" section. To restyle, edit the `fontconvert` calls in `gen_fonts.sh` and re-run.

**ESP32-S2 flashing gotcha:** native USB makes the auto-bootloader-reset unreliable. If flash fails with "No serial data received", manually enter ROM bootloader: hold **BOOT** (D0), tap **RESET**, release BOOT — the port name changes, then re-run `scripts/flash.sh`. Tap **RESET** once when it finishes.

## Architecture

`main.cpp` is a linear boot state machine in `setup()`, then a polled `loop()`. The boot flow is a **two-stage setup**, which is the central design idea:

- **Stage 1 — wifi (`wifi_setup` + WiFiManager):** if no saved wifi credentials, `run_portal()` raises the `WeatherDisplay-Setup` AP and captive portal for wifi *only*, then reboots. Credentials live in WiFiManager's own NVS, not ours.
- **Stage 2 — weather config (`setup_server`):** once wifi is up but config is incomplete, a local `WebServer` (also reachable at `weatherdisplay.local` via mDNS) serves a form for API key, optional lat/lon, optional WiGLE token, and refresh interval. Saving writes NVS and reboots.

The split exists because external links (OpenWeatherMap signup) don't work while joined to the device's own AP — Stage 2 only runs after the device is on real wifi.

Each setup function is `[[noreturn]]` and ends in `ESP.restart()`; the machine re-evaluates from the top on every boot rather than transitioning in place. Config completeness is judged the same way in three places (`main.cpp`, `storage::load`, `setup_server::handle_save`): **api_key present AND (explicit lat/lon OR a WiGLE token)** — keep those checks in sync if you change the rule.

**Config persistence (`storage`):** NVS via `Preferences` under namespace `"weather"`. `lat`/`lon` use `NAN` as the "unset" sentinel (0,0 is a real coordinate), so test coordinate-presence with `std::isnan`, never `== 0`. `WeatherConfig` is the shared struct.

**Geolocation (`geolocate`):** if the user gave a WiGLE token instead of coords, `resolve()` scans nearby BSSIDs, queries the strongest `MAX_BSSID_QUERY` against WiGLE, and averages the hits into a lat/lon that's then persisted via `storage::save_location`.

**Weather fetch (`weather_fetcher`):** `refresh()` does one HTTPS GET to OWM `/data/3.0/onecall` and parses it with ArduinoJson using a **filter document** (only the handful of fields we render are deserialized — memory-constrained). Holds a single `WeatherSnapshot` retrieved via `snapshot()`. `update_staleness()` flags data as stale past 2× the refresh interval.

**Display (`display`)** wraps the ST7789; **`buttons`** gives debounced edge events. `main` owns all timing and only repaints on change (e.g. the wifi dot) to avoid flicker.

## Constraints to respect

- **`fetch_one` runs from `loop()`**, so HTTP must finish before the loopTask watchdog (~5s) bites. `HTTP_TIMEOUT_MS` (4000) in `config.h` is deliberately under that — don't raise it past the WDT.
- **TLS is `setInsecure()`** everywhere (OWM and WiGLE) — no cert validation, intentional for a personal LAN device.
- All tunables (pins, colors, hosts, URL formats, bounds, timeouts) live in `config.h`. Add new constants there rather than inlining literals.
- OWM free tier allows ~1000 One Call requests/day; refresh interval is bounded 5–240 min (default 30) and clamped in `storage::save`.

## Working-tree note

`README.md` and most `src/` files show as modified and the `weather_fetcher`/`geolocate`/`setup_server` files are untracked — the conversion from the stock-ticker firmware is uncommitted. Expect to be committing this transition.
