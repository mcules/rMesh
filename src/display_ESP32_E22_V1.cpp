#ifdef ESP32_E22_V1

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>

#include "display_ESP32_E22_V1.h"
#include "settings.h"
#include "hal.h"
#include "webFunctions.h"
#include "logging.h"
#include "peer.h"
#include "routing.h"
#include "main.h"
#include "version.h"
#include <esp_system.h>

// ── OLED pin definitions (ESP32 default I²C) ────────────────────────────────
#define OLED_SDA  21
#define OLED_SCL  22
#define OLED_I2C_ADDR 0x3C

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);

static bool displayDetected = false;

static char lastMsgSrc[17] = {0};
static char lastMsgText[128] = {0};

static DisplayPage currentPage = PAGE_IDENTITY;
static uint32_t pageSwitchAt  = 0;
static uint32_t pageHoldUntil = 0;
static uint32_t splashUntil   = 0;
static bool     flashingLock  = false;

static int8_t   btnPinConfigured = -1;
static bool     btnLastLevel     = true;   // pullup idle = HIGH
static uint32_t btnLastChangeAt  = 0;

static const char* pageTitle(DisplayPage p) {
    switch (p) {
        case PAGE_IDENTITY:  return "ID";
        case PAGE_NETWORK:   return "NET";
        case PAGE_LORA_MESH: return "LoRa";
        case PAGE_MESSAGES:  return "MSG";
        case PAGE_SYSTEM:    return "SYS";
        default:             return "";
    }
}

static void formatUptime(char* out, size_t n, uint32_t secs) {
    uint32_t d = secs / 86400;
    uint32_t h = (secs % 86400) / 3600;
    uint32_t m = (secs % 3600) / 60;
    uint32_t s = secs % 60;
    if (d > 0)      snprintf(out, n, "%lud %02luh", (unsigned long)d, (unsigned long)h);
    else if (h > 0) snprintf(out, n, "%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
    else            snprintf(out, n, "%02lu:%02lu", (unsigned long)m, (unsigned long)s);
}

static void drawHeader(const char* title) {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawBox(0, 0, 128, 11);
    u8g2.setDrawColor(0);
    u8g2.drawStr(1, 9, "rMesh");
    if (title && title[0]) {
        uint8_t tw = u8g2.getStrWidth(title);
        u8g2.drawStr(127 - tw, 9, title);
    }
    u8g2.setDrawColor(1);
}

static void drawIdentityPage() {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 22, settings.mycall);

    char line2[32];
    if (settings.apMode) {
        snprintf(line2, sizeof(line2), "AP 192.168.1.1");
    } else if (WiFi.status() == WL_CONNECTED) {
        snprintf(line2, sizeof(line2), "IP %s", WiFi.localIP().toString().c_str());
    } else {
        snprintf(line2, sizeof(line2), "not connected");
    }
    u8g2.drawStr(0, 34, line2);

    char line3[26];
    if (settings.apMode) {
        snprintf(line3, sizeof(line3), "AP: %s",
                 apName.isEmpty() ? "rMesh" : apName.c_str());
    } else if (WiFi.status() == WL_CONNECTED) {
        snprintf(line3, sizeof(line3), "%.21s", WiFi.SSID().c_str());
    } else {
        line3[0] = '\0';
    }
    u8g2.drawStr(0, 46, line3);

    u8g2.drawHLine(0, 49, 128);

    if (lastMsgSrc[0] != '\0') {
        u8g2.setFont(u8g2_font_5x7_tf);
        char sender[28];
        snprintf(sender, sizeof(sender), "<%s>", lastMsgSrc);
        u8g2.drawStr(0, 57, sender);
        char msgLine1[27] = {0};
        strlcpy(msgLine1, lastMsgText, sizeof(msgLine1));
        u8g2.drawStr(0, 64, msgLine1);
    }
}

// ── Public API ──────────────────────────────────────────────────────────────

bool initStatusDisplay() {
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
    logPrintf(LOG_INFO, "Display", "SSD1306 detected (ESP32 E22 Multimodul)");

    // Always show the boot splash if a display is present, even when the
    // OLED is disabled in settings — the splash powers on briefly and the
    // regular update path will put it back to sleep afterwards.
    showStatusDisplaySplash(5000);

    // Optional page-next button
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
    // If the boot splash has expired and the display is disabled in settings,
    // clear the panel and put it to sleep (the main loop only calls
    // updateStatusDisplay when oledEnabled=true, so we handle it here).
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

static void drawNetworkPage() {
    u8g2.setFont(u8g2_font_6x10_tf);
    char buf[32];

    if (settings.apMode) {
        snprintf(buf, sizeof(buf), "Mode: AP");
        u8g2.drawStr(0, 22, buf);
        snprintf(buf, sizeof(buf), "SSID: %.18s",
                 apName.isEmpty() ? "rMesh" : apName.c_str());
        u8g2.drawStr(0, 34, buf);
        u8g2.drawStr(0, 46, "IP:   192.168.1.1");
    } else if (WiFi.status() == WL_CONNECTED) {
        snprintf(buf, sizeof(buf), "SSID: %.18s", WiFi.SSID().c_str());
        u8g2.drawStr(0, 22, buf);
        snprintf(buf, sizeof(buf), "IP:   %s", WiFi.localIP().toString().c_str());
        u8g2.drawStr(0, 34, buf);
        snprintf(buf, sizeof(buf), "RSSI: %d dBm", (int)WiFi.RSSI());
        u8g2.drawStr(0, 46, buf);
    } else {
        u8g2.drawStr(0, 22, "WiFi: disconnected");
    }

    u8g2.setFont(u8g2_font_5x7_tf);
    char host[40];
    snprintf(host, sizeof(host), "%s-rmesh.local", settings.mycall);
    u8g2.drawStr(0, 64, host);
}

static void drawLoraMeshPage() {
    u8g2.setFont(u8g2_font_6x10_tf);
    char buf[32];

    snprintf(buf, sizeof(buf), "F:%.3f SF%u",
             settings.loraFrequency, (unsigned)settings.loraSpreadingFactor);
    u8g2.drawStr(0, 22, buf);

    snprintf(buf, sizeof(buf), "BW:%.1f CR:4/%u",
             settings.loraBandwidth, (unsigned)settings.loraCodingRate);
    u8g2.drawStr(0, 34, buf);

    snprintf(buf, sizeof(buf), "TX pwr: %d dBm", (int)settings.loraOutputPower);
    u8g2.drawStr(0, 46, buf);

    size_t nPeers = 0;
    size_t nRoutes = 0;
    if (listMutex && xSemaphoreTake(listMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        nPeers = peerList.size();
        nRoutes = routingList.size();
        xSemaphoreGive(listMutex);
    }
    snprintf(buf, sizeof(buf), "Peers:%u Routes:%u",
             (unsigned)nPeers, (unsigned)nRoutes);
    u8g2.drawStr(0, 58, buf);
}

static void drawMessagesPage() {
    u8g2.setFont(u8g2_font_6x10_tf);
    if (lastMsgSrc[0] == '\0') {
        u8g2.drawStr(0, 34, "(no messages)");
        return;
    }
    char sender[28];
    snprintf(sender, sizeof(sender), "<%s>", lastMsgSrc);
    u8g2.drawStr(0, 22, sender);

    u8g2.setFont(u8g2_font_5x7_tf);
    char line1[27] = {0};
    char line2[27] = {0};
    char line3[27] = {0};
    size_t len = strlen(lastMsgText);
    strlcpy(line1, lastMsgText, sizeof(line1));
    if (len > 25) strlcpy(line2, lastMsgText + 25, sizeof(line2));
    if (len > 50) strlcpy(line3, lastMsgText + 50, sizeof(line3));
    u8g2.drawStr(0, 40, line1);
    u8g2.drawStr(0, 50, line2);
    u8g2.drawStr(0, 60, line3);
}

static void drawSystemPage() {
    u8g2.setFont(u8g2_font_6x10_tf);
    char buf[32];

    uint32_t freeKb = ESP.getFreeHeap() / 1024;
    uint32_t minKb  = ESP.getMinFreeHeap() / 1024;
    snprintf(buf, sizeof(buf), "Heap: %luk/%luk", (unsigned long)freeKb, (unsigned long)minKb);
    u8g2.drawStr(0, 22, buf);

    char up[16];
    formatUptime(up, sizeof(up), (uint32_t)(millis() / 1000));
    snprintf(buf, sizeof(buf), "Up:   %s", up);
    u8g2.drawStr(0, 34, buf);

    snprintf(buf, sizeof(buf), "CPU:  %u MHz", (unsigned)cpuFrequency);
    u8g2.drawStr(0, 46, buf);

    u8g2.setFont(u8g2_font_5x7_tf);
    // Full version, wrapped across up to two 25-char lines.
    const char* v = VERSION;
    char v1[26] = {0};
    char v2[26] = {0};
    strlcpy(v1, v, sizeof(v1));
    if (strlen(v) > 25) strlcpy(v2, v + 25, sizeof(v2));
    u8g2.drawStr(0, 56, v1);
    u8g2.drawStr(0, 64, v2);
}

// Returns true if the given page should participate in the rotation.
static bool pageAvailable(DisplayPage p) {
    if ((oledPageMask & (1u << p)) == 0) return false;
    if (p == PAGE_MESSAGES) {
        // Only show MESSAGES if a channel filter is active AND a message exists.
        if (oledDisplayGroup[0] == '\0') return false;
        if (lastMsgSrc[0] == '\0')       return false;
    }
    return true;
}

void showStatusDisplaySplash(uint32_t holdMs) {
    if (!displayDetected) return;
    u8g2.setPowerSave(0);
    u8g2.setContrast(displayBrightness);
    u8g2.clearBuffer();

    // Big "rMesh" heading centered
    u8g2.setFont(u8g2_font_logisoso22_tr);
    const char* title = "rMesh";
    uint8_t tw = u8g2.getStrWidth(title);
    u8g2.drawStr((128 - tw) / 2, 28, title);

    // Version line (wrap if needed)
    u8g2.setFont(u8g2_font_5x7_tf);
    char v1[26] = {0};
    char v2[26] = {0};
    const char* v = VERSION;
    strlcpy(v1, v, sizeof(v1));
    if (strlen(v) > 25) strlcpy(v2, v + 25, sizeof(v2));
    uint8_t w1 = u8g2.getStrWidth(v1);
    u8g2.drawStr((128 - w1) / 2, 42, v1);
    if (v2[0]) {
        uint8_t w2 = u8g2.getStrWidth(v2);
        u8g2.drawStr((128 - w2) / 2, 50, v2);
    }

    // Node name (if set)
    if (settings.mycall[0] != '\0') {
        u8g2.setFont(u8g2_font_6x10_tf);
        uint8_t cw = u8g2.getStrWidth(settings.mycall);
        u8g2.drawStr((128 - cw) / 2, 63, settings.mycall);
    }

    u8g2.sendBuffer();
    splashUntil = millis() + holdMs;
}

void showStatusDisplayFlashing() {
    if (!displayDetected) return;
    flashingLock = true;
    u8g2.setPowerSave(0);
    u8g2.setContrast(displayBrightness);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_logisoso22_tr);
    const char* txt = "Flashing";
    uint8_t tw = u8g2.getStrWidth(txt);
    u8g2.drawStr((128 - tw) / 2, 32, txt);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr((128 - u8g2.getStrWidth("...")) / 2, 50, "...");
    u8g2.drawStr((128 - u8g2.getStrWidth("do not power off")) / 2, 62, "do not power off");
    u8g2.sendBuffer();
}

void updateStatusDisplay() {
    if (!displayDetected || !oledEnabled) return;
    if (flashingLock) return;
    if (millis() < splashUntil) return;

    // Auto-advance to next page unless a hold is active (e.g. after new message).
    // Skip pages that are masked out via oledPageMask.
    uint32_t now = millis();
    if (now >= pageHoldUntil) {
        if (pageSwitchAt != 0 && now >= pageSwitchAt) {
            for (uint8_t i = 0; i < PAGE_COUNT; ++i) {
                currentPage = (DisplayPage)((currentPage + 1) % PAGE_COUNT);
                if (pageAvailable(currentPage)) break;
            }
        }
        // If the currently-selected page is no longer available (e.g. mask changed
        // via WebUI, or MESSAGES has no content), fall back to the next one.
        if (!pageAvailable(currentPage)) {
            for (uint8_t i = 0; i < PAGE_COUNT; ++i) {
                currentPage = (DisplayPage)((currentPage + 1) % PAGE_COUNT);
                if (pageAvailable(currentPage)) break;
            }
        }
        pageSwitchAt = now + (oledPageInterval ? oledPageInterval : 5000);
    }

    u8g2.setPowerSave(0);
    u8g2.setContrast(displayBrightness);
    u8g2.clearBuffer();

    drawHeader(pageTitle(currentPage));

    switch (currentPage) {
        case PAGE_NETWORK:   drawNetworkPage();  break;
        case PAGE_LORA_MESH: drawLoraMeshPage(); break;
        case PAGE_MESSAGES:  drawMessagesPage(); break;
        case PAGE_SYSTEM:    drawSystemPage();   break;
        case PAGE_IDENTITY:
        default:             drawIdentityPage(); break;
    }

    u8g2.sendBuffer();
}

void displayNextPage() {
    currentPage = (DisplayPage)((currentPage + 1) % PAGE_COUNT);
    pageSwitchAt = 0;
    pageHoldUntil = 0;
    updateStatusDisplay();
}

void displayForcePage(DisplayPage p, uint32_t holdMs) {
    currentPage = p;
    pageHoldUntil = millis() + holdMs;
    pageSwitchAt = pageHoldUntil;
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
