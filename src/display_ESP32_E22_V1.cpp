#ifdef ESP32_E22_V1

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#include "display_ESP32_E22_V1.h"
#include "displayPagesU8g2.h"
#include "displayRotation.h"
#include "statusDisplay.h"
#include "settings.h"
#include "hal.h"
#include "webFunctions.h"
#include "logging.h"
#include "main.h"

// ── OLED pin definitions (ESP32 default I²C) ────────────────────────────────
#define OLED_SDA  21
#define OLED_SCL  22
#define OLED_I2C_ADDR 0x3C

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);

static bool displayDetected = false;

static char lastMsgSrc[17] = {0};
static char lastMsgText[128] = {0};

static DisplayRotator rotator;
static uint32_t splashUntil  = 0;
static bool     flashingLock = false;

static int8_t   btnPinConfigured = -1;
static bool     btnLastLevel     = true;   // pullup idle = HIGH
static uint32_t btnLastChangeAt  = 0;

static bool pageAvailableCb(uint8_t p) {
    return u8g2PageAvailable(p, lastMsgSrc);
}

// ── Public API ──────────────────────────────────────────────────────────────

bool initStatusDisplay() {
    Wire.begin(OLED_SDA, OLED_SCL);
    Wire.beginTransmission(OLED_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
        displayDetected = false;
        logPrintf(LOG_WARN, "Display", "No SSD1306 found");
        return false;
    }

    u8g2.begin();
    displayDetected = true;
    logPrintf(LOG_INFO, "Display", "SSD1306 detected (ESP32 E22 Multimodul)");

    rotator.begin(PAGE_COUNT, pageAvailableCb);

    // Always show the boot splash if a display is present, even when the
    // OLED is disabled in settings — the regular update path will put it
    // back to sleep after the hold elapses.
    showStatusDisplaySplash(5000);

    btnPinConfigured = -1;
    if (oledButtonPin >= 0) {
        pinMode(oledButtonPin, INPUT_PULLUP);
        btnPinConfigured = oledButtonPin;
        btnLastLevel = true;
        btnLastChangeAt = millis();
        logPrintf(LOG_INFO, "Display", "Page-next button on GPIO %d", (int)oledButtonPin);
    }
    return true;
}

void displayButtonPoll() {
    // Once the boot splash has expired and the display is disabled in
    // settings, clear the panel and put it to sleep — updateStatusDisplay()
    // is gated on oledEnabled and won't do it for us.
    if (displayDetected && !oledEnabled && splashUntil != 0 && millis() >= splashUntil) {
        splashUntil = 0;
        u8g2.clearBuffer();
        u8g2.sendBuffer();
        u8g2.setPowerSave(1);
    }

    // Re-configure on pin change from WebUI
    if (oledButtonPin != btnPinConfigured) {
        btnPinConfigured = oledButtonPin;
        if (oledButtonPin >= 0) {
            pinMode(oledButtonPin, INPUT_PULLUP);
            btnLastLevel = true;
            btnLastChangeAt = millis();
        }
    }
    if (btnPinConfigured < 0 || !displayDetected || !oledEnabled) return;

    bool level = digitalRead(btnPinConfigured) != 0;
    uint32_t now = millis();
    if (level != btnLastLevel && (now - btnLastChangeAt) > 40) {
        btnLastChangeAt = now;
        btnLastLevel = level;
        if (!level) {  // pressed (active-low)
            displayNextPage();
        }
    }
}

void showStatusDisplaySplash(uint32_t holdMs) {
    if (!displayDetected) return;
    u8g2.setPowerSave(0);
    u8g2.setContrast(displayBrightness);
    u8g2.clearBuffer();
    u8g2DrawSplash(u8g2);
    u8g2.sendBuffer();
    splashUntil = millis() + holdMs;
}

void showStatusDisplayFlashing(const char* what) {
    if (!displayDetected) return;
    flashingLock = true;
    u8g2.setPowerSave(0);
    u8g2.setContrast(displayBrightness);
    u8g2.clearBuffer();
    u8g2DrawFlashing(u8g2, what);
    u8g2.sendBuffer();
}

void updateStatusDisplay() {
    if (!displayDetected || !oledEnabled) return;
    if (flashingLock) return;
    if (millis() < splashUntil) return;

    uint8_t page = rotator.tick(oledPageInterval ? oledPageInterval : 5000);

    u8g2.setPowerSave(0);
    u8g2.setContrast(displayBrightness);
    u8g2.clearBuffer();
    u8g2DrawPage(u8g2, page, lastMsgSrc, lastMsgText);
    u8g2.sendBuffer();
}

void displayNextPage() {
    rotator.next();
    updateStatusDisplay();
}

void displayForcePage(DisplayPage p, uint32_t holdMs) {
    rotator.forcePage((uint8_t)p, holdMs);
    updateStatusDisplay();
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

    if (oledPageMask & (1u << PAGE_MESSAGES)) {
        displayForcePage(PAGE_MESSAGES, 10000);
    } else {
        updateStatusDisplay();
    }
}

#endif // ESP32_E22_V1