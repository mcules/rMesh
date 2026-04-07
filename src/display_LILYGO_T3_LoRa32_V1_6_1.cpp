#ifdef LILYGO_T3_LORA32_V1_6_1

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#include "display_LILYGO_T3_LoRa32_V1_6_1.h"
#include "displayPagesU8g2.h"
#include "displayRotation.h"
#include "statusDisplay.h"
#include "settings.h"
#include "hal.h"
#include "webFunctions.h"
#include "logging.h"
#include "main.h"

// ── OLED pin definitions ────────────────────────────────────────────────────
#define OLED_SDA  21
#define OLED_SCL  22
#define OLED_RST  16
#define OLED_I2C_ADDR 0x3C

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);

static bool displayDetected = false;
static char lastMsgSrc[17] = {0};
static char lastMsgText[128] = {0};

static DisplayRotator rotator;
static uint32_t splashUntil  = 0;
static bool     flashingLock = false;

static bool pageAvailableCb(uint8_t p) {
    return u8g2PageAvailable(p, lastMsgSrc);
}

// ── Public API ──────────────────────────────────────────────────────────────

bool initStatusDisplay() {
    // No hardware reset — the T3 V1.6.1 uses an ESP32-PICO-D4 where
    // GPIO 16 is not available as a general-purpose pin (crashes on
    // pinMode).  The display reset is handled by U8g2 internally when
    // the reset pin is set to U8X8_PIN_NONE (power-on reset suffices).

    Wire.begin(OLED_SDA, OLED_SCL);
    Wire.beginTransmission(OLED_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
        displayDetected = false;
        logPrintf(LOG_WARN, "Display", "No SSD1306 found");
        return false;
    }

    delay(10); // feed watchdog before potentially long init
    u8g2.begin();
    displayDetected = true;
    logPrintf(LOG_INFO, "Display", "SSD1306 detected (T3 LoRa32 V1.6.1)");

    rotator.begin(PAGE_COUNT, pageAvailableCb);

    // Always show the boot splash, even when OLED is disabled in settings.
    showStatusDisplaySplash(5000);
    return true;
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
    if (!displayDetected) return;
    uint32_t now = millis();
    if (now < splashUntil) return;
    if (flashingLock) return;

    if (!oledEnabled) {
        if (splashUntil != 0) {
            splashUntil = 0;
            u8g2.clearBuffer();
            u8g2.sendBuffer();
            u8g2.setPowerSave(1);
        }
        return;
    }
    splashUntil = 0;

    uint8_t page = rotator.tick(oledPageInterval ? oledPageInterval : 5000);

    u8g2.setPowerSave(0);
    u8g2.setContrast(displayBrightness);
    u8g2.clearBuffer();
    u8g2DrawPage(u8g2, page, lastMsgSrc, lastMsgText);
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

    if (oledPageMask & (1u << PAGE_MESSAGES)) {
        rotator.forcePage((uint8_t)PAGE_MESSAGES, 10000);
    }
    updateStatusDisplay();
}

#endif