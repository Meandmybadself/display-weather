#include "geolocate.h"
#include "config.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

namespace {
    bool query_one(const String& wigle_token, const String& bssid,
                   float& out_lat, float& out_lon) {
        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(HTTP_TIMEOUT_MS / 1000);

        char path[96];
        snprintf(path, sizeof(path), WIGLE_SEARCH_FMT, bssid.c_str());
        char url[160];
        snprintf(url, sizeof(url), "https://%s%s", WIGLE_HOST, path);

        HTTPClient http;
        http.setTimeout(HTTP_TIMEOUT_MS);
        if (!http.begin(client, url)) return false;

        String auth = String("Basic ") + wigle_token;
        http.addHeader("Authorization", auth);
        http.addHeader("Accept", "application/json");
        http.addHeader("User-Agent", USER_AGENT);

        int code = http.GET();
        if (code != HTTP_CODE_OK) {
            log_w("WiGLE HTTP %d for %s", code, bssid.c_str());
            http.end();
            return false;
        }

        JsonDocument filter;
        filter["success"]                = true;
        filter["results"][0]["trilat"]   = true;
        filter["results"][0]["trilong"]  = true;

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, http.getStream(),
                                                   DeserializationOption::Filter(filter));
        http.end();
        if (err) {
            log_w("WiGLE JSON parse: %s", err.c_str());
            return false;
        }
        if (!doc["success"].as<bool>()) return false;

        JsonVariant r0 = doc["results"][0];
        if (r0.isNull() || r0["trilat"].isNull() || r0["trilong"].isNull()) return false;

        out_lat = r0["trilat"].as<float>();
        out_lon = r0["trilong"].as<float>();
        return true;
    }

    // Selects the indices of the MAX_BSSID_QUERY strongest scan results into out[].
    // Returns how many were filled.
    int top_by_rssi(int total, int out[]) {
        int filled = 0;
        for (int i = 0; i < total; ++i) {
            if (filled < MAX_BSSID_QUERY) {
                out[filled++] = i;
            } else {
                int weakest = 0;
                for (int k = 1; k < filled; ++k) {
                    if (WiFi.RSSI(out[k]) < WiFi.RSSI(out[weakest])) weakest = k;
                }
                if (WiFi.RSSI(i) > WiFi.RSSI(out[weakest])) out[weakest] = i;
            }
        }
        return filled;
    }
}

namespace geolocate {

bool resolve(const String& wigle_token, float& out_lat, float& out_lon) {
    if (wigle_token.length() == 0) return false;
    if (WiFi.status() != WL_CONNECTED) return false;

    int n = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/false);
    if (n <= 0) {
        log_w("WiFi scan returned %d", n);
        return false;
    }

    int idx[MAX_BSSID_QUERY];
    int filled = top_by_rssi(n, idx);

    double sum_lat = 0.0;
    double sum_lon = 0.0;
    int    hits    = 0;
    for (int k = 0; k < filled; ++k) {
        float lat = 0.0f, lon = 0.0f;
        if (query_one(wigle_token, WiFi.BSSIDstr(idx[k]), lat, lon)) {
            sum_lat += lat;
            sum_lon += lon;
            hits++;
        }
        delay(100);  // pacing between WiGLE calls so we stay polite under rate limits
    }
    WiFi.scanDelete();

    if (hits == 0) return false;
    out_lat = (float)(sum_lat / hits);
    out_lon = (float)(sum_lon / hits);
    return true;
}

} // namespace geolocate
