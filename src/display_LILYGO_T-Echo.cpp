#ifdef LILYGO_T_ECHO

#include <SPI.h>
#include <GxEPD2_BW.h>
#include "display_LILYGO_T-Echo.h"
#include "displayRotation.h"
#include "statusDisplay.h"
#include "hal_LILYGO_T-Echo.h"
#include "settings.h"
#include "peer.h"
#include "routing.h"
#include "config.h"
#include "version.h"

// ── E-Paper Display (GDEH0154D67 200x200 on dedicated SPI) ──────────────────

// E-Paper SPI on SPIM2 (LoRa uses SPIM3)
SPIClass einkSPI(NRF_SPIM2, 38, EINK_SCK, EINK_MOSI);

// GxEPD2 display: GDEH0154D67 = 200x200 b/w
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>
    display(GxEPD2_154_D67(EINK_CS, EINK_DC, EINK_RES, EINK_BUSY));

static bool displayDetected = false;
static DisplayRotator rotator;
static uint32_t lastButtonTime = 0;
static bool lastButtonState = true;  // Active low, so true = not pressed
static uint32_t lastRefreshTime = 0;
static bool backlightOn = false;
static uint32_t splashUntil = 0;
static bool flashingLock = false;

static bool pageAvailableCb(uint8_t p) {
    if ((oledPageMask & (1u << p)) == 0) return false;
    return true;
}

// Backlight control (P1.11 = pin 43)
#define PIN_EINK_BL 43

static void setBacklight(bool on) {
    backlightOn = on;
    digitalWrite(PIN_EINK_BL, on ? HIGH : LOW);
}

// Message ring buffer for display
#define MAX_DISPLAY_MESSAGES 5
static struct {
    char srcCall[10];
    char text[80];
} displayMessages[MAX_DISPLAY_MESSAGES];
static uint8_t displayMsgHead = 0;
static bool displayDirty = true;

// ── Helper: Draw text with word wrap ─────────────────────────────────────────

static void drawWrappedText(int16_t x, int16_t y, const char* text,
                            int16_t maxWidth, int16_t lineHeight) {
    char line[40];
    const char* p = text;
    int16_t cy = y;

    while (*p && cy < 200) {
        // Fill one line
        int lineLen = 0;
        int lastSpace = -1;
        while (p[lineLen] && lineLen < 39) {
            if (p[lineLen] == ' ') lastSpace = lineLen;
            // Approximate: 6px per char at size 1
            if ((lineLen + 1) * 6 > maxWidth) {
                if (lastSpace > 0) { lineLen = lastSpace; break; }
                break;
            }
            lineLen++;
        }
        memcpy(line, p, lineLen);
        line[lineLen] = '\0';
        display.setCursor(x, cy);
        display.print(line);
        p += lineLen;
        if (*p == ' ') p++;
        cy += lineHeight;
    }
}


// ── Page Renderers ───────────────────────────────────────────────────────────

static void drawPageStatus() {
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print(NAME);
    display.setTextSize(1);
    display.setCursor(140, 8);
    display.print(VERSION);

    display.drawLine(0, 20, 200, 20, GxEPD_BLACK);

    display.setTextSize(1);
    int16_t y = 28;

    // Callsign
    display.setCursor(0, y);
    display.print("Call: ");
    display.print(strlen(settings.mycall) > 0 ? settings.mycall : "(not set)");
    y += 14;

    // Position
    if (strlen(settings.position) > 0) {
        display.setCursor(0, y);
        display.print("Pos:  ");
        display.print(settings.position);
        y += 14;
    }

    // LoRa config
    display.setCursor(0, y);
    if (loraReady) {
        display.printf("LoRa: %.3f MHz SF%d", settings.loraFrequency,
                       settings.loraSpreadingFactor);
    } else {
        display.print("LoRa: not configured");
    }
    y += 14;

    display.setCursor(0, y);
    display.printf("BW: %.1f kHz  CR: 4/%d", settings.loraBandwidth,
                   settings.loraCodingRate);
    y += 14;

    display.setCursor(0, y);
    display.printf("TX: %d dBm  Repeat: %s", settings.loraOutputPower,
                   settings.loraRepeat ? "ON" : "OFF");
    y += 14;

    // Battery
    #ifdef HAS_BATTERY_ADC
    if (batteryEnabled) {
        float v = getBatteryVoltage();
        int pct = (int)((v - 3.0f) / (batteryFullVoltage - 3.0f) * 100.0f);
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        display.setCursor(0, y);
        display.printf("Battery: %.2fV (%d%%)", v, pct);
        y += 14;
    }
    #endif

    // Uptime
    uint32_t up = millis() / 1000;
    display.setCursor(0, y);
    display.printf("Uptime: %02d:%02d:%02d", (int)(up / 3600),
                   (int)((up % 3600) / 60), (int)(up % 60));
    y += 14;

    // Peers count
    int available = 0;
    for (size_t i = 0; i < peerList.size(); i++) {
        if (peerList[i].available) available++;
    }
    display.setCursor(0, y);
    display.printf("Peers: %d/%d available", available, (int)peerList.size());

    // Page indicator at bottom
    display.drawLine(0, 186, 200, 186, GxEPD_BLACK);
    display.setCursor(60, 190);
    display.print("[STATUS]  > >");
}


static void drawPagePeers() {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("PEERS");
    display.drawLine(0, 12, 200, 12, GxEPD_BLACK);

    if (peerList.empty()) {
        display.setCursor(30, 90);
        display.print("No peers found");
    } else {
        // Header
        display.setCursor(0, 16);
        display.print("Call      RSSI   SNR  Hop");
        display.drawLine(0, 27, 200, 27, GxEPD_BLACK);

        int16_t y = 31;
        for (size_t i = 0; i < peerList.size() && y < 180; i++) {
            display.setCursor(0, y);
            char line[40];
            // Find hop count for this peer
            uint8_t hops = 0;
            for (size_t r = 0; r < routingList.size(); r++) {
                if (strcmp(routingList[r].srcCall, peerList[i].nodeCall) == 0) {
                    hops = routingList[r].hopCount;
                    break;
                }
            }
            snprintf(line, sizeof(line), "%-9s %4.0f  %4.1f  %d %s",
                     peerList[i].nodeCall,
                     peerList[i].rssi,
                     peerList[i].snr,
                     hops,
                     peerList[i].available ? "" : "[x]");
            display.print(line);
            y += 12;
        }
    }

    display.drawLine(0, 186, 200, 186, GxEPD_BLACK);
    display.setCursor(55, 190);
    display.print("< [PEERS] > >");
}


static void drawPageRoutes() {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("ROUTES");
    display.drawLine(0, 12, 200, 12, GxEPD_BLACK);

    if (routingList.empty()) {
        display.setCursor(30, 90);
        display.print("No routes yet");
    } else {
        // Header
        display.setCursor(0, 16);
        display.print("Dest      Via       Hops");
        display.drawLine(0, 27, 200, 27, GxEPD_BLACK);

        int16_t y = 31;
        for (size_t i = 0; i < routingList.size() && y < 180; i++) {
            display.setCursor(0, y);
            char line[40];
            snprintf(line, sizeof(line), "%-9s %-9s %d",
                     routingList[i].srcCall,
                     routingList[i].viaCall,
                     routingList[i].hopCount);
            display.print(line);
            y += 12;
        }
    }

    display.drawLine(0, 186, 200, 186, GxEPD_BLACK);
    display.setCursor(45, 190);
    display.print("< < [ROUTES] >");
}


static void drawPageMessages() {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("MESSAGES");
    display.drawLine(0, 12, 200, 12, GxEPD_BLACK);

    bool hasMessages = false;
    int16_t y = 18;

    // Show messages newest first
    for (int n = 0; n < MAX_DISPLAY_MESSAGES && y < 180; n++) {
        int idx = (displayMsgHead - 1 - n + MAX_DISPLAY_MESSAGES) % MAX_DISPLAY_MESSAGES;
        if (displayMessages[idx].srcCall[0] == '\0') continue;

        hasMessages = true;
        display.setCursor(0, y);
        display.print("<");
        display.print(displayMessages[idx].srcCall);
        display.print(">");
        y += 12;

        drawWrappedText(4, y, displayMessages[idx].text, 196, 11);
        // Estimate lines used
        int textLen = strlen(displayMessages[idx].text);
        int lines = (textLen * 6 / 196) + 1;
        y += lines * 11 + 4;

        display.drawLine(10, y - 2, 190, y - 2, GxEPD_BLACK);
    }

    if (!hasMessages) {
        display.setCursor(30, 90);
        display.print("No messages yet");
    }

    display.drawLine(0, 186, 200, 186, GxEPD_BLACK);
    display.setCursor(40, 190);
    display.print("< < < [MESSAGES]");
}


// ── Public Interface ─────────────────────────────────────────────────────────

bool initStatusDisplay() {
    // Backlight control pin (P1.11)
    pinMode(PIN_EINK_BL, OUTPUT);
    setBacklight(false);

    // Start E-Paper SPI (SPIM2) and tell GxEPD2 to use it
    einkSPI.begin();
    display.epd2.selectSPI(einkSPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    display.init(0);  // 0 = no debug output
    display.setRotation(3);
    display.setTextColor(GxEPD_BLACK);
    display.setTextWrap(false);

    displayDetected = true;

    rotator.begin(PAGE_COUNT, pageAvailableCb);

    // Enable display on first boot (default is off for OLED boards, but E-Paper should be on)
    if (!oledEnabled) {
        oledEnabled = true;
        // Set default display group to show all broadcasts
        if (oledDisplayGroup[0] == '\0') {
            strlcpy(oledDisplayGroup, "*", sizeof(oledDisplayGroup));
        }
        saveOledSettings();
    }

    // Always show the boot splash, even when the display is disabled in
    // settings — the regular update path will hibernate the panel afterwards.
    showStatusDisplaySplash(5000);

    return true;
}

void showStatusDisplaySplash(uint32_t holdMs) {
    if (!displayDetected) return;
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setTextSize(3);
        display.setCursor(40, 60);
        display.print("rMesh");
        display.setTextSize(1);
        display.setCursor(20, 110);
        display.print(VERSION);
        if (settings.mycall[0] != '\0') {
            display.setTextSize(2);
            int16_t cw = strlen(settings.mycall) * 12;
            display.setCursor((200 - cw) / 2, 140);
            display.print(settings.mycall);
        }
    } while (display.nextPage());
    splashUntil = millis() + holdMs;
}

void showStatusDisplayFlashing(const char* what) {
    if (!displayDetected) return;
    flashingLock = true;
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setTextSize(3);
        display.setCursor(10, 50);
        display.print("Flashing");
        display.setTextSize(2);
        display.setCursor(20, 100);
        display.print((what && what[0]) ? what : "...");
        display.setTextSize(1);
        display.setCursor(20, 150);
        display.print("do not power off");
    } while (display.nextPage());
}

void updateStatusDisplay() {
    if (!displayDetected) return;
    if (flashingLock) return;
    if (millis() < splashUntil) return;

    if (!oledEnabled) {
        if (splashUntil != 0) {
            splashUntil = 0;
            display.setFullWindow();
            display.firstPage();
            do { display.fillScreen(GxEPD_WHITE); } while (display.nextPage());
            display.hibernate();
        }
        return;
    }
    splashUntil = 0;

    // No auto-rotate on e-paper — manual / button-driven only. Pass 0 so the
    // rotator just honours the mask + any pending hold from a new message.
    uint8_t page = rotator.tick(0);

    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        switch (page) {
            case PAGE_STATUS:   drawPageStatus();   break;
            case PAGE_PEERS:    drawPagePeers();    break;
            case PAGE_ROUTES:   drawPageRoutes();   break;
            case PAGE_MESSAGES: drawPageMessages();  break;
            default: break;
        }
    } while (display.nextPage());

    displayDirty = false;
    lastRefreshTime = millis();
}

void enableStatusDisplay() {
    oledEnabled = true;
    saveOledSettings();
    updateStatusDisplay();
}

void disableStatusDisplay() {
    oledEnabled = false;
    saveOledSettings();
    // Clear display to save power
    if (displayDetected) {
        display.setFullWindow();
        display.firstPage();
        do {
            display.fillScreen(GxEPD_WHITE);
        } while (display.nextPage());
        display.hibernate();
    }
}

bool hasStatusDisplay() {
    return displayDetected;
}

void onStatusDisplayMessage(const char* srcCall, const char* text,
                            const char* dstGroup, const char* dstCall) {
    // Apply display group filter
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

    // Store message in ring buffer
    strlcpy(displayMessages[displayMsgHead].srcCall, srcCall, 10);
    strlcpy(displayMessages[displayMsgHead].text, text, 80);
    displayMsgHead = (displayMsgHead + 1) % MAX_DISPLAY_MESSAGES;

    displayDirty = true;

    // Auto-switch to messages page on new message (if MESSAGES is in mask).
    if (oledPageMask & (1u << PAGE_MESSAGES)) {
        rotator.forcePage((uint8_t)PAGE_MESSAGES, 30000);
    }

    // Trigger immediate refresh
    updateStatusDisplay();
}

void displayUpdateLoop() {
    if (!displayDetected) return;

    // Button handling (debounced)
    static bool longPressHandled = false;
    bool buttonState = digitalRead(PIN_BUTTON);

    // Button pressed (falling edge)
    if (!buttonState && lastButtonState && (millis() - lastButtonTime > 300)) {
        lastButtonTime = millis();
        longPressHandled = false;
    }

    // Long press (>1s): toggle backlight
    if (!buttonState && !longPressHandled && (millis() - lastButtonTime > 1000)) {
        setBacklight(!backlightOn);
        longPressHandled = true;
    }

    // Button released (rising edge) - short press: cycle pages
    if (buttonState && !lastButtonState && !longPressHandled && (millis() - lastButtonTime > 50)) {
        rotator.next();
        displayDirty = true;
    }
    lastButtonState = buttonState;

    // Periodic refresh (every 60s for e-paper to save power)
    if (oledEnabled && displayDirty && (millis() - lastRefreshTime > 5000)) {
        updateStatusDisplay();
    }
    if (oledEnabled && (millis() - lastRefreshTime > 60000)) {
        displayDirty = true;  // Force refresh every 60s
    }
}


#endif // LILYGO_T_ECHO
