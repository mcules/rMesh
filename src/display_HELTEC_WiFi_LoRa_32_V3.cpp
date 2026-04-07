#ifdef HELTEC_WIFI_LORA_32_V3

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#include "display_HELTEC_WiFi_LoRa_32_V3.h"
#include "displayPagesU8g2.h"
#include "displayRotation.h"
#include "statusDisplay.h"
#include "settings.h"
#include "hal.h"
#include "webFunctions.h"
#include "logging.h"
#include "main.h"

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

static DisplayRotator rotator;
static uint32_t splashUntil  = 0;
static bool     flashingLock = false;

static bool pageAvailableCb(uint8_t p) {
    return u8g2PageAvailable(p, lastMsgSrc);
}

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

    // After the splash hold elapses, honour the user's enable flag.
    uint32_t now = millis();
    if (now < splashUntil) return;
    if (flashingLock) return;

    if (!oledEnabled) {
        // Splash just expired and the user has the OLED disabled — clear once.
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