#include "display.h"
#include "config.h"
#include "weather_fetcher.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <IPAddress.h>
#include <math.h>

#include "fonts/helvetica_neue.h"

namespace {
    Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

    // Helvetica Neue, in three GFX faces (see fonts/helvetica_neue.h).
    const GFXfont* const FONT_SMALL = &HelveticaNeueRegular8pt7b;  // hints, footers
    const GFXfont* const FONT_COL   = &HelveticaNeueRegular9pt7b;  // weather right-column items
    const GFXfont* const FONT_MED   = &HelveticaNeueMedium10pt7b;  // headers, boot screens
    const GFXfont* const FONT_BIG   = &HelveticaNeueBold30pt7b;    // factory-reset countdown
    const GFXfont* const FONT_TEMP  = &HelveticaNeueBold57pt7b;    // full-height temperature

    constexpr int16_t INDICATOR_R   = 3;
    constexpr int16_t INDICATOR_PAD = 5;

    // Draw `text` horizontally centered, with its top edge at `top`. GFX custom
    // fonts position glyphs from the baseline, so we offset by the bounds' y1 to
    // make `top` mean the visual top regardless of which face is used.
    void center_print(const GFXfont* font, int16_t top, uint16_t color, const String& text) {
        tft.setFont(font);
        tft.setTextColor(color);
        int16_t  x1, y1;
        uint16_t w, h;
        tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
        int16_t x = (TFT_WIDTH - (int16_t)w) / 2 - x1;
        tft.setCursor(x, top - y1);
        tft.print(text);
    }

    // Draw `text` left-justified at `x`, with its top edge at `top`.
    void left_print_top(const GFXfont* font, int16_t x, int16_t top,
                        uint16_t color, const String& text) {
        tft.setFont(font);
        tft.setTextColor(color);
        int16_t  x1, y1;
        uint16_t w, h;
        tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
        tft.setCursor(x - x1, top - y1);
        tft.print(text);
    }

    // Draw `text` left-justified at `x`, vertically centered on `center_y`.
    void left_print_centered(const GFXfont* font, int16_t x, int16_t center_y,
                             uint16_t color, const String& text) {
        tft.setFont(font);
        tft.setTextColor(color);
        int16_t  x1, y1;
        uint16_t w, h;
        tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
        int16_t top = center_y - (int16_t)h / 2;
        tft.setCursor(x - x1, top - y1);
        tft.print(text);
    }

    // Draw `text` left-justified at `x`, word-wrapped to `max_w` (up to `max_lines`
    // lines stepped by `line_h`), with the last line's bottom edge at `bottom` (the
    // block grows upward from there).
    void left_print_wrapped_bottom(const GFXfont* font, int16_t x, int16_t bottom, int16_t max_w,
                                   int16_t line_h, uint8_t max_lines, uint16_t color, const String& text) {
        tft.setFont(font);
        tft.setTextColor(color);

        int16_t  x1, y1;
        uint16_t w, h;
        String   lines[4];           // max_lines is <= 4 in practice
        if (max_lines > 4) max_lines = 4;
        uint8_t  n     = 0;
        String   line  = "";
        int      start = 0;
        int      len   = text.length();

        while (start < len && n < max_lines) {
            int    sp   = text.indexOf(' ', start);
            String word = (sp < 0) ? text.substring(start) : text.substring(start, sp);
            String cand = line.length() ? line + " " + word : word;
            tft.getTextBounds(cand, 0, 0, &x1, &y1, &w, &h);
            if ((int16_t)w <= max_w || line.length() == 0) {
                line  = cand;
                start = (sp < 0) ? len : sp + 1;
            } else {
                lines[n++] = line;
                line = "";
            }
        }
        if (line.length() && n < max_lines) lines[n++] = line;

        // Last line (i = n-1) bottom sits at `bottom`; earlier lines stack up by line_h.
        for (uint8_t i = 0; i < n; ++i) {
            tft.getTextBounds(lines[i], 0, 0, &x1, &y1, &w, &h);
            int16_t line_bottom = bottom - (int16_t)(n - 1 - i) * line_h;
            tft.setCursor(x - x1, line_bottom - (int16_t)h - y1);
            tft.print(lines[i]);
        }
    }

    // Center `text` on both axes within the full screen.
    void center_print_middle(const GFXfont* font, uint16_t color, const String& text) {
        tft.setFont(font);
        tft.setTextColor(color);
        int16_t  x1, y1;
        uint16_t w, h;
        tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
        int16_t x = (TFT_WIDTH  - (int16_t)w) / 2 - x1;
        int16_t y = (TFT_HEIGHT - (int16_t)h) / 2 - y1;
        tft.setCursor(x, y);
        tft.print(text);
    }
}

namespace display {

void begin() {
    // Reverse TFT Feather: powering the I2C/peripheral rail also enables the display backlight rail.
    pinMode(PIN_TFT_I2C_POWER, OUTPUT);
    digitalWrite(PIN_TFT_I2C_POWER, HIGH);
    pinMode(PIN_TFT_BACKLIGHT, OUTPUT);
    digitalWrite(PIN_TFT_BACKLIGHT, HIGH);
    delay(10);

    tft.init(135, 240);
    tft.setRotation(TFT_ROTATION);
    tft.fillScreen(COLOR_BG);
    tft.setTextWrap(false);
    tft.setTextSize(1);   // custom fonts carry their own size; never scale them
}

void show_centered_message(const char* text) {
    tft.fillScreen(COLOR_BG);
    center_print_middle(FONT_MED, COLOR_FG, text);
}

void show_boot_message(const char* line1, const char* line2) {
    tft.fillScreen(COLOR_BG);
    center_print(FONT_MED, TFT_HEIGHT / 2 - 20, COLOR_FG, line1);
    if (line2) {
        center_print(FONT_SMALL, TFT_HEIGHT / 2 + 10, COLOR_DIM, line2);
    }
}

void show_portal_instructions(const char* ssid, const IPAddress& ip) {
    tft.fillScreen(COLOR_BG);
    center_print(FONT_MED,   8,  COLOR_ACCENT, "Setup mode");
    center_print(FONT_SMALL, 40, COLOR_FG,     "Connect phone to wifi:");
    center_print(FONT_MED,   56, COLOR_FG,     ssid);
    center_print(FONT_SMALL, 86, COLOR_DIM,    "Then open browser to:");
    center_print(FONT_SMALL, 102,COLOR_FG,     ip.toString());
    center_print(FONT_SMALL, 120,COLOR_DIM,    "(captive portal auto-pops)");
}

void show_factory_reset_countdown(uint8_t seconds_remaining) {
    tft.fillScreen(COLOR_BG);
    center_print(FONT_MED, 20, COLOR_WARN, "Hold D0 to");
    center_print(FONT_MED, 48, COLOR_WARN, "reset config");
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", seconds_remaining);
    center_print(FONT_BIG, 80, COLOR_FG, buf);
}

void draw_weather(const WeatherSnapshot& s) {
    tft.fillScreen(COLOR_BG);

    // Big temperature, left-justified and vertically centered, filling the height.
    char tbuf[8];
    snprintf(tbuf, sizeof(tbuf), "%.0f", s.temp_f);
    String temp = tbuf;
    if (s.stale) temp += "*";

    constexpr int16_t TEMP_X = 8;
    tft.setFont(FONT_TEMP);
    tft.setTextColor(COLOR_FG);
    int16_t  x1, y1;
    uint16_t w, h;
    tft.getTextBounds(temp, 0, 0, &x1, &y1, &w, &h);
    int16_t top = (TFT_HEIGHT - (int16_t)h) / 2;
    tft.setCursor(TEMP_X - x1, top - y1);
    tft.print(temp);

    // Right column, anchored to the temperature:
    //   low / high   top-aligned with the temperature's top
    //   precip       vertically centered on the screen middle
    //   description  bottom-aligned with the temperature's bottom (wraps upward)
    constexpr int16_t COL_X = 150;
    constexpr int16_t COL_W = TFT_WIDTH - COL_X;   // 90px
    int16_t temp_top    = top;
    int16_t temp_bottom = top + (int16_t)h;

    char hl[20], rn[12];
    snprintf(hl, sizeof(hl), "%.0f / %.0f", s.low_f, s.high_f);   // low / high
    snprintf(rn, sizeof(rn), "Rain %d%%", (int)roundf(s.pop * 100.0f));

    left_print_top(FONT_COL, COL_X, temp_top, COLOR_DIM, hl);
    left_print_centered(FONT_COL, COL_X, TFT_HEIGHT / 2, COLOR_DIM, rn);
    left_print_wrapped_bottom(FONT_COL, COL_X, temp_bottom, COL_W, 16, 2, COLOR_ACCENT, s.conditions);
}

void draw_no_data() {
    tft.fillScreen(COLOR_BG);
    center_print_middle(FONT_MED, COLOR_DIM, "Loading weather");
}

void show_setup_url(const IPAddress& ip, const char* mdns_host) {
    tft.fillScreen(COLOR_BG);
    center_print(FONT_MED,   6,  COLOR_ACCENT, "Finish setup");
    center_print(FONT_SMALL, 34, COLOR_FG,     "Open in your browser:");

    String mdns_url = String("http://") + mdns_host + ".local";
    center_print(FONT_SMALL, 54, COLOR_FG, mdns_url);

    center_print(FONT_SMALL, 72, COLOR_DIM, "or");

    String ip_url = String("http://") + ip.toString();
    center_print(FONT_SMALL, 90, COLOR_FG, ip_url);

    center_print(FONT_SMALL, 118, COLOR_DIM, "(any device on this wifi)");
}

void draw_wifi_indicator(bool wifi_ok) {
    int16_t cx = TFT_WIDTH - INDICATOR_PAD - INDICATOR_R;
    int16_t cy = INDICATOR_PAD + INDICATOR_R;
    tft.fillRect(cx - INDICATOR_R - 1, cy - INDICATOR_R - 1,
                 INDICATOR_R * 2 + 2, INDICATOR_R * 2 + 2, COLOR_BG);
    tft.fillCircle(cx, cy, INDICATOR_R, wifi_ok ? COLOR_ACCENT : COLOR_WARN);
}

} // namespace display
