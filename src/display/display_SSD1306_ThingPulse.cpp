#include <Arduino.h>
#include <Wire.h>
#include <SSD1306Wire.h>
#include <WiFi.h>
#include <esp_system.h>

#include "display_SSD1306_ThingPulse.h"
#include "displayRotation.h"
#include "statusDisplay.h"
#include "hal/settings.h"
#include "hal/hal.h"
#include "network/webFunctions.h"
#include "util/logging.h"
#include "mesh/peer.h"
#include "mesh/routing.h"
#include "main.h"
#include "version.h"
#include "bsp/IBoardConfig.h"

#define OLED_I2C_ADDR 0x3C

// Page identifiers (mirror displayPagesU8g2.h — kept local because the
// T-Beam uses a different OLED library and can't share the U8G2 renderer).
enum TBeamPage : uint8_t {
    TB_PAGE_IDENTITY = 0,
    TB_PAGE_NETWORK,
    TB_PAGE_LORA_MESH,
    TB_PAGE_MESSAGES,
    TB_PAGE_SYSTEM,
    TB_PAGE_COUNT
};

static SSD1306Wire* display = nullptr;

static bool displayDetected = false;
static char lastMsgSrc[17] = {0};
static char lastMsgText[128] = {0};

static DisplayRotator rotator;
static uint32_t splashUntil  = 0;
static bool     flashingLock = false;

static bool pageAvailableCb(uint8_t p) {
    if ((oledPageMask & (1u << p)) == 0) return false;
    if (p == TB_PAGE_MESSAGES) {
        if (oledDisplayGroup[0] == '\0') return false;
        if (lastMsgSrc[0] == '\0')       return false;
    }
    return true;
}

static const char* pageTitle(uint8_t p) {
    switch (p) {
        case TB_PAGE_IDENTITY:  return "ID";
        case TB_PAGE_NETWORK:   return "NET";
        case TB_PAGE_LORA_MESH: return "LoRa";
        case TB_PAGE_MESSAGES:  return "MSG";
        case TB_PAGE_SYSTEM:    return "SYS";
        default:                return "";
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

// ── Page renderers ──────────────────────────────────────────────────────────

static void drawHeader(const char* title) {
    display->setFont(ArialMT_Plain_10);
    display->setColor(WHITE);
    display->fillRect(0, 0, 128, 12);
    display->setColor(BLACK);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(0, 0, "rMesh");
    if (title && title[0]) {
        display->setTextAlignment(TEXT_ALIGN_RIGHT);
        display->drawString(127, 0, title);
    }
    display->setColor(WHITE);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
}

static void drawIdentity() {
    display->setFont(ArialMT_Plain_10);
    display->drawString(0, 14, settings.mycall);

    char line2[32];
    if (settings.apMode) {
        snprintf(line2, sizeof(line2), "AP 192.168.1.1");
    } else if (WiFi.status() == WL_CONNECTED) {
        snprintf(line2, sizeof(line2), "IP %s", WiFi.localIP().toString().c_str());
    } else {
        snprintf(line2, sizeof(line2), "not connected");
    }
    display->drawString(0, 26, line2);

    char line3[26];
    if (settings.apMode) {
        snprintf(line3, sizeof(line3), "AP: %s",
                 apName.isEmpty() ? "rMesh" : apName.c_str());
    } else if (WiFi.status() == WL_CONNECTED) {
        snprintf(line3, sizeof(line3), "%.21s", WiFi.SSID().c_str());
    } else {
        line3[0] = '\0';
    }
    display->drawString(0, 38, line3);

    if (lastMsgSrc[0] != '\0') {
        char sender[40];
        snprintf(sender, sizeof(sender), "<%s> %s", lastMsgSrc, lastMsgText);
        display->drawString(0, 51, sender);
    }
}

static void drawNetwork() {
    display->setFont(ArialMT_Plain_10);
    char buf[32];

    if (settings.apMode) {
        display->drawString(0, 14, "Mode: AP");
        snprintf(buf, sizeof(buf), "SSID: %.18s",
                 apName.isEmpty() ? "rMesh" : apName.c_str());
        display->drawString(0, 26, buf);
        display->drawString(0, 38, "IP:   192.168.1.1");
    } else if (WiFi.status() == WL_CONNECTED) {
        snprintf(buf, sizeof(buf), "SSID: %.18s", WiFi.SSID().c_str());
        display->drawString(0, 14, buf);
        snprintf(buf, sizeof(buf), "IP:   %s", WiFi.localIP().toString().c_str());
        display->drawString(0, 26, buf);
        snprintf(buf, sizeof(buf), "RSSI: %d dBm", (int)WiFi.RSSI());
        display->drawString(0, 38, buf);
    } else {
        display->drawString(0, 14, "WiFi: disconnected");
    }

    char host[40];
    snprintf(host, sizeof(host), "%s-rmesh.local", settings.mycall);
    display->drawString(0, 51, host);
}

static void drawLoraMesh() {
    display->setFont(ArialMT_Plain_10);
    char buf[32];

    snprintf(buf, sizeof(buf), "F:%.3f SF%u",
             settings.loraFrequency, (unsigned)settings.loraSpreadingFactor);
    display->drawString(0, 14, buf);

    snprintf(buf, sizeof(buf), "BW:%.1f CR:4/%u",
             settings.loraBandwidth, (unsigned)settings.loraCodingRate);
    display->drawString(0, 26, buf);

    snprintf(buf, sizeof(buf), "TX pwr: %d dBm", (int)settings.loraOutputPower);
    display->drawString(0, 38, buf);

    size_t nPeers = 0;
    size_t nRoutes = 0;
    if (listMutex && xSemaphoreTake(listMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        nPeers = peerList.size();
        nRoutes = routingList.size();
        xSemaphoreGive(listMutex);
    }
    snprintf(buf, sizeof(buf), "Peers:%u Routes:%u",
             (unsigned)nPeers, (unsigned)nRoutes);
    display->drawString(0, 51, buf);
}

static void drawMessages() {
    display->setFont(ArialMT_Plain_10);
    if (lastMsgSrc[0] == '\0') {
        display->drawString(0, 26, "(no messages)");
        return;
    }
    char sender[28];
    snprintf(sender, sizeof(sender), "<%s>", lastMsgSrc);
    display->drawString(0, 14, sender);

    // Wrap long messages across two lines.
    char line1[27] = {0};
    char line2[27] = {0};
    char line3[27] = {0};
    size_t len = strlen(lastMsgText);
    strlcpy(line1, lastMsgText, sizeof(line1));
    if (len > 25) strlcpy(line2, lastMsgText + 25, sizeof(line2));
    if (len > 50) strlcpy(line3, lastMsgText + 50, sizeof(line3));
    display->drawString(0, 28, line1);
    display->drawString(0, 40, line2);
    display->drawString(0, 52, line3);
}

static void drawSystem() {
    display->setFont(ArialMT_Plain_10);
    char buf[32];

    uint32_t freeKb = ESP.getFreeHeap() / 1024;
    uint32_t minKb  = ESP.getMinFreeHeap() / 1024;
    snprintf(buf, sizeof(buf), "Heap: %luk/%luk", (unsigned long)freeKb, (unsigned long)minKb);
    display->drawString(0, 14, buf);

    char up[16];
    formatUptime(up, sizeof(up), (uint32_t)(millis() / 1000));
    snprintf(buf, sizeof(buf), "Up:   %s", up);
    display->drawString(0, 26, buf);

    snprintf(buf, sizeof(buf), "CPU:  %u MHz", (unsigned)cpuFrequency);
    display->drawString(0, 38, buf);

    char v1[26] = {0};
    strlcpy(v1, VERSION, sizeof(v1));
    display->drawString(0, 51, v1);
}

static void drawSplashContent() {
    display->setFont(ArialMT_Plain_24);
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(64, 4, "rMesh");

    display->setFont(ArialMT_Plain_10);
    display->drawString(64, 32, VERSION);

    if (settings.mycall[0] != '\0') {
        display->drawString(64, 48, settings.mycall);
    }
    display->setTextAlignment(TEXT_ALIGN_LEFT);
}

static void drawFlashingContent(const char* what) {
    display->setFont(ArialMT_Plain_24);
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(64, 0, "Flashing");

    display->setFont(ArialMT_Plain_10);
    display->drawString(64, 30, (what && what[0]) ? what : "...");
    display->drawString(64, 48, "do not power off");
    display->setTextAlignment(TEXT_ALIGN_LEFT);
}

// ── Public API ──────────────────────────────────────────────────────────────

bool initStatusDisplay(const IBoardConfig* board) {
    Wire.begin(board->pinSDA(), board->pinSCL());
    Wire.beginTransmission(OLED_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
        displayDetected = false;
        logPrintf(LOG_WARN, "Display", "No display found");
        return false;
    }

    display = new SSD1306Wire(OLED_I2C_ADDR, board->pinSDA(), board->pinSCL());
    display->init();
    display->flipScreenVertically();
    displayDetected = true;
    logPrintf(LOG_INFO, "Display", "SSD1306 128x64 detected (T-Beam, ThingPulse)");

    rotator.begin(TB_PAGE_COUNT, pageAvailableCb);

    // Always show the boot splash, even when OLED is disabled in settings.
    showStatusDisplaySplash(5000);
    return true;
}

void showStatusDisplaySplash(uint32_t holdMs) {
    if (!displayDetected) return;
    display->displayOn();
    display->setBrightness(displayBrightness);
    display->clear();
    drawSplashContent();
    display->display();
    splashUntil = millis() + holdMs;
}

void showStatusDisplayFlashing(const char* what) {
    if (!displayDetected) return;
    flashingLock = true;
    display->displayOn();
    display->setBrightness(displayBrightness);
    display->clear();
    drawFlashingContent(what);
    display->display();
}

void updateStatusDisplay() {
    if (!displayDetected) return;
    uint32_t now = millis();
    if (now < splashUntil) return;
    if (flashingLock) return;

    if (!oledEnabled) {
        if (splashUntil != 0) {
            splashUntil = 0;
            display->clear();
            display->display();
            display->displayOff();
        }
        return;
    }
    splashUntil = 0;

    uint8_t page = rotator.tick(oledPageInterval ? oledPageInterval : 5000);

    display->displayOn();
    display->setBrightness(displayBrightness);
    display->clear();
    drawHeader(pageTitle(page));
    switch (page) {
        case TB_PAGE_NETWORK:   drawNetwork();   break;
        case TB_PAGE_LORA_MESH: drawLoraMesh();  break;
        case TB_PAGE_MESSAGES:  drawMessages();  break;
        case TB_PAGE_SYSTEM:    drawSystem();    break;
        case TB_PAGE_IDENTITY:
        default:                drawIdentity();  break;
    }
    display->display();
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
    display->clear();
    display->display();
    display->displayOff();
    sendSettings();
}

bool hasStatusDisplay() {
    return displayDetected;
}

void onStatusDisplayMessage(const char* srcCall, const char* text, const char* dstGroup, const char* dstCall) {
    if (!matchesDisplayGroup(dstGroup, dstCall)) return;

    strlcpy(lastMsgSrc, srcCall, sizeof(lastMsgSrc));
    strlcpy(lastMsgText, text, sizeof(lastMsgText));

    if (oledPageMask & (1u << TB_PAGE_MESSAGES)) {
        rotator.forcePage((uint8_t)TB_PAGE_MESSAGES, 10000);
    }
    updateStatusDisplay();
}
