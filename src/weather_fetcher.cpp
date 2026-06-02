#include "weather_fetcher.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

namespace {
    float  lat_v = 0.0f;
    float  lon_v = 0.0f;
    String api_key_v;
    WeatherSnapshot snap = {};

    // Title-case each word, e.g. "scattered clouds" -> "Scattered Clouds".
    String title_case(const String& in) {
        String s = in;
        bool word_start = true;
        for (size_t i = 0; i < s.length(); ++i) {
            char c = s.charAt(i);
            if (c == ' ') {
                word_start = true;
            } else {
                s.setCharAt(i, word_start ? toupper(c) : tolower(c));
                word_start = false;
            }
        }
        return s;
    }

    bool fetch_one(WeatherSnapshot& out) {
        if (WiFi.status() != WL_CONNECTED) return false;

        WiFiClientSecure client;
        client.setInsecure();           // hobby device on LAN; no cert pinning
        client.setTimeout(HTTP_TIMEOUT_MS / 1000);

        char path[256];
        snprintf(path, sizeof(path), OWM_PATH_FMT, lat_v, lon_v, api_key_v.c_str());
        char url[320];
        snprintf(url, sizeof(url), "https://%s%s", OWM_HOST, path);

        HTTPClient http;
        http.setTimeout(HTTP_TIMEOUT_MS);
        if (!http.begin(client, url)) {
            return false;
        }
        http.addHeader("User-Agent", USER_AGENT);
        http.addHeader("Accept", "application/json");

        int code = http.GET();
        if (code != HTTP_CODE_OK) {
            log_w("HTTP %d from OWM", code);
            http.end();
            return false;
        }

        JsonDocument filter;
        filter["current"]["temp"]                     = true;
        filter["current"]["weather"][0]["description"] = true;
        filter["daily"][0]["temp"]["max"]             = true;
        filter["daily"][0]["temp"]["min"]             = true;
        filter["daily"][0]["pop"]                     = true;

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, http.getStream(),
                                                   DeserializationOption::Filter(filter));
        http.end();
        if (err) {
            log_w("JSON parse failed: %s", err.c_str());
            return false;
        }

        JsonVariant cur = doc["current"];
        JsonVariant day = doc["daily"][0];
        if (cur.isNull() || day.isNull() ||
            cur["temp"].isNull() || day["temp"]["max"].isNull() || day["temp"]["min"].isNull() ||
            cur["weather"][0]["description"].isNull()) {
            log_w("OWM response missing expected fields");
            return false;
        }

        out.temp_f     = cur["temp"].as<float>();
        out.high_f     = day["temp"]["max"].as<float>();
        out.low_f      = day["temp"]["min"].as<float>();
        out.pop        = day["pop"].isNull() ? 0.0f : day["pop"].as<float>();
        out.conditions = title_case(cur["weather"][0]["description"].as<String>());
        out.valid      = true;
        out.stale      = false;
        out.fetched_at = millis();
        return true;
    }
}

namespace weather_fetcher {

void begin(float lat, float lon, const String& api_key) {
    lat_v     = lat;
    lon_v     = lon;
    api_key_v = api_key;
    snap      = {};
}

bool refresh() {
    return fetch_one(snap);
}

const WeatherSnapshot& snapshot() { return snap; }

void update_staleness(uint32_t stale_threshold_ms) {
    if (snap.valid) {
        snap.stale = (millis() - snap.fetched_at) > stale_threshold_ms;
    }
}

} // namespace weather_fetcher
