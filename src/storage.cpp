#include "storage.h"
#include <Preferences.h>
#include <cmath>

namespace {
    Preferences prefs;
    constexpr const char* NS          = "weather";
    constexpr const char* KEY_LAT     = "lat";
    constexpr const char* KEY_LON     = "lon";
    constexpr const char* KEY_API_KEY = "apikey";
    constexpr const char* KEY_WIGLE   = "wigle";
    constexpr const char* KEY_UPMIN   = "upmin";
}

namespace storage {

void begin() {
    prefs.begin(NS, false);
}

bool load(WeatherConfig& out) {
    // NAN sentinel for "not set" since 0,0 is a valid (if unlikely) coordinate.
    out.lat         = prefs.getFloat(KEY_LAT, NAN);
    out.lon         = prefs.getFloat(KEY_LON, NAN);
    out.api_key     = prefs.getString(KEY_API_KEY, "");
    out.wigle_token = prefs.getString(KEY_WIGLE, "");
    out.update_min  = prefs.getUShort(KEY_UPMIN, 0);

    if (out.api_key.length() == 0 || out.update_min == 0) {
        return false;
    }
    // Config is usable if we have explicit coords OR a WiGLE token to resolve them.
    bool have_coords = !std::isnan(out.lat) && !std::isnan(out.lon);
    bool have_wigle  = out.wigle_token.length() > 0;
    return have_coords || have_wigle;
}

void save(float lat, float lon, const String& api_key,
          const String& wigle_token, uint16_t update_min) {
    if (update_min < MIN_UPDATE_MIN) update_min = MIN_UPDATE_MIN;
    if (update_min > MAX_UPDATE_MIN) update_min = MAX_UPDATE_MIN;
    prefs.putFloat(KEY_LAT, lat);   // NAN is preserved
    prefs.putFloat(KEY_LON, lon);
    prefs.putString(KEY_API_KEY, api_key);
    prefs.putString(KEY_WIGLE, wigle_token);
    prefs.putUShort(KEY_UPMIN, update_min);
}

void save_location(float lat, float lon) {
    prefs.putFloat(KEY_LAT, lat);
    prefs.putFloat(KEY_LON, lon);
}

void clear() {
    prefs.clear();
}

} // namespace storage
