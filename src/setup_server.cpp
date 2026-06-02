#include "setup_server.h"
#include "config.h"
#include "storage.h"
#include "display.h"

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <cmath>

namespace {
    WebServer server(80);

    // Stage-2 form. Placeholders (%LAT% etc.) are filled in serve_form() so the
    // user doesn't have to re-type values if validation kicks them back here.
    const char FORM_HTML[] = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WeatherDisplay Setup</title>
<style>
body{font-family:'Helvetica Neue',Helvetica,Arial,sans-serif;max-width:540px;margin:1.5em auto;padding:0 1em;color:#222;line-height:1.4}
h1{font-size:1.5em;margin-bottom:0.3em}
.lede{color:#555;margin-top:0;margin-bottom:1em}
.banner{background:#fff3cd;border:1px solid #ffeaa7;padding:0.8em 1em;border-radius:4px;margin:1em 0;font-size:0.92em}
.banner a{color:#0066cc}
label{display:block;margin-top:1.1em;font-weight:600}
input{width:100%;padding:0.55em;font-size:1em;border:1px solid #ccc;border-radius:4px;font-family:inherit;box-sizing:border-box}
input:focus{outline:none;border-color:#0066cc;box-shadow:0 0 0 2px rgba(0,102,204,0.15)}
.hint{font-size:0.85em;color:#666;margin-top:0.3em}
.hint a{color:#0066cc}
button{margin-top:1.6em;padding:0.7em 1.6em;background:#0066cc;color:white;border:none;border-radius:4px;font-size:1em;font-family:inherit;cursor:pointer}
button:hover{background:#0055aa}
.row{display:flex;gap:0.6em}
.row>div{flex:1}
.opt{font-weight:normal;color:#666;font-size:0.9em}
</style>
</head>
<body>
<h1>WeatherDisplay setup</h1>
<p class="lede">Step 2 of 2. You're on your home wifi now, so external links work.</p>
<div class="banner">
<strong>You'll need:</strong>
a free <a href="https://openweathermap.org/api" target="_blank" rel="noopener">OpenWeatherMap API key</a>.
Optionally, a <a href="https://wigle.net/account" target="_blank" rel="noopener">WiGLE API token</a>
auto-detects your location instead of typing coordinates.
</div>
<form method="post" action="/save">
<label>OpenWeatherMap API key</label>
<input name="api_key" value="%APIKEY%" autofocus required autocomplete="off">
<div class="hint">Free tier is plenty. Sign up at <a href="https://openweathermap.org/api" target="_blank" rel="noopener">openweathermap.org/api</a> &mdash; activation can take an hour.</div>

<div class="row">
<div>
<label>Latitude <span class="opt">(optional)</span></label>
<input name="lat" value="%LAT%" placeholder="44.9778" autocomplete="off">
</div>
<div>
<label>Longitude <span class="opt">(optional)</span></label>
<input name="lon" value="%LON%" placeholder="-93.2650" autocomplete="off">
</div>
</div>
<div class="hint">Optional if you provide a WiGLE token below.</div>

<label>WiGLE API token <span class="opt">(optional)</span></label>
<input name="wigle" value="%WIGLE%" autocomplete="off">
<div class="hint">"Encoded for use" string from <a href="https://wigle.net/account" target="_blank" rel="noopener">wigle.net/account</a>. If lat/lon are blank, this is used to derive coordinates from nearby wifi networks.</div>

<label>Update every N minutes</label>
<input name="upmin" type="number" min="5" max="240" value="%UPMIN%">

<button type="submit">Save</button>
</form>
</body>
</html>)HTML";

    const char SAVED_HTML[] = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"><title>WeatherDisplay</title>
<style>body{font-family:'Helvetica Neue',Helvetica,Arial,sans-serif;max-width:520px;margin:2.5em auto;padding:0 1em;color:#222;line-height:1.4}</style>
</head><body>
<h1>Saved.</h1>
<p>The device is rebooting and will start displaying weather in a moment.</p>
</body></html>)HTML";

    const char ERROR_HTML[] = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"><title>WeatherDisplay</title>
<style>body{font-family:'Helvetica Neue',Helvetica,Arial,sans-serif;max-width:520px;margin:2.5em auto;padding:0 1em;color:#222;line-height:1.4}a{color:#0066cc}</style>
</head><body>
<h1>Missing required fields</h1>
<p>You need an OpenWeatherMap API key, plus either explicit latitude AND longitude, or a WiGLE token. Coordinates must be in valid ranges.</p>
<p><a href="/setup">Back to form</a></p>
</body></html>)HTML";

    void serve_form() {
        WeatherConfig cfg;
        storage::load(cfg);   // partial config is fine here — we just want to prefill

        String html = FORM_HTML;
        html.replace("%LAT%",    std::isnan(cfg.lat) ? String("") : String(cfg.lat, 6));
        html.replace("%LON%",    std::isnan(cfg.lon) ? String("") : String(cfg.lon, 6));
        html.replace("%APIKEY%", cfg.api_key);
        html.replace("%WIGLE%",  cfg.wigle_token);
        html.replace("%UPMIN%",  String(cfg.update_min ? cfg.update_min : (uint16_t)String(DEFAULT_UPDATE_MIN).toInt()));

        server.send(200, "text/html", html);
    }

    void handle_save() {
        String api_key = server.arg("api_key");
        String lat_s   = server.arg("lat");
        String lon_s   = server.arg("lon");
        String wigle   = server.arg("wigle");
        String upmin_s = server.arg("upmin");
        api_key.trim();
        lat_s.trim();
        lon_s.trim();
        wigle.trim();
        upmin_s.trim();

        bool  have_coords = lat_s.length() > 0 && lon_s.length() > 0;
        float lat = have_coords ? lat_s.toFloat() : NAN;
        float lon = have_coords ? lon_s.toFloat() : NAN;

        uint16_t upmin = (uint16_t)upmin_s.toInt();
        if (upmin == 0) upmin = (uint16_t)String(DEFAULT_UPDATE_MIN).toInt();

        bool ok = api_key.length() > 0 && (have_coords || wigle.length() > 0);
        if (ok && have_coords) {
            ok = lat >= -90.0f  && lat <= 90.0f
              && lon >= -180.0f && lon <= 180.0f;
        }

        if (!ok) {
            server.send(400, "text/html", ERROR_HTML);
            return;
        }

        storage::save(lat, lon, api_key, wigle, upmin);
        server.send(200, "text/html", SAVED_HTML);
        delay(1500);
        ESP.restart();
    }

    void redirect_to_setup() {
        server.sendHeader("Location", "/setup");
        server.send(302, "text/plain", "");
    }
}

namespace setup_server {

[[noreturn]] void run() {
    MDNS.begin(MDNS_HOSTNAME);
    MDNS.addService("http", "tcp", 80);

    server.on("/",       HTTP_GET,  redirect_to_setup);
    server.on("/setup",  HTTP_GET,  serve_form);
    server.on("/save",   HTTP_POST, handle_save);
    server.onNotFound(redirect_to_setup);
    server.begin();

    display::show_setup_url(WiFi.localIP(), MDNS_HOSTNAME);

    while (true) {
        server.handleClient();
        delay(10);
    }
}

} // namespace setup_server
