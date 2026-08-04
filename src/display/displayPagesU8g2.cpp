#ifdef HAS_U8G2_DISPLAY

#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>

#include "displayPagesU8g2.h"
#include "hal/settings.h"
#include "hal/hal.h"
#include "network/webFunctions.h"
#include "mesh/peer.h"
#include "mesh/routing.h"
#include "main.h"
#include "version.h"

// ── Helpers ─────────────────────────────────────────────────────────────────

const char* u8g2PageTitle(uint8_t page) {
    switch (page) {
        case PAGE_IDENTITY:  return "ID";
        case PAGE_NETWORK:   return "NET";
        case PAGE_LORA_MESH: return "LoRa";
        case PAGE_MESSAGES:  return "MSG";
        case PAGE_SYSTEM:    return "SYS";
        default:             return "";
    }
}

bool u8g2PageAvailable(uint8_t page, const char* lastMsgSrc) {
    if ((oledPageMask & (1u << page)) == 0) return false;
    if (page == PAGE_MESSAGES) {
        if (oledDisplayGroup[0] == '\0') return false;
        if (!lastMsgSrc || lastMsgSrc[0] == '\0') return false;
    }
    return true;
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

static void drawHeader(U8G2& u8g2, const char* title) {
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

// ── Pages ───────────────────────────────────────────────────────────────────

static void drawIdentity(U8G2& u8g2, const char* lastMsgSrc, const char* lastMsgText) {
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

    if (lastMsgSrc && lastMsgSrc[0] != '\0') {
        u8g2.setFont(u8g2_font_5x7_tf);
        char sender[28];
        snprintf(sender, sizeof(sender), "<%s>", lastMsgSrc);
        u8g2.drawStr(0, 57, sender);
        char msgLine1[27] = {0};
        strlcpy(msgLine1, lastMsgText ? lastMsgText : "", sizeof(msgLine1));
        u8g2.drawStr(0, 64, msgLine1);
    }
}

static void drawNetwork(U8G2& u8g2) {
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

static void drawLoraMesh(U8G2& u8g2) {
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

static void drawMessages(U8G2& u8g2, const char* lastMsgSrc, const char* lastMsgText) {
    u8g2.setFont(u8g2_font_6x10_tf);
    if (!lastMsgSrc || lastMsgSrc[0] == '\0') {
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
    const char* text = lastMsgText ? lastMsgText : "";
    size_t len = strlen(text);
    strlcpy(line1, text, sizeof(line1));
    if (len > 25) strlcpy(line2, text + 25, sizeof(line2));
    if (len > 50) strlcpy(line3, text + 50, sizeof(line3));
    u8g2.drawStr(0, 40, line1);
    u8g2.drawStr(0, 50, line2);
    u8g2.drawStr(0, 60, line3);
}

static void drawSystem(U8G2& u8g2) {
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
    const char* v = VERSION;
    char v1[26] = {0};
    char v2[26] = {0};
    strlcpy(v1, v, sizeof(v1));
    if (strlen(v) > 25) strlcpy(v2, v + 25, sizeof(v2));
    u8g2.drawStr(0, 56, v1);
    u8g2.drawStr(0, 64, v2);
}

void u8g2DrawPage(U8G2& u8g2, uint8_t page,
                  const char* lastMsgSrc, const char* lastMsgText) {
    drawHeader(u8g2, u8g2PageTitle(page));
    switch (page) {
        case PAGE_NETWORK:   drawNetwork(u8g2);                          break;
        case PAGE_LORA_MESH: drawLoraMesh(u8g2);                         break;
        case PAGE_MESSAGES:  drawMessages(u8g2, lastMsgSrc, lastMsgText); break;
        case PAGE_SYSTEM:    drawSystem(u8g2);                           break;
        case PAGE_IDENTITY:
        default:             drawIdentity(u8g2, lastMsgSrc, lastMsgText); break;
    }
}

void u8g2DrawSplash(U8G2& u8g2) {
    u8g2.setFont(u8g2_font_logisoso22_tr);
    const char* title = "rMesh";
    uint8_t tw = u8g2.getStrWidth(title);
    u8g2.drawStr((128 - tw) / 2, 28, title);

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

    if (settings.mycall[0] != '\0') {
        u8g2.setFont(u8g2_font_6x10_tf);
        uint8_t cw = u8g2.getStrWidth(settings.mycall);
        u8g2.drawStr((128 - cw) / 2, 63, settings.mycall);
    }
}

void u8g2DrawFlashing(U8G2& u8g2, const char* what) {
    u8g2.setFont(u8g2_font_logisoso22_tr);
    const char* txt = "Flashing";
    uint8_t tw = u8g2.getStrWidth(txt);
    u8g2.drawStr((128 - tw) / 2, 26, txt);
    u8g2.setFont(u8g2_font_6x10_tf);
    const char* label = (what && what[0]) ? what : "...";
    uint8_t lw = u8g2.getStrWidth(label);
    u8g2.drawStr((128 - lw) / 2, 44, label);
    u8g2.setFont(u8g2_font_5x7_tf);
    const char* warn = "do not power off";
    uint8_t ww = u8g2.getStrWidth(warn);
    u8g2.drawStr((128 - ww) / 2, 62, warn);
}

#endif