/**
 * @file display_SSD1306_U8g2.cpp
 * @brief Generic SSD1306 128x64 OLED status display driver (U8g2).
 *
 * Serves all boards with a U8g2-driven SSD1306: Heltec V3, ESP32 E22,
 * LilyGo T3 LoRa32 V1.6.1, and any future board using the same display.
 * All pin configuration comes from IBoardConfig at runtime.
 */

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#include "display_SSD1306_U8g2.h"
#include "displayPagesU8g2.h"
#include "displayRotation.h"
#include "statusDisplay.h"
#include "hal/settings.h"
#include "hal/hal.h"
#include "network/webFunctions.h"
#include "util/logging.h"
#include "main.h"

#define OLED_I2C_ADDR 0x3C

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, U8X8_PIN_NONE, U8X8_PIN_NONE);

static bool displayDetected = false;
static char lastMsgSrc[17] = {0};
static char lastMsgText[128] = {0};

static DisplayRotator rotator;
static uint32_t splashUntil  = 0;
static bool     flashingLock = false;

// ── Optional page-cycle button (configured via WebUI oledButtonPin) ─────────
static int8_t   btnPinConfigured = -1;
static bool     btnLastLevel     = true;   // pullup idle = HIGH
static uint32_t btnLastChangeAt  = 0;

static bool pageAvailableCb(uint8_t p) {
    return u8g2PageAvailable(p, lastMsgSrc);
}

// ── Public API ──────────────────────────────────────────────────────────────

bool initStatusDisplay(const IBoardConfig* board) {
    // Enable OLED power supply (Vext) if present
    if (board->pinVext() >= 0) {
        pinMode(board->pinVext(), OUTPUT);
        digitalWrite(board->pinVext(), board->vextActiveLow() ? LOW : HIGH);
        delay(50);
    }

    // Hardware reset if the board has a dedicated reset pin
    if (board->pinDisplayRST() >= 0) {
        pinMode(board->pinDisplayRST(), OUTPUT);
        digitalWrite(board->pinDisplayRST(), LOW);
        delay(20);
        digitalWrite(board->pinDisplayRST(), HIGH);
        delay(20);
    }

    // Probe I2C for the display
    Wire.begin(board->pinSDA(), board->pinSCL());
    Wire.beginTransmission(OLED_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
        displayDetected = false;
        logPrintf(LOG_WARN, "Display", "No SSD1306 found");
        return false;
    }

    delay(10);  // feed watchdog before potentially long U8g2 init
    u8g2.begin();
    displayDetected = true;
    logPrintf(LOG_INFO, "Display", "SSD1306 128x64 detected");

    rotator.begin(PAGE_COUNT, pageAvailableCb);

    // Always show the boot splash, even when OLED is disabled in settings.
    showStatusDisplaySplash(5000);

    // Optional page-cycle button (only active when oledButtonPin is set via WebUI)
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
    if (!matchesDisplayGroup(dstGroup, dstCall)) return;

    strlcpy(lastMsgSrc, srcCall, sizeof(lastMsgSrc));
    strlcpy(lastMsgText, text, sizeof(lastMsgText));

    if (oledPageMask & (1u << PAGE_MESSAGES)) {
        rotator.forcePage((uint8_t)PAGE_MESSAGES, 10000);
    }
    updateStatusDisplay();
}

// ── Optional page-cycle button ──────────────────────────────────────────────

static void displayNextPage() {
    rotator.next();
    updateStatusDisplay();
}

void displayButtonPoll() {
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
