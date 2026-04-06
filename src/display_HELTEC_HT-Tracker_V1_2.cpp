#ifdef HELTEC_HT_TRACKER_V1_2

#include <Arduino.h>
#include <WiFi.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "display_HELTEC_HT-Tracker_V1_2.h"
#include "settings.h"
#include "hal.h"
#include "webFunctions.h"
#include "logging.h"

// ── LovyanGFX panel configuration for ST7735S (80x160) ─────────────────────

class LGFX_HTTracker : public lgfx::LGFX_Device {
    lgfx::Panel_ST7735S _panel;
    lgfx::Bus_SPI       _bus;
    lgfx::Light_PWM     _light;
public:
    LGFX_HTTracker() {
        // SPI bus
        auto cfg_bus = _bus.config();
        cfg_bus.spi_host   = SPI2_HOST;
        cfg_bus.spi_mode   = 0;
        cfg_bus.freq_write  = 40000000;
        cfg_bus.freq_read   = 16000000;
        cfg_bus.pin_sclk   = TFT_SPI_SCK;
        cfg_bus.pin_mosi   = TFT_SPI_MOSI;
        cfg_bus.pin_miso   = -1;
        cfg_bus.pin_dc     = TFT_DC;
        _bus.config(cfg_bus);
        _panel.setBus(&_bus);

        // Panel
        auto cfg_panel = _panel.config();
        cfg_panel.pin_cs     = TFT_CS;
        cfg_panel.pin_rst    = TFT_RST;
        cfg_panel.pin_busy   = -1;
        cfg_panel.panel_width  = 80;
        cfg_panel.panel_height = 160;
        cfg_panel.offset_x     = 26;
        cfg_panel.offset_y     = 1;
        cfg_panel.offset_rotation = 0;
        cfg_panel.invert       = true;
        cfg_panel.rgb_order    = false;
        cfg_panel.memory_width  = 132;
        cfg_panel.memory_height = 162;
        _panel.config(cfg_panel);

        // Backlight
        auto cfg_light = _light.config();
        cfg_light.pin_bl   = TFT_BL;
        cfg_light.invert   = false;
        cfg_light.freq     = 12000;
        cfg_light.pwm_channel = 7;
        _light.config(cfg_light);
        _panel.setLight(&_light);

        setPanel(&_panel);
    }
};

static LGFX_HTTracker tft;
static bool displayDetected = false;

static char lastMsgSrc[17] = {0};
static char lastMsgText[128] = {0};

// ── Color definitions ───────────────────────────────────────────────────────
#define COL_BG      TFT_BLACK
#define COL_TEXT    TFT_WHITE
#define COL_HEADER  TFT_CYAN
#define COL_SEP     TFT_DARKGREY
#define COL_MSG     TFT_GREEN
#define COL_SENDER  TFT_YELLOW

// ── Battery helper ──────────────────────────────────────────────────────────
#ifdef HAS_BATTERY_ADC
static int getBatteryPercent() {
    float v = getBatteryVoltage();
    const float vEmpty = 3.0f;
    int pct = (int)((v - vEmpty) / (batteryFullVoltage - vEmpty) * 100.0f);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}
#endif

// ── Public API ──────────────────────────────────────────────────────────────

bool initStatusDisplay() {
    // Vext should already be enabled by initHal()
    tft.init();
    tft.setRotation(3);  // 90° CCW → Landscape 160x80
    tft.fillScreen(COL_BG);
    tft.setTextWrap(true);

    displayDetected = true;
    logPrintf(LOG_INFO, "Display", "ST7735 initialised (HT-Tracker V1.2)");

    if (oledEnabled) {
        tft.setBrightness(128);
        updateStatusDisplay();
    } else {
        tft.setBrightness(0);
    }
    return true;
}

void updateStatusDisplay() {
    if (!displayDetected || !oledEnabled) return;

    tft.setBrightness(128);
    tft.fillScreen(COL_BG);
    tft.setTextColor(COL_TEXT, COL_BG);

    int y = 2;

    // ── Line 1: Callsign ────────────────────────────────────────────────────
    tft.setTextSize(1);
    tft.setTextColor(COL_HEADER, COL_BG);
    tft.setCursor(2, y);
    tft.print(settings.mycall);

    // ── Battery (right-aligned) ─────────────────────────────────────────────
    #ifdef HAS_BATTERY_ADC
    if (batteryEnabled) {
        char batStr[12];
        snprintf(batStr, sizeof(batStr), "%d%%", getBatteryPercent());
        int w = strlen(batStr) * 6;  // 6px per char at size 1
        tft.setCursor(160 - w - 2, y);
        tft.print(batStr);
    }
    #endif
    y += 12;

    // ── Line 2: WiFi mode + IP ──────────────────────────────────────────────
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.setCursor(2, y);
    if (settings.apMode) {
        tft.print("AP 192.168.1.1");
    } else if (WiFi.status() == WL_CONNECTED) {
        tft.printf("IP %s", WiFi.localIP().toString().c_str());
    } else {
        tft.print("not connected");
    }
    y += 12;

    // ── Line 3: SSID / AP name ─────────────────────────────────────────────
    tft.setCursor(2, y);
    if (settings.apMode) {
        tft.printf("AP: %s", apName.isEmpty() ? "rMesh" : apName.c_str());
    } else if (WiFi.status() == WL_CONNECTED) {
        String ssid = WiFi.SSID();
        if (ssid.length() > 26) ssid = ssid.substring(0, 26);
        tft.print(ssid);
    }
    y += 14;

    // ── Separator ───────────────────────────────────────────────────────────
    tft.drawFastHLine(0, y, 160, COL_SEP);
    y += 4;

    // ── Last message ────────────────────────────────────────────────────────
    if (lastMsgSrc[0] != '\0') {
        tft.setTextColor(COL_SENDER, COL_BG);
        tft.setCursor(2, y);
        tft.printf("<%s>", lastMsgSrc);
        y += 10;

        tft.setTextColor(COL_MSG, COL_BG);
        tft.setCursor(2, y);
        // Word-wrap message text into remaining display area
        const char* p = lastMsgText;
        int charsPerLine = 26;  // 160px / 6px per char
        int maxLines = (80 - y) / 10;
        for (int line = 0; line < maxLines && *p; line++) {
            char buf[27] = {0};
            strlcpy(buf, p, sizeof(buf));
            tft.setCursor(2, y);
            tft.print(buf);
            p += strlen(buf);
            y += 10;
        }
    }
}

void enableStatusDisplay() {
    if (!displayDetected) return;
    oledEnabled = true;
    saveOledSettings();
    updateStatusDisplay();
    sendSettings();
}

void disableStatusDisplay() {
    if (!displayDetected) return;
    oledEnabled = false;
    saveOledSettings();
    tft.fillScreen(COL_BG);
    tft.setBrightness(0);
    sendSettings();
}

bool hasStatusDisplay() {
    return displayDetected;
}

void onStatusDisplayMessage(const char* srcCall, const char* text, const char* dstGroup, const char* dstCall) {
    if (oledDisplayGroup[0] == '\0') return;

    bool match = false;
    if (strcmp(oledDisplayGroup, "*") == 0) {
        match = (dstGroup[0] == '\0' && dstCall[0] == '\0');
    } else if (strcmp(oledDisplayGroup, "@DM") == 0) {
        match = (strcmp(dstCall, settings.mycall) == 0);
    } else {
        match = (strcmp(dstGroup, oledDisplayGroup) == 0);
    }
    if (!match) return;

    strlcpy(lastMsgSrc, srcCall, sizeof(lastMsgSrc));
    strlcpy(lastMsgText, text, sizeof(lastMsgText));

    updateStatusDisplay();
}

#endif
