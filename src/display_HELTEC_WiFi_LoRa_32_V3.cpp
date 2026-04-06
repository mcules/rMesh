#ifdef HELTEC_WIFI_LORA_32_V3

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>

#include "display_HELTEC_WiFi_LoRa_32_V3.h"
#include "settings.h"
#include "hal.h"
#include "webFunctions.h"
#include "logging.h"

// ── OLED pin definitions ────────────────────────────────────────────────────
#define OLED_SDA  17
#define OLED_SCL  18
#define OLED_RST  21
#define OLED_VEXT 36  // Vext controls OLED power (LOW = on)
#define OLED_I2C_ADDR 0x3C

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_RST, OLED_SCL, OLED_SDA);

static bool displayDetected = false;

static char lastMsgSrc[17] = {0};
static char lastMsgText[128] = {0};

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
    // Enable OLED power supply (Vext)
    pinMode(OLED_VEXT, OUTPUT);
    digitalWrite(OLED_VEXT, LOW);  // LOW = power on
    delay(50);

    // Reset OLED
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);
    delay(20);
    digitalWrite(OLED_RST, HIGH);
    delay(20);

    // Probe I2C
    Wire.begin(OLED_SDA, OLED_SCL);
    Wire.beginTransmission(OLED_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
        displayDetected = false;
        logPrintf(LOG_WARN, "Display", "No SSD1306 found");
        return false;
    }

    u8g2.begin();
    displayDetected = true;
    logPrintf(LOG_INFO, "Display", "SSD1306 detected (Heltec V3)");

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
    u8g2.setContrast(displayBrightness);
    u8g2.clearBuffer();

    // ── Line 1 (y=10): Callsign + battery ──────────────────────────────────
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, settings.mycall);

    #ifdef HAS_BATTERY_ADC
    if (batteryEnabled) {
        char batStr[12];
        snprintf(batStr, sizeof(batStr), "BAT:%d%%", getBatteryPercent());
        int w = u8g2.getStrWidth(batStr);
        u8g2.drawStr(128 - w, 10, batStr);
    }
    #endif

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
