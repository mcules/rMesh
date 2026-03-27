#ifdef LILYGO_T3_LORA32_V1_6_1

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>

#include "display_LILYGO_T3_LoRa32_V1_6_1.h"
#include "settings.h"
#include "hal.h"
#include "webFunctions.h"

// ── OLED pin definitions ────────────────────────────────────────────────────
#define OLED_SDA  21
#define OLED_SCL  22
#define OLED_RST  16
#define OLED_I2C_ADDR 0x3C

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);

static bool displayDetected = false;

static char lastMsgSrc[17] = {0};
static char lastMsgText[128] = {0};

// ── Public API ──────────────────────────────────────────────────────────────

bool initStatusDisplay() {
    // No hardware reset — the T3 V1.6.1 uses an ESP32-PICO-D4 where
    // GPIO 16 is not available as a general-purpose pin (crashes on
    // pinMode).  The display reset is handled by U8g2 internally when
    // the reset pin is set to U8X8_PIN_NONE (power-on reset suffices).

    // Probe I2C
    Wire.begin(OLED_SDA, OLED_SCL);
    Wire.beginTransmission(OLED_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
        displayDetected = false;
        Serial.println("[OLED] No SSD1306 found");
        return false;
    }

    delay(10); // feed watchdog before potentially long init
    u8g2.begin();
    displayDetected = true;
    Serial.println("[OLED] SSD1306 detected (T3 LoRa32 V1.6.1)");

    if (oledEnabled) {
        updateStatusDisplay();
    } else {
        u8g2.setPowerSave(1);
    }
    return true;
}

void updateStatusDisplay() {
    if (!displayDetected || !oledEnabled) return;

    u8g2.setPowerSave(0);
    u8g2.clearBuffer();

    // ── Line 1 (y=10): Callsign ────────────────────────────────────────────
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, settings.mycall);

    // ── Line 2 (y=22): Mode + IP ───────────────────────────────────────────
    char line2[32];
    if (settings.apMode) {
        snprintf(line2, sizeof(line2), "AP 192.168.1.1");
    } else if (WiFi.status() == WL_CONNECTED) {
        snprintf(line2, sizeof(line2), "IP %s", WiFi.localIP().toString().c_str());
    } else {
        snprintf(line2, sizeof(line2), "not connected");
    }
    u8g2.drawStr(0, 22, line2);

    // ── Line 3 (y=34): SSID / AP name ──────────────────────────────────────
    char line3[26];
    if (settings.apMode) {
        snprintf(line3, sizeof(line3), "AP: %s",
                 apName.isEmpty() ? "rMesh" : apName.c_str());
    } else if (WiFi.status() == WL_CONNECTED) {
        snprintf(line3, sizeof(line3), "%.21s", WiFi.SSID().c_str());
    } else {
        line3[0] = '\0';
    }
    u8g2.drawStr(0, 34, line3);

    // ── Separator ───────────────────────────────────────────────────────────
    u8g2.drawHLine(0, 37, 128);

    // ── Lines 4-6 (y=48..64): Last message ──────────────────────────────────
    if (lastMsgSrc[0] != '\0') {
        u8g2.setFont(u8g2_font_5x7_tf);
        char sender[28];
        snprintf(sender, sizeof(sender), "<%s>", lastMsgSrc);
        u8g2.drawStr(0, 48, sender);
        char msgLine1[27] = {0};
        char msgLine2[27] = {0};
        strlcpy(msgLine1, lastMsgText, sizeof(msgLine1));
        if (strlen(lastMsgText) > 26) {
            strlcpy(msgLine2, lastMsgText + 26, sizeof(msgLine2));
        }
        u8g2.drawStr(0, 57, msgLine1);
        u8g2.drawStr(0, 64, msgLine2);
    }

    u8g2.sendBuffer();
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
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    u8g2.setPowerSave(1);
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
