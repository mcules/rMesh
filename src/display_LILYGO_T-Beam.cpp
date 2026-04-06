#ifdef LILYGO_T_BEAM

#include <Arduino.h>
#include <Wire.h>
#include <SSD1306Wire.h>
#include <WiFi.h>

#include "display_LILYGO_T-Beam.h"
#include "settings.h"
#include "hal.h"
#include "webFunctions.h"
#include "logging.h"

// ── OLED configuration (matches sfambach/ThingPulse reference) ──────────────
#define OLED_SDA  21
#define OLED_SCL  22
#define OLED_I2C_ADDR 0x3C

static SSD1306Wire display(OLED_I2C_ADDR, OLED_SDA, OLED_SCL);

static bool displayDetected = false;

static char lastMsgSrc[17] = {0};
static char lastMsgText[128] = {0};

// ── Public API ──────────────────────────────────────────────────────────────

bool initStatusDisplay() {
    // Probe I2C
    Wire.begin(OLED_SDA, OLED_SCL);
    Wire.beginTransmission(OLED_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
        displayDetected = false;
        logPrintf(LOG_WARN, "Display", "No display found");
        return false;
    }

    display.init();
    display.flipScreenVertically();
    displayDetected = true;
    logPrintf(LOG_INFO, "Display", "SSD1306 128x64 detected (T-Beam, ThingPulse)");

    if (oledEnabled) {
        updateStatusDisplay();
    } else {
        display.displayOff();
    }
    return true;
}

void updateStatusDisplay() {
    if (!displayDetected || !oledEnabled) return;

    display.displayOn();
    display.setBrightness(displayBrightness);
    display.clear();

    // ── Line 1 (y=0): Callsign ─────────────────────────────────────────────
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, settings.mycall);

    // ── Line 2 (y=13): Mode + IP ───────────────────────────────────────────
    char line2[32];
    if (settings.apMode) {
        snprintf(line2, sizeof(line2), "AP 192.168.1.1");
    } else if (WiFi.status() == WL_CONNECTED) {
        snprintf(line2, sizeof(line2), "IP %s", WiFi.localIP().toString().c_str());
    } else {
        snprintf(line2, sizeof(line2), "not connected");
    }
    display.drawString(0, 13, line2);

    // ── Line 3 (y=26): SSID / AP name ──────────────────────────────────────
    char line3[26];
    if (settings.apMode) {
        snprintf(line3, sizeof(line3), "AP: %s",
                 apName.isEmpty() ? "rMesh" : apName.c_str());
    } else if (WiFi.status() == WL_CONNECTED) {
        snprintf(line3, sizeof(line3), "%.21s", WiFi.SSID().c_str());
    } else {
        line3[0] = '\0';
    }
    display.drawString(0, 26, line3);

    // ── Separator ───────────────────────────────────────────────────────────
    display.drawHorizontalLine(0, 39, 128);

    // ── Lines 4-6 (y=42..): Last message ────────────────────────────────────
    if (lastMsgSrc[0] != '\0') {
        char sender[28];
        snprintf(sender, sizeof(sender), "<%s>", lastMsgSrc);
        display.drawString(0, 41, sender);
        char msgLine1[27] = {0};
        char msgLine2[27] = {0};
        strlcpy(msgLine1, lastMsgText, sizeof(msgLine1));
        if (strlen(lastMsgText) > 26) {
            strlcpy(msgLine2, lastMsgText + 26, sizeof(msgLine2));
        }
        display.drawString(0, 51, msgLine1);
    }

    display.display();
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
    display.clear();
    display.display();
    display.displayOff();
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
