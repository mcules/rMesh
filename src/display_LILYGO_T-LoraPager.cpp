/*
 * display_LILYGO_T-LoraPager.cpp
 *
 * Chat-Display + QWERTY-Keyboard + Einstellungsmenü + Gruppen für den LILYGO T-LoraPager.
 */

#ifdef LILYGO_T_LORA_PAGER

#include "display_LILYGO_T-LoraPager.h"
#include "settings.h"
#include "helperFunctions.h"
#include "config.h"
#include "main.h"
#include "frame.h"
#include "routing.h"
#include "peer.h"
#include "wifiFunctions.h"
#include "logging.h"

#include <LilyGoLib.h>
#include <WiFi.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ─── UTF-8 → CP437 conversion (LovyanGFX Font0 is CP437) ─────────────
static void utf8ToCP437(const char* src, char* dst, size_t dstLen) {
    size_t di = 0;
    const uint8_t* s = (const uint8_t*)src;
    while (*s && di < dstLen - 1) {
        if (*s < 0x80) {
            dst[di++] = (char)*s++;
        } else if ((*s & 0xE0) == 0xC0 && s[1]) {
            uint16_t cp = (uint16_t)((*s & 0x1F) << 6) | (s[1] & 0x3F);
            s += 2;
            switch (cp) {
                case 0x00C4: dst[di++] = (char)0x8E; break; // Ä
                case 0x00D6: dst[di++] = (char)0x99; break; // Ö
                case 0x00DC: dst[di++] = (char)0x9A; break; // Ü
                case 0x00E4: dst[di++] = (char)0x84; break; // ä
                case 0x00F6: dst[di++] = (char)0x94; break; // ö
                case 0x00FC: dst[di++] = (char)0x81; break; // ü
                case 0x00DF: dst[di++] = (char)0xE1; break; // ß
                case 0x00E9: dst[di++] = (char)0x82; break; // é
                case 0x00E8: dst[di++] = (char)0x8A; break; // è
                case 0x00E0: dst[di++] = (char)0x85; break; // à
                default:     dst[di++] = '?';         break;
            }
        } else if ((*s & 0xF0) == 0xE0 && s[1] && s[2]) {
            s += 3; dst[di++] = '?';
        } else if ((*s & 0xF8) == 0xF0 && s[1] && s[2] && s[3]) {
            s += 4; dst[di++] = '?';
        } else {
            s++;
        }
    }
    dst[di] = '\0';
}

// ─── Display constants ─────────────────────────────────────────────────
#define DISP_W        480
#define DISP_H        222
#define DISP_CS       38
#define DISP_DC       37
#define DISP_BL       42
#define LORA_SCK_PIN  35
#define LORA_MISO_PIN 33
#define LORA_MOSI_PIN 34

// Layout
#define GROUP_TAB_H   18          // header bar: callsign | group tabs | time + battery
#define INPUT_BAR_Y   200         // 18 header + 182 msg = 200
#define INPUT_BAR_H   22          // 222 - 200 = 22
#define MSG_AREA_Y    GROUP_TAB_H
#define MSG_AREA_H    (INPUT_BAR_Y - MSG_AREA_Y)  // 182 px (unchanged)

#define MAX_HISTORY    60
#define INPUT_MAX_LEN 200
#define MAX_GROUPS     10         // Maximum groups (practical upper limit)

// Menu layout
#define MENU_HDR_H     20
#define MENU_FOT_H     12
#define MENU_ITEM_H    22   // textSize 2 (16px) + 3px padding each side
#define MENU_AREA_H_   (DISP_H - MENU_HDR_H - MENU_FOT_H)
#define MENU_ITEMS_VIS (MENU_AREA_H_ / MENU_ITEM_H)

// RGB565 colors (chat)
#define COL_BG        0x0000u
#define COL_SEPARATOR 0x4A49u
#define COL_OWN       0x07E0u
#define COL_RX        0xFFE0u
#define COL_INPUT_BG  0x1082u
#define COL_INPUT_FG  0xFFFFu
#define COL_CURSOR    0xF800u
#define COL_STATUS    0x632Cu

// RGB565 colors (menu)
#define COL_MENU_HDR     0x2104u
#define COL_MENU_HDR_FG  0xFFFFu
#define COL_MENU_SEL     0x1294u
#define COL_MENU_SEL_FG  0xFFFFu
#define COL_MENU_FG      0xCE79u
#define COL_MENU_VAL     0x07FFu
#define COL_MENU_FOT     0x2104u
#define COL_MENU_FOT_FG  0x8410u
#define COL_MENU_EDIT_BG 0x18C3u
#define COL_UNREAD_BG    0x8400u   // dark red for groups with unread messages
#define COL_UNREAD_FG    0xFFE0u   // gelb
#define COL_DELETE_BG    0x6000u   // dark red for delete button
#define COL_DELETE_FG    0xFFFFu

// ─── LovyanGFX panel ──────────────────────────────────────────────────
class LGFX_Pager : public lgfx::LGFX_Device {
    lgfx::Panel_ST7796 _panel;
    lgfx::Bus_SPI      _bus;
public:
    LGFX_Pager() {
        {
            auto cfg          = _bus.config();
            cfg.spi_host      = SPI2_HOST;
            cfg.spi_mode      = 0;
            cfg.freq_write    = 40000000;
            cfg.freq_read     = 16000000;
            cfg.spi_3wire     = false;
            cfg.use_lock      = true;
            cfg.dma_channel   = SPI_DMA_CH_AUTO;
            cfg.pin_sclk      = LORA_SCK_PIN;
            cfg.pin_miso      = LORA_MISO_PIN;
            cfg.pin_mosi      = LORA_MOSI_PIN;
            cfg.pin_dc        = DISP_DC;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg              = _panel.config();
            cfg.pin_cs            = DISP_CS;
            cfg.pin_rst           = -1;
            cfg.pin_busy          = -1;
            cfg.panel_width       = 222;
            cfg.panel_height      = 480;
            cfg.memory_width      = 320;
            cfg.memory_height     = 480;
            cfg.offset_x          = 49;
            cfg.offset_y          = 0;
            cfg.offset_rotation   = 0;
            cfg.readable          = false;
            cfg.invert            = true;
            cfg.rgb_order         = false;
            cfg.dlen_16bit        = false;
            cfg.bus_shared        = true;
            _panel.config(cfg);
        }
        setPanel(&_panel);
    }
};

// ─── Menu types ───────────────────────────────────────────────────────
enum UiMode {
    UI_CHAT, UI_MENU_TOP, UI_MENU_LIST,
    UI_EDIT_STR, UI_EDIT_NUM, UI_EDIT_DROP,
    UI_ROUTING, UI_PEERS, UI_MONITOR, UI_ABOUT, UI_CONFIRM_POWER,
};

enum FieldType {
    FTYPE_BOOL, FTYPE_STRING, FTYPE_IP, FTYPE_FLOAT,
    FTYPE_INT8, FTYPE_INT16, FTYPE_UINT8, FTYPE_HEX8,
    FTYPE_DROP_F, FTYPE_DROP_I, FTYPE_READONLY, FTYPE_READONLY_STR, FTYPE_ACTION,
    FTYPE_DELETE_GROUP,   // delete group, aux = group index
    FTYPE_TOGGLE_MUTE,    // toggle mute,   aux = group index
    FTYPE_TOGGLE_INSAM,   // toggle inSammel, aux = group index
    FTYPE_SET_SAMMEL,     // set/unset as collection group, aux = group index
};

struct DropF { const char* label; float v; };
struct DropI { const char* label; int   v; };

struct MenuItem {
    const char*  label;
    FieldType    type;
    void*        ptr;
    int          aux;
    const void*  opts;
    const char*  unit;
    float        step;
    float        minVal;
    float        maxVal;
    void       (*action)();
};

// ─── Display settings ──────────────────────────────────────────────────
static float   dispBrightness = 200.0f;
static float   dispTextSize   = 1.0f;
static char    setupChipId[13] = {0};

// ─── Groups ───────────────────────────────────────────────────────────
static int  groupCount  = 0;
static int  groupUnread[MAX_GROUPS]  = {0};
static bool groupMute[MAX_GROUPS]    = {false};
static bool groupInSammel[MAX_GROUPS]= {false};
static int  sammelGroupIdx           = -1;   // Index of collection group, -1 = none
static int  activeGroup = -1;   // -1 = "All"

// ─── Forward declarations ──────────────────────────────────────────────
static void doSave();
static void doDeleteMessages();
static void doSaveDisplay();
static void doSaveSetup();
static void doReboot();
static void doUpdate();
static void doUpdate();
static void doForceUpdateRelease();
static void doForceUpdateDev();
static void doSaveGroups();
static void doNewGroup();
static void doAnnounce();
static void buildGroupMenu();
static void deleteGroup(int idx);

// ─── Dropdown options ─────────────────────────────────────────────────
static const DropF bwOpts[] = {
    {"7 kHz",      7.0f}, {"10,4 kHz",  10.4f}, {"15,6 kHz",  15.6f},
    {"20,8 kHz",  20.8f}, {"31,25 kHz", 31.25f}, {"62,5 kHz",  62.5f},
    {"125 kHz",   125.0f}, {"250 kHz",   250.0f}, {"500 kHz",   500.0f},
};
static const DropI crOpts[] = {
    {"5 - Standard", 5}, {"6 - Erhöhter Schutz", 6},
    {"7 - Hoher Schutz", 7}, {"8 - Maximaler Schutz", 8},
};
static const DropI sfOpts[] = {
    {"6 - Gering (Sichtverbindung)", 6}, {"7 - Gut (Standard)", 7},
    {"8 - Besser", 8}, {"9 - Sehr gut", 9}, {"10 - Exzellent", 10},
    {"11 - Maximum", 11}, {"12 - Deep Indoor", 12},
};

// ─── IP helper functions ───────────────────────────────────────────────
static char tmpPeerIP[5][16];
static void ipToStr(IPAddress& ip, char* buf, int len) {
    snprintf(buf, len, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}
static void strToIP(const char* s, IPAddress& ip) {
    unsigned a=0,b=0,c=0,d=0;
    sscanf(s, "%u.%u.%u.%u", &a, &b, &c, &d);
    ip = IPAddress(a,b,c,d);
}

// ─── Menu arrays ──────────────────────────────────────────────────────
static MenuItem netItems[] = {
    {"AP Mode",     FTYPE_BOOL,   &settings.apMode,       0, nullptr, nullptr,  0.f, 0.f, 0.f, nullptr},
    {"SSID",        FTYPE_STRING, settings.wifiSSID,     63, nullptr, nullptr,  0.f, 0.f, 0.f, nullptr},
    {"Password",    FTYPE_STRING, settings.wifiPassword, 63, nullptr, nullptr,  0.f, 0.f, 0.f, nullptr},
    {"DHCP",        FTYPE_BOOL,   &settings.dhcpActive,   0, nullptr, nullptr,  0.f, 0.f, 0.f, nullptr},
    {"UDP Peer 1",  FTYPE_IP,     tmpPeerIP[0],          15, nullptr, nullptr,  0.f, 0.f, 0.f, nullptr},
    {"UDP Peer 2",  FTYPE_IP,     tmpPeerIP[1],          15, nullptr, nullptr,  0.f, 0.f, 0.f, nullptr},
    {"UDP Peer 3",  FTYPE_IP,     tmpPeerIP[2],          15, nullptr, nullptr,  0.f, 0.f, 0.f, nullptr},
    {"UDP Peer 4",  FTYPE_IP,     tmpPeerIP[3],          15, nullptr, nullptr,  0.f, 0.f, 0.f, nullptr},
    {"UDP Peer 5",  FTYPE_IP,     tmpPeerIP[4],          15, nullptr, nullptr,  0.f, 0.f, 0.f, nullptr},
    {"NTP Server",  FTYPE_STRING, settings.ntpServer,    63, nullptr, nullptr,  0.f, 0.f, 0.f, nullptr},
    {"Speichern",   FTYPE_ACTION, nullptr,                0, nullptr, nullptr,  0.f, 0.f, 0.f, doSave},
};
static MenuItem loraItems[] = {
    {"Frequenz",           FTYPE_FLOAT,   &settings.loraFrequency,       0, nullptr, "MHz",    0.01f, 0.f,  0.f, nullptr},
    {"Sendeleistung",      FTYPE_INT8,    &settings.loraOutputPower,     0, nullptr, "dBm",     1.0f, 0.f,  0.f, nullptr},
    {"Bandbreite",         FTYPE_DROP_F,  &settings.loraBandwidth,       9, bwOpts,  nullptr,   0.f, 0.f,  0.f, nullptr},
    {"Coding Rate",        FTYPE_DROP_I,  &settings.loraCodingRate,      4, crOpts,  nullptr,   0.f, 0.f,  0.f, nullptr},
    {"Spreading Factor",   FTYPE_DROP_I,  &settings.loraSpreadingFactor, 7, sfOpts,  nullptr,   0.f, 0.f,  0.f, nullptr},
    {"Sync Word",          FTYPE_HEX8,    &settings.loraSyncWord,        0, nullptr, nullptr,   0.f, 0.f,  0.f, nullptr},
    {"Preamble Laenge",    FTYPE_INT16,   &settings.loraPreambleLength,  0, nullptr, nullptr,   1.f, 0.f,  0.f, nullptr},
    {"Nachr. wiederholen", FTYPE_BOOL,    &settings.loraRepeat,          0, nullptr, nullptr,   0.f, 0.f,  0.f, nullptr},
    {"Max. Nachr.-Laenge", FTYPE_READONLY,&settings.loraMaxMessageLength,0, nullptr, "Zeichen", 0.f, 0.f,  0.f, nullptr},
    {"Speichern",          FTYPE_ACTION,  nullptr,                       0, nullptr, nullptr,   0.f, 0.f,  0.f, doSave},
};
static MenuItem setupItems[] = {
    {"Rufzeichen",         FTYPE_STRING,       settings.mycall,     16, nullptr, nullptr, 0.f, 0.f, 0.f, nullptr},
    {"Position",           FTYPE_STRING,       settings.position,   23, nullptr, nullptr, 0.f, 0.f, 0.f, nullptr},
    {"Helligkeit",         FTYPE_FLOAT,        &dispBrightness,      0, nullptr, nullptr, 5.0f, 5.f, 255.f, nullptr},
    {"Schriftgroesse",     FTYPE_FLOAT,        &dispTextSize,        0, nullptr, nullptr, 0.1f, 0.5f, 4.f, nullptr},
    {"Chip ID",            FTYPE_READONLY_STR, setupChipId,          0, nullptr, nullptr, 0.f, 0.f, 0.f, nullptr},
    {"Speichern",          FTYPE_ACTION,       nullptr,              0, nullptr, nullptr, 0.f, 0.f, 0.f, doSaveSetup},
    {"Neustart",           FTYPE_ACTION,       nullptr,              0, nullptr, nullptr, 0.f, 0.f, 0.f, doReboot},
    {"Update",             FTYPE_ACTION,       nullptr,              0, nullptr, nullptr, 0.f, 0.f, 0.f, doUpdate},
    {"Update Release",     FTYPE_ACTION,       nullptr,              0, nullptr, nullptr, 0.f, 0.f, 0.f, doForceUpdateRelease},
    {"Update Dev",         FTYPE_ACTION,       nullptr,              0, nullptr, nullptr, 0.f, 0.f, 0.f, doForceUpdateDev},
    {"Nachr. loeschen",    FTYPE_ACTION,       nullptr,              0, nullptr, nullptr, 0.f, 0.f, 0.f, doDeleteMessages},
};

// Dynamically built group menu
// Per group: [Name] + [Delete] + [Mute] + [Collection/InCollection]
// At the end: [+ New Group] + [Save]
static MenuItem groupItemsBuf[MAX_GROUPS * 4 + 2];  // 4 items per group
static char     groupLabelBufs[MAX_GROUPS][16];
static char     groupMuteLbls [MAX_GROUPS][16];
static char     groupSamLbls  [MAX_GROUPS][16];
static int      groupItemsLen = 0;

// For "New group" – which slot is currently being created
static int newGroupSlot = -1;

// ─── Global objects (chat) ────────────────────────────────────────────
static LGFX_Pager        lcd;
static lgfx::LGFX_Sprite spr(&lcd);  // off-screen buffer for flicker-free menu

// Wrapper: UTF-8 → CP437, draw to lcd
static void drawStr(const char* str, int32_t x, int32_t y) {
    char buf[300];
    utf8ToCP437(str, buf, sizeof(buf));
    lcd.drawString(buf, x, y);
}

// Wrapper: UTF-8 → CP437, draw to sprite
static void drawStrS(const char* str, int32_t x, int32_t y) {
    char buf[300];
    utf8ToCP437(str, buf, sizeof(buf));
    spr.drawString(buf, x, y);
}

// Push sprite to display (single DMA transfer = no flicker)
static void sprPush() {
    instance.lockSPI();
    lcd.startWrite();
    spr.pushSprite(0, 0);
    lcd.endWrite();
    instance.unlockSPI();
}

struct ChatLine {
    char call[MAX_CALLSIGN_LENGTH + 8];
    char dst[MAX_CALLSIGN_LENGTH + 1];   // Frame's dstCall, "" = broadcast/direct
    char text[INPUT_MAX_LEN + 1];
    char group[MAX_CALLSIGN_LENGTH + 1];
    bool own;
};

static ChatLine history[MAX_HISTORY];
static int      historyCount = 0;
static int      historyHead  = 0;

static char     inputBuf[INPUT_MAX_LEN + 1] = {0};
static int      inputLen = 0;
static bool     needRedraw = true;

// Battery state (refreshed every 10 s to avoid I2C overhead each frame)
static int      battPct    = -1;   // -1 = not yet read
static bool     battCharging = false;
static uint32_t lastBattMs = 0;
static int      lastMinute = -1;   // triggers header redraw when minute changes

// ─── Menu state ───────────────────────────────────────────────────────
static UiMode    uiMode      = UI_CHAT;
static int       topSel      = 0;
static int       topScroll   = 0;
static int       listSel     = 0;
static int       listScroll  = 0;
static MenuItem* curMenu     = nullptr;
static int       curMenuLen  = 0;
static int       editItemIdx = -1;

static char      editStrBuf[65] = {0};
static float     editFloat      = 0.0f;
static int       editDropIdx    = 0;

#ifndef ROTARY_C
  #define ROTARY_C 7
#endif
static bool      prevBtn       = false;
static uint32_t  btnDownMs     = 0;
static bool      longHandled   = false;

// Chat sub-modes: group switching (default) or message scrolling
static bool      chatScrollMode = false;
static int       chatScrollOff  = 0;  // messages hidden at bottom (0 = newest visible)

static int       infoScroll    = 0;    // scroll offset for info views

// Monitor ring buffer
#define MON_HISTORY   80
#define MON_LINE_W    72
static char      monLines[MON_HISTORY][MON_LINE_W];
static int       monHead     = 0;
static int       monCount    = 0;
static bool      monNewData  = false;

// ─── Group helper functions ────────────────────────────────────────────
static int buildTabList(int* tabList) {
    int count = 0;
    tabList[count++] = -1;  // "All"
    for (int i = 0; i < groupCount; i++) {
        // inSammel groups are not shown as a separate tab
        if (strlen(groupNames[i]) > 0 && !groupInSammel[i]) tabList[count++] = i;
    }
    return count;
}

static int currentTabIndex() {
    int tabList[MAX_GROUPS + 1];
    int n = buildTabList(tabList);
    for (int i = 0; i < n; i++) if (tabList[i] == activeGroup) return i;
    return 0;
}

// ─── Build dynamic group menu ─────────────────────────────────────────
static void buildGroupMenu() {
    groupItemsLen = 0;
    for (int i = 0; i < groupCount; i++) {
        snprintf(groupLabelBufs[i], sizeof(groupLabelBufs[i]), "Gruppe %d", i + 1);
        groupItemsBuf[groupItemsLen++] = {
            groupLabelBufs[i], FTYPE_STRING, groupNames[i], MAX_CALLSIGN_LENGTH,
            nullptr, nullptr, 0.f, 0.f, 0.f, nullptr
        };
        groupItemsBuf[groupItemsLen++] = {
            "Loeschen", FTYPE_DELETE_GROUP, nullptr, i,
            nullptr, nullptr, 0.f, 0.f, 0.f, nullptr
        };
        snprintf(groupMuteLbls[i], sizeof(groupMuteLbls[i]), groupMute[i] ? "Laut" : "Stumm");
        groupItemsBuf[groupItemsLen++] = {
            groupMuteLbls[i], FTYPE_TOGGLE_MUTE, nullptr, i,
            nullptr, nullptr, 0.f, 0.f, 0.f, nullptr
        };
        if (sammelGroupIdx == i) {
            snprintf(groupSamLbls[i], sizeof(groupSamLbls[i]), "Sam.aufheben");
        } else if (groupInSammel[i]) {
            snprintf(groupSamLbls[i], sizeof(groupSamLbls[i]), "ausSammel");
        } else {
            snprintf(groupSamLbls[i], sizeof(groupSamLbls[i]),
                     sammelGroupIdx >= 0 ? "->Sammel" : "AlsSammel");
        }
        groupItemsBuf[groupItemsLen++] = {
            groupSamLbls[i], FTYPE_TOGGLE_INSAM, nullptr, i,
            nullptr, nullptr, 0.f, 0.f, 0.f, nullptr
        };
    }
    if (groupCount < MAX_GROUPS) {
        groupItemsBuf[groupItemsLen++] = {
            "+ Neue Gruppe", FTYPE_ACTION, nullptr, 0,
            nullptr, nullptr, 0.f, 0.f, 0.f, doNewGroup
        };
    }
    groupItemsBuf[groupItemsLen++] = {
        "Speichern", FTYPE_ACTION, nullptr, 0,
        nullptr, nullptr, 0.f, 0.f, 0.f, doSaveGroups
    };
}

static void deleteGroup(int idx) {
    if (idx < 0 || idx >= groupCount) return;
    // Shift down
    for (int i = idx; i < groupCount - 1; i++) {
        strncpy(groupNames[i], groupNames[i + 1], MAX_CALLSIGN_LENGTH);
        groupUnread[i]   = groupUnread[i + 1];
        groupMute[i]     = groupMute[i + 1];
        groupInSammel[i] = groupInSammel[i + 1];
    }
    groupCount--;
    groupNames[groupCount][0] = '\0';
    groupUnread[groupCount]   = 0;
    groupMute[groupCount]     = false;
    groupInSammel[groupCount] = false;
    // Adjust sammelGroupIdx
    if (sammelGroupIdx == idx)       sammelGroupIdx = -1;
    else if (sammelGroupIdx > idx)   sammelGroupIdx--;
    // Adjust activeGroup
    if (activeGroup == idx)        activeGroup = -1;
    else if (activeGroup > idx)    activeGroup--;
    // Save immediately to NVS
    doSaveGroups();
    buildGroupMenu();
    curMenuLen = groupItemsLen;
    if (listSel >= curMenuLen) listSel = max(0, curMenuLen - 1);
    if (listSel < listScroll)  listScroll = listSel;
    needRedraw = true;
}

static void doNewGroup() {
    if (groupCount >= MAX_GROUPS) return;
    newGroupSlot = groupCount;
    groupNames[newGroupSlot][0] = '\0';
    groupUnread[newGroupSlot]   = 0;
    groupMute[newGroupSlot]     = false;
    groupInSammel[newGroupSlot] = false;
    groupCount++;
    buildGroupMenu();
    curMenuLen = groupItemsLen;
    // The name entry for the new slot is at position newGroupSlot*2
    editItemIdx = newGroupSlot * 4;
    editStrBuf[0] = '\0';
    uiMode = UI_EDIT_STR;
    needRedraw = true;
}

// ─── Ring buffer ──────────────────────────────────────────────────────
static void addLine(const char* call, const char* text, bool own, const char* grp, const char* dst = "") {
    int idx = historyHead % MAX_HISTORY;
    strncpy(history[idx].call,  call, sizeof(history[idx].call) - 1);
    strncpy(history[idx].dst,   dst,  MAX_CALLSIGN_LENGTH);
    strncpy(history[idx].text,  text, INPUT_MAX_LEN);
    strncpy(history[idx].group, grp,  MAX_CALLSIGN_LENGTH);
    history[idx].own = own;
    historyHead++;
    if (historyCount < MAX_HISTORY) historyCount++;
}

static ChatLine* getLine(int i) {
    if (historyCount == 0) return nullptr;
    int offset = (historyHead - historyCount + i) % MAX_HISTORY;
    if (offset < 0) offset += MAX_HISTORY;
    return &history[offset];
}

static void fmtAge(time_t t, char* buf, size_t bufLen) {
    time_t age = time(NULL) - t;
    if (age < 0) age = 0;
    if      (age < 60)    snprintf(buf, bufLen, "%llds",  (long long)age);
    else if (age < 3600)  snprintf(buf, bufLen, "%lldm",  (long long)(age / 60));
    else if (age < 86400) snprintf(buf, bufLen, "%lldh",  (long long)(age / 3600));
    else                  snprintf(buf, bufLen, "%lldd",  (long long)(age / 86400));
}

// ─── Group tab strip (combined header: callsign | tabs | time + battery) ──
static void drawGroupTabs() {
    // ── Left zone: callsign ─────────────────────────────────────────────
    int callLen = min((int)strlen(settings.mycall), 8);
    int CALL_W  = (callLen > 0) ? callLen * 6 + 8 : 0;

    // ── Right zone: time + battery ──────────────────────────────────────
    char timeStr[6] = {0};
    char battStr[8] = {0};
    time_t now = time(NULL);
    struct tm* tm_ = localtime(&now);
    if (tm_) strftime(timeStr, sizeof(timeStr), "%H:%M", tm_);
    if (battPct >= 0)
        snprintf(battStr, sizeof(battStr), battCharging ? "%d%%+" : "%d%%", battPct);
    // Compute actual right-zone pixel width
    int timeW = timeStr[0] ? (int)strlen(timeStr) * 6 + 4 : 0;
    int battW = battStr[0] ? (int)strlen(battStr) * 6 + 4 : 0;
    int RIGHT_W = timeW + battW + 2;  // +2 right margin

    // ── Tab area (middle) ───────────────────────────────────────────────
    int tabAreaX = CALL_W;
    int tabAreaW      = DISP_W - CALL_W - RIGHT_W;
    int tabList[MAX_GROUPS + 1];
    int tabCount = buildTabList(tabList);
    int tabW = (tabCount > 0) ? tabAreaW / tabCount : tabAreaW;

    // ── Background ──────────────────────────────────────────────────────
    lcd.fillRect(0, 0, DISP_W, GROUP_TAB_H, COL_MENU_HDR);
    lcd.setTextSize(1);

    // ── Callsign (left) ─────────────────────────────────────────────────
    if (CALL_W > 0) {
        char callBuf[10];
        strncpy(callBuf, settings.mycall, 8); callBuf[8] = '\0';
        lcd.setTextColor(0x07FFu);  // cyan
        drawStr(callBuf, 4, 5);
        lcd.drawFastVLine(CALL_W - 1, 0, GROUP_TAB_H, COL_SEPARATOR);
    }

    // ── Group tabs (center) ─────────────────────────────────────────────
    for (int i = 0; i < tabCount; i++) {
        int x = tabAreaX + i * tabW;
        int g = tabList[i];
        bool active    = (g == activeGroup);
        bool hasUnread = (g >= 0 && groupUnread[g] > 0);

        uint16_t bg = active ? COL_MENU_SEL : (hasUnread ? COL_UNREAD_BG : COL_MENU_HDR);
        lcd.fillRect(x, 0, tabW, GROUP_TAB_H, bg);

        const char* name = (g == -1) ? "Alle" : groupNames[g];
        lcd.setTextColor(active ? COL_MENU_SEL_FG : (hasUnread ? COL_UNREAD_FG : COL_MENU_FG));

        char tabLabel[16];
        if (hasUnread) snprintf(tabLabel, sizeof(tabLabel), "%s*", name);
        else           snprintf(tabLabel, sizeof(tabLabel), "%s",  name);

        int tw = (int)strlen(tabLabel) * 6;
        int tx = x + (tabW - tw) / 2;
        if (tx < x + 1) tx = x + 1;
        drawStr(tabLabel, tx, 5);

        if (i < tabCount - 1)
            lcd.drawFastVLine(x + tabW - 1, 0, GROUP_TAB_H, COL_SEPARATOR);
    }

    // ── Right zone: time + battery ──────────────────────────────────────
    if (RIGHT_W > 2) {
        lcd.drawFastVLine(DISP_W - RIGHT_W - 1, 0, GROUP_TAB_H, COL_SEPARATOR);
        int rx = DISP_W - RIGHT_W + 2;
        if (timeStr[0]) {
            lcd.setTextColor(COL_MENU_FG);
            drawStr(timeStr, rx, 5);
            rx += timeW;
        }
        if (battStr[0]) {
            uint16_t col = (battPct <= 15) ? 0xF800u
                         : (battPct <= 40) ? 0xFD20u
                         : 0x07E0u;
            lcd.setTextColor(col);
            drawStr(battStr, rx, 5);
        }
    }

    // ── Scroll-mode border: green frame around active scroll target ──────
    if (!chatScrollMode)
        lcd.drawRect(0, 0, DISP_W, GROUP_TAB_H, COL_MENU_SEL);  // group tabs active
}

// ─── Info view helper ─────────────────────────────────────────────────
// Draws header + footer + scrollbar. Content area starts at MENU_HDR_H+colHdrH.
static void infoHeader(const char* title) {
    spr.fillScreen(COL_BG);
    spr.fillRect(0, 0, DISP_W, MENU_HDR_H, COL_MENU_HDR);
    spr.setFont(&fonts::FreeSans9pt7b);
    spr.setTextSize(1);
    spr.setTextColor(COL_MENU_HDR_FG);
    char hdr[48];
    snprintf(hdr, sizeof(hdr), "rMesh > %s", title);
    drawStrS(hdr, 4, 2);
}
static void infoFooter(int total, int vis, int scroll) {
    int fy = DISP_H - MENU_FOT_H;
    spr.fillRect(0, fy, DISP_W, MENU_FOT_H, COL_MENU_FOT);
    spr.setFont(&fonts::Font0);
    spr.setTextSize(1);
    spr.setTextColor(COL_MENU_FOT_FG);
    drawStrS("Drehen=Scr  Druecken=Zurueck", 4, fy + 1);
    if (total > vis) {
        int sbH = max(4, MENU_AREA_H_ * vis / total);
        int sbY = MENU_HDR_H + MENU_AREA_H_ * scroll / total;
        spr.fillRect(DISP_W - 3, sbY, 3, sbH, COL_MENU_HDR_FG);
    }
}

// ─── Routing view ─────────────────────────────────────────────────────
static void drawRouting() {
    infoHeader("Routing");
    const int colHdrH = 20;
    const int areaY   = MENU_HDR_H + colHdrH;
    const int areaH   = DISP_H - MENU_FOT_H - areaY;
    const int lineH   = MENU_ITEM_H;
    const int vis     = areaH / lineH;

    // Column header
    spr.setFont(&fonts::FreeSans9pt7b);
    spr.setTextSize(1);
    spr.setTextColor(0x7BEFu);  // dim gray
    drawStrS("Rufzeichen  Via          H  Alter", 4, MENU_HDR_H + 2);
    spr.drawFastHLine(0, MENU_HDR_H + colHdrH - 1, DISP_W, COL_SEPARATOR);

    int n = (int)routingList.size();
    int start = max(0, min(infoScroll, n > vis ? n - vis : 0));
    if (n == 0) {
        spr.setFont(&fonts::FreeSans9pt7b);
        spr.setTextColor(COL_MENU_FG);
        drawStrS("(keine Eintraege)", 4, areaY + areaH / 2 - 8);
    }
    for (int i = start; i < n && (i - start) < vis; i++) {
        const Route& r = routingList[i];
        char age[10];
        fmtAge(r.timestamp, age, sizeof(age));
        char line[MON_LINE_W];
        snprintf(line, sizeof(line), "%-11s %-12s %2d  %s",
                 r.srcCall, r.viaCall, r.hopCount, age);
        spr.setFont(&fonts::FreeMono9pt7b);
        spr.setTextSize(1);
        spr.setTextColor(COL_MENU_FG);
        drawStrS(line, 4, areaY + (i - start) * lineH + 3);
    }
    infoFooter(max(1, n), vis, start);
    sprPush();
}

// ─── Peers view ───────────────────────────────────────────────────────
static void drawPeers() {
    infoHeader("Peers");
    const int colHdrH = 20;
    const int areaY   = MENU_HDR_H + colHdrH;
    const int areaH   = DISP_H - MENU_FOT_H - areaY;
    const int lineH   = MENU_ITEM_H;
    const int vis     = areaH / lineH;

    // Column header
    spr.setFont(&fonts::FreeSans9pt7b);
    spr.setTextSize(1);
    spr.setTextColor(0x7BEFu);
    drawStrS("Rufzeichen  Typ  RSSI   SNR  Alter", 4, MENU_HDR_H + 2);
    spr.drawFastHLine(0, MENU_HDR_H + colHdrH - 1, DISP_W, COL_SEPARATOR);

    int n = (int)peerList.size();
    int start = max(0, min(infoScroll, n > vis ? n - vis : 0));
    if (n == 0) {
        spr.setFont(&fonts::FreeSans9pt7b);
        spr.setTextColor(COL_MENU_FG);
        drawStrS("(keine Eintraege)", 4, areaY + areaH / 2 - 8);
    }
    for (int i = start; i < n && (i - start) < vis; i++) {
        const Peer& p = peerList[i];
        char age[10];
        fmtAge(p.timestamp, age, sizeof(age));
        char line[MON_LINE_W];
        snprintf(line, sizeof(line), "%-10s %-4s %6.1f %5.1f %s",
                 p.nodeCall,
                 p.port == 0 ? "LoRa" : "WiFi",
                 p.rssi, p.snr,
                 age);
        spr.setFont(&fonts::FreeMono9pt7b);
        spr.setTextSize(1);
        spr.setTextColor(p.available ? 0x07E0u : 0xAD55u);  // green or gray
        drawStrS(line, 4, areaY + (i - start) * lineH + 3);
    }
    infoFooter(max(1, n), vis, start);
    sprPush();
}

// ─── Monitor view ─────────────────────────────────────────────────────
static void drawMonitor() {
    infoHeader("Monitor");
    const int areaY = MENU_HDR_H;
    const int areaH = DISP_H - MENU_FOT_H - areaY;
    const int lineH = MENU_ITEM_H;
    const int vis   = areaH / lineH;

    // infoScroll=0 → newest at bottom; positive → scrolled up
    int clampedScroll = max(0, min(infoScroll, monCount > vis ? monCount - vis : 0));
    int visEnd  = monCount - clampedScroll;
    int start   = max(0, visEnd - vis);

    spr.setFont(&fonts::FreeMono9pt7b);
    spr.setTextSize(1);
    for (int i = start; i < visEnd; i++) {
        int ri = (monHead - monCount + i + MON_HISTORY) % MON_HISTORY;
        spr.setTextColor(COL_MENU_FG);
        drawStrS(monLines[ri], 4, areaY + (i - start) * lineH + 3);
    }
    if (monCount == 0) {
        spr.setFont(&fonts::FreeSans9pt7b);
        spr.setTextColor(COL_MENU_FG);
        drawStrS("(kein Traffic)", 4, areaY + areaH / 2 - 8);
    }
    infoFooter(max(1, monCount), vis, clampedScroll);
    sprPush();
}

// ─── Chat drawing functions ───────────────────────────────────────────
#define MSG_GAP 3   // pixels of spacing between messages

static void drawMessages() {
    float ts         = dispTextSize;
    int lineH        = max(1, (int)(10.0f * ts + 0.5f));
    int charW        = max(1, (int)(6.0f  * ts + 0.5f));
    int charsPerLine = max(1, DISP_W / charW);

    instance.lockSPI();
    lcd.startWrite();
    drawGroupTabs();
    lcd.fillRect(0, MSG_AREA_Y, DISP_W, MSG_AREA_H, COL_BG);
    lcd.setTextSize(ts);

    // Collect matching chat lines
    int matches[MAX_HISTORY];
    int nMatches = 0;
    for (int i = 0; i < historyCount; i++) {
        ChatLine* l = getLine(i);
        if (!l) continue;
        bool show;
        if (activeGroup == -1) {
            // "All": hide messages addressed to known groups
            bool isKnownGroup = false;
            if (strlen(l->group) > 0) {
                for (int g = 0; g < groupCount; g++) {
                    if (strlen(groupNames[g]) > 0 && strcmp(l->group, groupNames[g]) == 0) {
                        isKnownGroup = true;
                        break;
                    }
                }
            }
            show = !isKnownGroup;
        } else {
            show = (strcmp(l->group, groupNames[activeGroup]) == 0);
        }
        if (show && nMatches < MAX_HISTORY) matches[nMatches++] = i;
    }

    // Pre-calculate wrapped line count per message to enable bottom-alignment
    char buf[290];
    int lineCounts[MAX_HISTORY] = {};
    for (int mi = 0; mi < nMatches; mi++) {
        ChatLine* l = getLine(matches[mi]);
        if (!l) { lineCounts[mi] = 1; continue; }
        if (activeGroup == -1 && strlen(l->dst) > 0)
            snprintf(buf, sizeof(buf), "<%s\xBB%s> %s", l->call, l->dst, l->text);
        else
            snprintf(buf, sizeof(buf), "<%s> %s", l->call, l->text);
        int len = (int)strlen(buf);
        lineCounts[mi] = max(1, (len + charsPerLine - 1) / charsPerLine);
    }

    // Clamp scroll offset and compute the visible end index
    if (chatScrollOff < 0) chatScrollOff = 0;
    if (chatScrollOff >= nMatches) chatScrollOff = (nMatches > 0) ? nMatches - 1 : 0;
    int visEnd = nMatches - chatScrollOff;  // newest visible message (exclusive)

    // Walk backwards from visEnd to find the first message that still fits
    int totalH     = 0;
    int startMatch = visEnd;
    for (int mi = visEnd - 1; mi >= 0; mi--) {
        int h = lineCounts[mi] * lineH + MSG_GAP;
        if (totalH + h > MSG_AREA_H) break;
        totalH     += h;
        startMatch  = mi;
    }

    // Pin content to the bottom of the message area
    int y = MSG_AREA_Y + MSG_AREA_H - totalH;

    for (int mi = startMatch; mi < visEnd; mi++) {
        ChatLine* l = getLine(matches[mi]);
        if (!l) continue;
        lcd.setTextColor(l->own ? COL_OWN : COL_RX);
        if (activeGroup == -1 && strlen(l->dst) > 0)
            snprintf(buf, sizeof(buf), "<%s\xBB%s> %s", l->call, l->dst, l->text);
        else
            snprintf(buf, sizeof(buf), "<%s> %s", l->call, l->text);
        int len = (int)strlen(buf), pos = 0;
        while (pos < len && y < MSG_AREA_Y + MSG_AREA_H) {
            int take = min(len - pos, charsPerLine);
            char tmp[82];
            memcpy(tmp, buf + pos, take);
            tmp[take] = '\0';
            drawStr(tmp, 0, y);
            pos += take;
            y   += lineH;
        }
        y += MSG_GAP;
    }

    // Green border around the area that the encoder currently scrolls
    if (chatScrollMode)
        lcd.drawRect(0, MSG_AREA_Y, DISP_W, MSG_AREA_H, COL_MENU_SEL);  // message area active

    lcd.endWrite();
    instance.unlockSPI();
}

static void drawInputBar() {
    instance.lockSPI();
    lcd.startWrite();
    lcd.fillRect(0, INPUT_BAR_Y, DISP_W, INPUT_BAR_H, COL_INPUT_BG);
    lcd.drawFastHLine(0, INPUT_BAR_Y, DISP_W, COL_SEPARATOR);
    lcd.setTextSize(1);

    // Input line (battery is now shown in the header bar)
    lcd.setTextColor(COL_INPUT_FG);
    char prompt[16];
    if (activeGroup >= 0 && strlen(groupNames[activeGroup]) > 0)
        snprintf(prompt, sizeof(prompt), "[%s]>", groupNames[activeGroup]);
    else
        snprintf(prompt, sizeof(prompt), ">");
    char dispLine[INPUT_MAX_LEN + 20];
    snprintf(dispLine, sizeof(dispLine), "%s%s", prompt, inputBuf);
    drawStr(dispLine, 2, INPUT_BAR_Y + 7);
    int cx = 2 + ((int)strlen(prompt) + inputLen) * 6;
    if (cx < DISP_W - 6) lcd.drawFastVLine(cx, INPUT_BAR_Y + 6, 9, COL_CURSOR);

    lcd.endWrite();
    instance.unlockSPI();
}

static void redraw() {
    drawMessages();
    drawInputBar();
    needRedraw = false;
}

// ─── Menu: value formatting ───────────────────────────────────────────
static void fmtValue(char* buf, int buflen, MenuItem& item) {
    switch (item.type) {
        case FTYPE_BOOL:
            snprintf(buf, buflen, "%s", *(bool*)item.ptr ? "[X]" : "[ ]"); break;
        case FTYPE_STRING: case FTYPE_IP:
            snprintf(buf, buflen, "%s", (char*)item.ptr); break;
        case FTYPE_FLOAT: {
            int dec = (item.step >= 1.0f) ? 0 : (fabsf(item.step - 0.1f) < 0.05f) ? 1 : 3;
            char fmt[12]; snprintf(fmt, sizeof(fmt), "%%.%df", dec);
            char num[24]; snprintf(num, sizeof(num), fmt, *(float*)item.ptr);
            if (item.unit) snprintf(buf, buflen, "%s %s", num, item.unit);
            else           snprintf(buf, buflen, "%s", num);
            break;
        }
        case FTYPE_INT8:
            if (item.unit) snprintf(buf, buflen, "%d %s", (int)*(int8_t*)item.ptr, item.unit);
            else           snprintf(buf, buflen, "%d", (int)*(int8_t*)item.ptr); break;
        case FTYPE_INT16:
            if (item.unit) snprintf(buf, buflen, "%d %s", (int)*(int16_t*)item.ptr, item.unit);
            else           snprintf(buf, buflen, "%d", (int)*(int16_t*)item.ptr); break;
        case FTYPE_UINT8:
            if (item.unit) snprintf(buf, buflen, "%u %s", (unsigned)*(uint8_t*)item.ptr, item.unit);
            else           snprintf(buf, buflen, "%u", (unsigned)*(uint8_t*)item.ptr); break;
        case FTYPE_HEX8:
            snprintf(buf, buflen, "%02X", *(uint8_t*)item.ptr); break;
        case FTYPE_DROP_F: {
            float fv = *(float*)item.ptr;
            const DropF* o = (const DropF*)item.opts;
            for (int i = 0; i < item.aux; i++)
                if (fabsf(o[i].v - fv) < 0.01f) { snprintf(buf, buflen, "%s", o[i].label); return; }
            snprintf(buf, buflen, "%.2f", fv); break;
        }
        case FTYPE_DROP_I: {
            int iv = (int)*(uint8_t*)item.ptr;
            const DropI* o = (const DropI*)item.opts;
            for (int i = 0; i < item.aux; i++)
                if (o[i].v == iv) { snprintf(buf, buflen, "%s", o[i].label); return; }
            snprintf(buf, buflen, "%d", iv); break;
        }
        case FTYPE_READONLY:
            if (item.unit) snprintf(buf, buflen, "%u %s", (unsigned)*(uint8_t*)item.ptr, item.unit);
            else           snprintf(buf, buflen, "%u", (unsigned)*(uint8_t*)item.ptr); break;
        case FTYPE_READONLY_STR:
            snprintf(buf, buflen, "%s", (char*)item.ptr); break;
        case FTYPE_DELETE_GROUP:
        default: buf[0] = 0; break;
    }
}

// ─── Menu: top level (scrollable list, textSize 2) ────────────────────
#define TOP_ITEM_H  26   // px per row — textSize-2 glyph is 16px high
#define TOP_MENU_N  11
#define TOP_MENU_VIS ((DISP_H - MENU_HDR_H - MENU_FOT_H) / TOP_ITEM_H)  // 7

// Draw a 14×14 pixel icon at (ix, iy) for the given top-menu entry index
static void drawMenuIcon(int idx, int ix, int iy, uint16_t col, uint16_t bg) {
    switch (idx) {
        case 0: // Network – signal bars
            spr.fillRect(ix+1,  iy+10, 3,  4, col);
            spr.fillRect(ix+5,  iy+7,  3,  7, col);
            spr.fillRect(ix+9,  iy+4,  3, 10, col);
            break;
        case 1: // LoRa – signal arcs (WiFi-style)
            spr.fillCircle(ix+7, iy+11, 1, col);
            spr.drawArc(ix+7, iy+11, 3, 2, 225, 315, col);
            spr.drawArc(ix+7, iy+11, 5, 4, 225, 315, col);
            spr.drawArc(ix+7, iy+11, 7, 6, 225, 315, col);
            break;
        case 2: // Setup – gear (circle with 4 teeth)
            spr.fillCircle(ix+7, iy+7, 5, col);
            spr.fillCircle(ix+7, iy+7, 2, bg);
            spr.fillRect(ix+6, iy+0,  2, 3, col);
            spr.fillRect(ix+6, iy+11, 2, 3, col);
            spr.fillRect(ix+11, iy+6, 3, 2, col);
            spr.fillRect(ix+0,  iy+6, 3, 2, col);
            break;
        case 3: // Groups – two people
            spr.fillCircle(ix+4,  iy+3, 2, col);
            spr.fillRect(ix+2,   iy+6, 4, 6, col);
            spr.fillCircle(ix+10, iy+3, 2, col);
            spr.fillRect(ix+8,   iy+6, 4, 6, col);
            break;
        case 4: // Routing – branching tree
            spr.fillCircle(ix+2,  iy+7,  2, col);
            spr.drawLine(ix+2, iy+7, ix+11, iy+3,  col);
            spr.fillCircle(ix+11, iy+3,  2, col);
            spr.drawLine(ix+2, iy+7, ix+11, iy+11, col);
            spr.fillCircle(ix+11, iy+11, 2, col);
            break;
        case 5: // Peers – mesh triangle
            spr.fillCircle(ix+7,  iy+2,  2, col);
            spr.fillCircle(ix+2,  iy+11, 2, col);
            spr.fillCircle(ix+12, iy+11, 2, col);
            spr.drawLine(ix+7,  iy+2,  ix+2,  iy+11, col);
            spr.drawLine(ix+7,  iy+2,  ix+12, iy+11, col);
            spr.drawLine(ix+2,  iy+11, ix+12, iy+11, col);
            break;
        case 6: // Monitor – screen with waveform
            spr.drawRect(ix+1, iy+2, 12, 8, col);
            spr.fillRect(ix+5, iy+10, 4,  2, col);
            spr.fillRect(ix+3, iy+12, 8,  2, col);
            spr.drawLine(ix+3,  iy+6, ix+5, iy+6, col);
            spr.drawLine(ix+5,  iy+4, ix+7, iy+8, col);
            spr.drawLine(ix+7,  iy+8, ix+9, iy+4, col);
            spr.drawLine(ix+9,  iy+6, ix+11, iy+6, col);
            break;
        case 7: // Send Announce – antenna with beams
            spr.drawFastVLine(ix+7, iy+5, 7, col);
            spr.fillRect(ix+5, iy+12, 5,  2, col);
            spr.drawLine(ix+7, iy+5, ix+3, iy+1,  col);
            spr.drawLine(ix+7, iy+5, ix+5, iy+2,  col);
            spr.drawLine(ix+7, iy+5, ix+9, iy+2,  col);
            spr.drawLine(ix+7, iy+5, ix+11, iy+1, col);
            break;
        case 8: // Tune – sine wave
            spr.drawLine(ix+1,  iy+7,  ix+4,  iy+3,  col);
            spr.drawLine(ix+4,  iy+3,  ix+7,  iy+7,  col);
            spr.drawLine(ix+7,  iy+7,  ix+10, iy+11, col);
            spr.drawLine(ix+10, iy+11, ix+13, iy+7,  col);
            break;
        case 9: // About – circle with "i"
            spr.drawCircle(ix+7, iy+7, 6, col);
            spr.fillCircle(ix+7, iy+4, 1, col);
            spr.fillRect(ix+6, iy+6, 2, 5, col);
            break;
        case 10: // Ausschalten – power symbol (arc + vertical line)
            spr.drawArc(ix+7, iy+8, 5, 4, 40, 320, col);
            spr.drawFastVLine(ix+7, iy+3, 6, col);
            break;
    }
}

static void drawMenuTop() {
    static const char* labels[TOP_MENU_N] = {
        "Network",
        "LoRa",
        "Setup",
        "Gruppen",
        "Routing",
        "Peers",
        "Monitor",
        "Send Announce",
        "Tune",
        "About",
        "Ausschalten"
    };
    const int areaH = DISP_H - MENU_HDR_H - MENU_FOT_H;

    spr.fillScreen(COL_BG);
    spr.fillRect(0, 0, DISP_W, MENU_HDR_H, COL_MENU_HDR);
    spr.setFont(&fonts::FreeSans9pt7b);
    spr.setTextSize(1);
    spr.setTextColor(COL_MENU_HDR_FG);
    drawStrS("rMesh  Einstellungen", 4, 2);

    int end = min(topScroll + TOP_MENU_VIS, TOP_MENU_N);
    for (int i = topScroll; i < end; i++) {
        int y   = MENU_HDR_H + (i - topScroll) * TOP_ITEM_H;
        bool sel = (i == topSel);
        if (sel) spr.fillRect(0, y, DISP_W, TOP_ITEM_H, COL_MENU_SEL);
        uint16_t icol = sel ? COL_MENU_SEL_FG : COL_MENU_FG;
        uint16_t ibg  = sel ? COL_MENU_SEL : COL_BG;
        drawMenuIcon(i, 6, y + 6, icol, ibg);
        spr.setFont(&fonts::FreeSans9pt7b);
        spr.setTextSize(1);
        spr.setTextColor(icol);
        drawStrS(labels[i], 26, y + (TOP_ITEM_H - 16) / 2);
        if (sel) drawStrS(">", DISP_W - 20, y + (TOP_ITEM_H - 16) / 2);
    }

    // Scrollbar (only when list overflows)
    if (TOP_MENU_N > TOP_MENU_VIS) {
        int sbH = max(4, areaH * TOP_MENU_VIS / TOP_MENU_N);
        int sbY = MENU_HDR_H + areaH * topScroll / TOP_MENU_N;
        spr.fillRect(DISP_W - 3, sbY, 3, sbH, COL_MENU_SEL);
    }

    int fy = DISP_H - MENU_FOT_H;
    spr.fillRect(0, fy, DISP_W, MENU_FOT_H, COL_MENU_FOT);
    spr.setTextSize(1);
    spr.setTextColor(COL_MENU_FOT_FG);
    drawStrS("Drehen=Wahl  Drucken=OK  Halten=Zuruck", 4, fy + 1);
    sprPush();
}

// ─── Menu: item list ──────────────────────────────────────────────────
static void drawMenuList() {
    const char* catNames[4] = {"Network", "LoRa", "Setup", "Gruppen"};
    spr.fillRect(0, 0, DISP_W, MENU_HDR_H, COL_MENU_HDR);
    spr.setFont(&fonts::FreeSans9pt7b);
    spr.setTextSize(1);
    spr.setTextColor(COL_MENU_HDR_FG);
    char title[48];
    snprintf(title, sizeof(title), "rMesh > %s", catNames[topSel]);
    drawStrS(title, 4, 2);
    spr.fillRect(0, MENU_HDR_H, DISP_W, MENU_AREA_H_, COL_BG);
    spr.setFont(&fonts::FreeSans9pt7b);
    spr.setTextSize(1);
    int end = min(listScroll + MENU_ITEMS_VIS, curMenuLen);
    for (int i = listScroll; i < end; i++) {
        int y   = MENU_HDR_H + (i - listScroll) * MENU_ITEM_H;
        bool sel = (i == listSel);
        if (sel) spr.fillRect(0, y, DISP_W, MENU_ITEM_H, COL_MENU_SEL);
        MenuItem& item = curMenu[i];

        if (item.type == FTYPE_ACTION) {
            int lw = spr.textWidth(item.label);
            spr.fillRect(DISP_W/2 - lw/2 - 8, y+1, lw+16, MENU_ITEM_H-2,
                         sel ? COL_MENU_SEL : COL_MENU_EDIT_BG);
            spr.setTextColor(sel ? COL_MENU_SEL_FG : COL_MENU_HDR_FG);
            drawStrS(item.label, DISP_W/2 - lw/2, y+3);
        } else if (item.type == FTYPE_DELETE_GROUP) {
            int lw = spr.textWidth(item.label);
            int bx = DISP_W - lw - 16;
            spr.fillRect(bx - 4, y+1, lw + 12, MENU_ITEM_H-2,
                         sel ? 0xF800u : COL_DELETE_BG);
            spr.setTextColor(sel ? 0xFFFFu : COL_DELETE_FG);
            drawStrS(item.label, bx, y+3);
        } else {
            spr.setTextColor(sel ? COL_MENU_SEL_FG : COL_MENU_FG);
            drawStrS(item.label, 6, y+3);
            char val[80]; fmtValue(val, sizeof(val), item);
            if (strlen(val) > 0) {
                int vx = DISP_W - spr.textWidth(val) - 6;
                if (vx < DISP_W/2) vx = DISP_W/2;
                spr.setTextColor(sel ? COL_MENU_SEL_FG : COL_MENU_VAL);
                drawStrS(val, vx, y+3);
            }
        }
    }
    if (curMenuLen > MENU_ITEMS_VIS) {
        int sbH = MENU_AREA_H_ * MENU_ITEMS_VIS / curMenuLen;
        int sbY = MENU_HDR_H   + MENU_AREA_H_ * listScroll  / curMenuLen;
        spr.fillRect(DISP_W - 3, sbY, 3, sbH, COL_MENU_SEL);
    }
    int fy = DISP_H - MENU_FOT_H;
    spr.fillRect(0, fy, DISP_W, MENU_FOT_H, COL_MENU_FOT);
    spr.setFont(&fonts::Font0);
    spr.setTextSize(1);
    spr.setTextColor(COL_MENU_FOT_FG);
    drawStrS("Drehen=Nav  Drucken=Bearb.  Halten=Zuruck", 4, fy+1);
    sprPush();
}

// ─── Menu: edit views ─────────────────────────────────────────────────
static void drawEditStr() {
    spr.fillScreen(COL_BG);
    spr.fillRect(0, 0, DISP_W, MENU_HDR_H, COL_MENU_HDR);
    spr.setFont(&fonts::FreeSans9pt7b);
    spr.setTextSize(1);
    spr.setTextColor(COL_MENU_HDR_FG);
    char title[64];
    if (newGroupSlot >= 0)
        snprintf(title, sizeof(title), "Neue Gruppe anlegen");
    else
        snprintf(title, sizeof(title), "Bearb.: %s", curMenu[editItemIdx].label);
    drawStrS(title, 4, 2);
    spr.setTextColor(COL_MENU_FG);
    drawStrS("Wert:", 6, 26);
    spr.drawRect(6, 46, DISP_W-12, 22, COL_MENU_SEL);
    spr.setTextColor(COL_MENU_VAL);
    drawStrS(editStrBuf, 10, 50);
    int cx = 10 + spr.textWidth(editStrBuf);
    if (cx < DISP_W-18) spr.drawFastVLine(cx, 49, 14, COL_CURSOR);
    spr.setFont(&fonts::Font0);
    spr.setTextSize(1);
    spr.setTextColor(COL_MENU_FOT_FG);
    drawStrS("Tastatur=Eingabe  Enter/Drucken=OK  Halten=Abbr.", 6, 90);
    sprPush();
}

static void drawEditNum() {
    int bx=40, by=55, bw=400, bh=100;
    spr.fillRect(0, 0, DISP_W, DISP_H, COL_BG);
    spr.fillRoundRect(bx, by, bw, bh, 6, COL_MENU_EDIT_BG);
    spr.drawRoundRect(bx, by, bw, bh, 6, COL_MENU_SEL);
    spr.setFont(&fonts::FreeSans9pt7b);
    spr.setTextSize(1);
    spr.setTextColor(COL_MENU_HDR_FG);
    drawStrS(curMenu[editItemIdx].label, bx+10, by+8);
    char val[48];
    MenuItem& item = curMenu[editItemIdx];
    int dec = (item.step >= 1.0f) ? 0 : (fabsf(item.step - 0.1f) < 0.05f) ? 1 : 3;
    char fmt[12]; snprintf(fmt, sizeof(fmt), "%%.%df", dec);
    char num[24]; snprintf(num, sizeof(num), fmt, editFloat);
    if (item.unit) snprintf(val, sizeof(val), "%s %s", num, item.unit);
    else           snprintf(val, sizeof(val), "%s", num);
    spr.setTextColor(COL_MENU_VAL);
    drawStrS(val, bx+10, by+32);
    spr.setFont(&fonts::Font0);
    spr.setTextSize(1);
    spr.setTextColor(COL_MENU_FOT_FG);
    drawStrS("Drehen=Wert  Drucken=OK  Halten=Abbr.", bx+10, by+82);
    sprPush();
}

static void drawEditDrop() {
    MenuItem& item = curMenu[editItemIdx];
    int numOpts = item.aux;
    int bx=20, by=10, bw=DISP_W-40;
    int maxVis = (DISP_H - by - MENU_HDR_H - MENU_FOT_H - 4) / MENU_ITEM_H;
    int visible = min(numOpts, maxVis);
    int bh = MENU_HDR_H + visible * MENU_ITEM_H + MENU_FOT_H + 4;
    spr.fillRect(0, 0, DISP_W, DISP_H, COL_BG);
    spr.fillRect(bx, by, bw, bh, COL_MENU_EDIT_BG);
    spr.drawRect(bx, by, bw, bh, COL_MENU_SEL);
    spr.setFont(&fonts::FreeSans9pt7b);
    spr.setTextSize(1);
    spr.setTextColor(COL_MENU_HDR_FG);
    drawStrS(item.label, bx+6, by+3);
    int scroll = editDropIdx - visible/2;
    if (scroll < 0) scroll = 0;
    if (scroll > numOpts - visible) scroll = max(0, numOpts - visible);
    for (int i = scroll; i < scroll+visible && i < numOpts; i++) {
        int y = by + MENU_HDR_H + (i-scroll)*MENU_ITEM_H;
        bool sel = (i == editDropIdx);
        if (sel) spr.fillRect(bx+1, y, bw-2, MENU_ITEM_H, COL_MENU_SEL);
        const char* lbl = (item.type == FTYPE_DROP_F)
            ? ((const DropF*)item.opts)[i].label
            : ((const DropI*)item.opts)[i].label;
        spr.setTextColor(sel ? COL_MENU_SEL_FG : COL_MENU_FG);
        drawStrS(lbl, bx+8, y+3);
    }
    int fy = by + bh - MENU_FOT_H;
    spr.setFont(&fonts::Font0);
    spr.setTextSize(1);
    spr.setTextColor(COL_MENU_FOT_FG);
    drawStrS("Drehen=Wahl  Drucken=OK  Halten=Abbr.", bx+6, fy+1);
    sprPush();
}

// ─── Menu actions ─────────────────────────────────────────────────────
static void doDeleteMessages() {
    historyCount = 0; historyHead = 0;
    uiMode = UI_CHAT; needRedraw = true;
}
static void doPowerOff() {
    spr.fillSprite(COL_BG);
    spr.setFont(&fonts::FreeSans9pt7b); spr.setTextSize(1);
    spr.setTextColor(COL_MENU_HDR_FG);
    drawStrS("Ausschalten...", 4, DISP_H / 2 - 8);
    sprPush();
    delay(800);
    instance.sleep();  // deep sleep; wake via boot button (GPIO0)
}
static void doAnnounce() {
    Frame f;
    f.frameType = Frame::FrameTypes::ANNOUNCE_FRAME;
    f.transmitMillis = 0;
    f.port = 0; txBuffer.push_back(f);
    f.port = 1; txBuffer.push_back(f);
    announceTimer = millis() + ANNOUNCE_TIME;  // reset periodic timer
    uiMode = UI_CHAT; needRedraw = true;
}
static void doSave() {
    IPAddress parsedIPs[5];
    for (int i = 0; i < 5; i++) strToIP(tmpPeerIP[i], parsedIPs[i]);
    bool legacyBak[5] = {};
    for (int i = 0; i < 5 && (size_t)i < udpPeerLegacy.size(); i++) legacyBak[i] = (bool)udpPeerLegacy[i];
    std::vector<IPAddress> tail;
    std::vector<bool> tailLegacy;
    for (size_t i = 5; i < udpPeers.size(); i++) { tail.push_back(udpPeers[i]); tailLegacy.push_back((bool)udpPeerLegacy[i]); }
    udpPeers.clear(); udpPeerLegacy.clear();
    for (int i = 0; i < 5; i++) {
        if (parsedIPs[i] != IPAddress(0,0,0,0)) { udpPeers.push_back(parsedIPs[i]); udpPeerLegacy.push_back(legacyBak[i]); }
    }
    for (size_t i = 0; i < tail.size(); i++) { udpPeers.push_back(tail[i]); udpPeerLegacy.push_back(tailLegacy[i]); }
    uiMode = UI_CHAT; needRedraw = true;
    saveSettings();
}
static void doSaveDisplay() {
    instance.setBrightness((uint8_t)dispBrightness);
    displayBrightness = (uint8_t)dispBrightness;
    prefs.putUChar("dispBrightW", displayBrightness);
    prefs.putFloat("dispTxtSize", dispTextSize);
    uiMode = UI_CHAT; needRedraw = true;
}
static void doSaveSetup() {
    instance.setBrightness((uint8_t)dispBrightness);
    displayBrightness = (uint8_t)dispBrightness;
    prefs.putUChar("dispBrightW", displayBrightness);
    prefs.putFloat("dispTxtSize", dispTextSize);
    saveSettings();
    uiMode = UI_CHAT; needRedraw = true;
}
static void doReboot() {
    rebootTimer = millis(); rebootRequested = true;
    uiMode = UI_CHAT; needRedraw = true;
}
static void doUpdate() {
    checkForUpdates();
    uiMode = UI_CHAT; needRedraw = true;
}
static void doForceUpdateRelease() {
    checkForUpdates(true, 0);
    uiMode = UI_CHAT; needRedraw = true;
}
static void doForceUpdateDev() {
    checkForUpdates(true, 1);
    uiMode = UI_CHAT; needRedraw = true;
}
static void doSaveGroups() {
    prefs.putInt("grpCount", groupCount);
    prefs.putInt("grpSamCol", sammelGroupIdx);
    for (int i = 0; i < groupCount; i++) {
        char key[8];  snprintf(key,  sizeof(key),  "grp%d",    i); prefs.putString(key, groupNames[i]);
        char mkey[10]; snprintf(mkey, sizeof(mkey), "grpMute%d", i); prefs.putUChar(mkey, groupMute[i] ? 1 : 0);
        char skey[10]; snprintf(skey, sizeof(skey), "grpInSm%d", i); prefs.putUChar(skey, groupInSammel[i] ? 1 : 0);
    }
    // Remove excess old slots
    for (int i = groupCount; i < MAX_GROUPS; i++) {
        char key[8];   snprintf(key,  sizeof(key),  "grp%d",    i); prefs.remove(key);
        char mkey[10]; snprintf(mkey, sizeof(mkey), "grpMute%d", i); prefs.remove(mkey);
        char skey[10]; snprintf(skey, sizeof(skey), "grpInSm%d", i); prefs.remove(skey);
    }
    uiMode = UI_CHAT; needRedraw = true;
}

static int powerConfirmSel = 0;  // 0 = Nein, 1 = Ja

static void drawConfirmPower() {
    spr.fillScreen(COL_BG);
    spr.fillRect(0, 0, DISP_W, MENU_HDR_H, COL_MENU_HDR);
    spr.setFont(&fonts::FreeSans9pt7b);
    spr.setTextSize(1);
    spr.setTextColor(COL_MENU_HDR_FG);
    drawStrS("rMesh > Ausschalten", 4, 2);

    spr.setTextColor(COL_MENU_FG);
    spr.setTextDatum(lgfx::middle_center);
    drawStrS("Geraet ausschalten?", DISP_W / 2, DISP_H / 2 - 22);
    spr.setTextDatum(lgfx::top_left);

    const int btnW = 80, btnH = 26;
    const int btnY = DISP_H / 2;
    const int btnNeinX = DISP_W / 2 - 90;
    const int btnJaX   = DISP_W / 2 + 10;

    spr.fillRoundRect(btnNeinX, btnY, btnW, btnH, 4,
        powerConfirmSel == 0 ? COL_MENU_SEL : COL_MENU_HDR);
    spr.setTextColor(COL_MENU_SEL_FG);
    spr.setTextDatum(lgfx::middle_center);
    drawStrS("Nein", btnNeinX + btnW / 2, btnY + btnH / 2);

    spr.fillRoundRect(btnJaX, btnY, btnW, btnH, 4,
        powerConfirmSel == 1 ? COL_MENU_SEL : COL_MENU_HDR);
    drawStrS("Ja", btnJaX + btnW / 2, btnY + btnH / 2);
    spr.setTextDatum(lgfx::top_left);

    infoFooter(1, 1, 0);
    sprPush();
}

static void doTune() {
    Frame f;
    f.frameType = Frame::FrameTypes::TUNE_FRAME;
    f.transmitMillis = 0;
    f.port = 0; txBuffer.push_back(f);
    uiMode = UI_CHAT; needRedraw = true;
}

static void drawAbout() {
    infoHeader("About");

    const int lineH = 18;
    int y = MENU_HDR_H + 6;

    spr.setFont(&fonts::FreeSans9pt7b);
    spr.setTextSize(1);

    // Version
    spr.setTextColor(0x7BEFu);
    drawStrS("Version:", 4, y);
    spr.setTextColor(COL_MENU_FG);
    drawStrS(VERSION, 90, y);
    y += lineH;

    // IP-Adresse
    char ipBuf[20] = "-";
    if (WiFi.status() == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    }
    spr.setTextColor(0x7BEFu);
    drawStrS("WiFi IP:", 4, y);
    spr.setTextColor(COL_MENU_FG);
    drawStrS(ipBuf, 90, y);
    y += lineH;

    // Chip ID
    spr.setTextColor(0x7BEFu);
    drawStrS("Chip ID:", 4, y);
    spr.setTextColor(COL_MENU_FG);
    drawStrS(setupChipId, 90, y);
    y += lineH + 4;

    // Separator
    spr.drawFastHLine(4, y, DISP_W - 8, COL_SEPARATOR);
    y += 8;

    // Website & GitHub
    spr.setFont(&fonts::FreeSans9pt7b);
    spr.setTextSize(1);
    spr.setTextColor(0x07FFu);
    drawStrS("www.rMesh.de", 4, y);
    y += 16;
    drawStrS("github.com/DN9KGB/rMesh", 4, y);

    infoFooter(1, 1, 0);
    sprPush();
}

// ─── Menu navigation ──────────────────────────────────────────────────
static void openMenu() {
    for (int i = 0; i < 5; i++) {
        if ((size_t)i < udpPeers.size()) ipToStr(udpPeers[i], tmpPeerIP[i], sizeof(tmpPeerIP[i]));
        else strncpy(tmpPeerIP[i], "0.0.0.0", sizeof(tmpPeerIP[i]));
    }
    topSel = 0; topScroll = 0; uiMode = UI_MENU_TOP; needRedraw = true;
}

static void enterSubmenu(int idx) {
    topSel = idx;
    switch (idx) {
        case 0: curMenu = netItems;   curMenuLen = sizeof(netItems)/sizeof(netItems[0]);    break;
        case 1: curMenu = loraItems;  curMenuLen = sizeof(loraItems)/sizeof(loraItems[0]);   break;
        case 2: curMenu = setupItems; curMenuLen = sizeof(setupItems)/sizeof(setupItems[0]); break;
        case 3:
            buildGroupMenu();
            curMenu = groupItemsBuf;
            curMenuLen = groupItemsLen;
            break;
        case 4: infoScroll = 0; uiMode = UI_ROUTING; needRedraw = true; return;
        case 5: infoScroll = 0; uiMode = UI_PEERS;   needRedraw = true; return;
        case 6: infoScroll = 0; uiMode = UI_MONITOR; needRedraw = true; return;
        case 7: doAnnounce();  return;
        case 8: doTune();      return;
        case 9: infoScroll = 0; uiMode = UI_ABOUT; needRedraw = true; return;
        case 10: powerConfirmSel = 0; uiMode = UI_CONFIRM_POWER; needRedraw = true; return;
        default: return;
    }
    listSel = 0; listScroll = 0; uiMode = UI_MENU_LIST; needRedraw = true;
}

static void activateItem() {
    MenuItem& item = curMenu[listSel];
    editItemIdx = listSel;
    switch (item.type) {
        case FTYPE_BOOL:
            *(bool*)item.ptr = !*(bool*)item.ptr; needRedraw = true; break;
        case FTYPE_STRING: case FTYPE_IP:
            strncpy(editStrBuf, (char*)item.ptr, item.aux);
            editStrBuf[item.aux] = 0; uiMode = UI_EDIT_STR; needRedraw = true; break;
        case FTYPE_HEX8:
            snprintf(editStrBuf, sizeof(editStrBuf), "%02X", *(uint8_t*)item.ptr);
            uiMode = UI_EDIT_STR; needRedraw = true; break;
        case FTYPE_FLOAT:
            editFloat = *(float*)item.ptr; uiMode = UI_EDIT_NUM; needRedraw = true; break;
        case FTYPE_INT8:
            editFloat = (float)*(int8_t*)item.ptr; uiMode = UI_EDIT_NUM; needRedraw = true; break;
        case FTYPE_INT16:
            editFloat = (float)*(int16_t*)item.ptr; uiMode = UI_EDIT_NUM; needRedraw = true; break;
        case FTYPE_UINT8:
            editFloat = (float)*(uint8_t*)item.ptr; uiMode = UI_EDIT_NUM; needRedraw = true; break;
        case FTYPE_DROP_F: {
            float fv = *(float*)item.ptr; const DropF* o = (const DropF*)item.opts;
            editDropIdx = 0;
            for (int i = 0; i < item.aux; i++) if (fabsf(o[i].v-fv)<0.01f) {editDropIdx=i;break;}
            uiMode = UI_EDIT_DROP; needRedraw = true; break;
        }
        case FTYPE_DROP_I: {
            int iv = (int)*(uint8_t*)item.ptr; const DropI* o = (const DropI*)item.opts;
            editDropIdx = 0;
            for (int i = 0; i < item.aux; i++) if (o[i].v==iv) {editDropIdx=i;break;}
            uiMode = UI_EDIT_DROP; needRedraw = true; break;
        }
        case FTYPE_READONLY: case FTYPE_READONLY_STR: break;
        case FTYPE_DELETE_GROUP:
            deleteGroup(item.aux); break;
        case FTYPE_TOGGLE_MUTE: {
            int idx = item.aux;
            if (idx >= 0 && idx < groupCount) {
                groupMute[idx] = !groupMute[idx];
                doSaveGroups();
                buildGroupMenu();
                curMenuLen = groupItemsLen;
                needRedraw = true;
            }
            break;
        }
        case FTYPE_TOGGLE_INSAM: {
            int idx = item.aux;
            if (idx >= 0 && idx < groupCount) {
                if (sammelGroupIdx == idx) {
                    // This is the collection group itself -> unset it
                    sammelGroupIdx = -1;
                    for (int j = 0; j < groupCount; j++) groupInSammel[j] = false;
                } else if (sammelGroupIdx < 0) {
                    // No collection group yet -> set this group as collection group
                    sammelGroupIdx = idx;
                } else if (groupInSammel[idx]) {
                    groupInSammel[idx] = false;
                } else {
                    groupInSammel[idx] = true;
                }
                doSaveGroups();
                buildGroupMenu();
                curMenuLen = groupItemsLen;
                needRedraw = true;
            }
            break;
        }
        case FTYPE_ACTION:
            if (item.action) item.action(); break;
    }
}

static void confirmEditStr() {
    if (newGroupSlot >= 0) {
        // New group: write name into slot
        strncpy(groupNames[newGroupSlot], editStrBuf, MAX_CALLSIGN_LENGTH);
        groupNames[newGroupSlot][MAX_CALLSIGN_LENGTH] = '\0';
        // Empty input = do not create group
        if (strlen(groupNames[newGroupSlot]) == 0) {
            groupCount--;
        } else {
            doSaveGroups();
        }
        newGroupSlot = -1;
        buildGroupMenu();
        curMenuLen = groupItemsLen;
        if (listSel >= curMenuLen) listSel = max(0, curMenuLen - 1);
        uiMode = UI_MENU_LIST;
        needRedraw = true;
        return;
    }

    MenuItem& item = curMenu[editItemIdx];
    if (item.type == FTYPE_HEX8) {
        unsigned int v = 0; sscanf(editStrBuf, "%X", &v);
        *(uint8_t*)item.ptr = (uint8_t)(v & 0xFF);
    } else {
        strncpy((char*)item.ptr, editStrBuf, item.aux);
        ((char*)item.ptr)[item.aux] = 0;
    }
    uiMode = UI_MENU_LIST; needRedraw = true;
}

static void confirmEditNum() {
    MenuItem& item = curMenu[editItemIdx];
    if (item.maxVal > 0.0f || item.minVal > 0.0f) {
        if (item.minVal > 0.0f && editFloat < item.minVal) editFloat = item.minVal;
        if (item.maxVal > 0.0f && editFloat > item.maxVal) editFloat = item.maxVal;
    }
    if      (item.type == FTYPE_FLOAT) *(float*)item.ptr   = editFloat;
    else if (item.type == FTYPE_INT8)  *(int8_t*)item.ptr  = (int8_t)editFloat;
    else if (item.type == FTYPE_INT16) *(int16_t*)item.ptr = (int16_t)editFloat;
    else if (item.type == FTYPE_UINT8) *(uint8_t*)item.ptr = (uint8_t)editFloat;
    uiMode = UI_MENU_LIST; needRedraw = true;
}

static void confirmEditDrop() {
    MenuItem& item = curMenu[editItemIdx];
    if (item.type == FTYPE_DROP_F) *(float*)item.ptr   = ((const DropF*)item.opts)[editDropIdx].v;
    else                           *(uint8_t*)item.ptr = (uint8_t)((const DropI*)item.opts)[editDropIdx].v;
    uiMode = UI_MENU_LIST; needRedraw = true;
}

static void scrollListTo(int newSel) {
    listSel = newSel;
    if (listSel < listScroll) listScroll = listSel;
    if (listSel >= listScroll + MENU_ITEMS_VIS) listScroll = listSel - MENU_ITEMS_VIS + 1;
    needRedraw = true;
}

// ─── Public API ───────────────────────────────────────────────────────
void initDisplay() {
    logPrintf(LOG_INFO, "Display", "instance.begin() ...");
    Serial.flush();
    uint32_t probe = instance.begin(NO_INIT_FATFS);
    logPrintf(LOG_INFO, "Display", "instance.begin() done, probe=0x%08X", probe);
    Serial.flush();

    dispBrightness = (float)displayBrightness;
    if (dispBrightness < 5.0f) dispBrightness = 5.0f;
    if (dispBrightness > 255.0f) dispBrightness = 255.0f;
    dispTextSize   = prefs.getFloat("dispTxtSize", 1.0f);
    if (dispTextSize < 0.5f) dispTextSize = 0.5f;
    if (dispTextSize > 4.0f) dispTextSize = 4.0f;

    // Chip ID
    {
        uint64_t mac = ESP.getEfuseMac();
        snprintf(setupChipId, sizeof(setupChipId), "%02X%02X%02X%02X%02X%02X",
            (uint8_t)(mac >> 40), (uint8_t)(mac >> 32), (uint8_t)(mac >> 24),
            (uint8_t)(mac >> 16), (uint8_t)(mac >> 8), (uint8_t)(mac));
    }

    // Load groups
    groupCount = prefs.getInt("grpCount", 0);
    if (groupCount > MAX_GROUPS) groupCount = MAX_GROUPS;
    sammelGroupIdx = prefs.getInt("grpSamCol", -1);
    if (sammelGroupIdx >= groupCount) sammelGroupIdx = -1;
    for (int i = 0; i < groupCount; i++) {
        char key[8];   snprintf(key,  sizeof(key),  "grp%d",    i);
        String s = prefs.getString(key, "");
        strncpy(groupNames[i], s.c_str(), MAX_CALLSIGN_LENGTH);
        groupNames[i][MAX_CALLSIGN_LENGTH] = '\0';
        groupUnread[i] = 0;
        char mkey[10]; snprintf(mkey, sizeof(mkey), "grpMute%d", i); groupMute[i]     = (prefs.getUChar(mkey, 0) != 0);
        char skey[10]; snprintf(skey, sizeof(skey), "grpInSm%d", i); groupInSammel[i] = (prefs.getUChar(skey, 0) != 0);
    }

    spr.setColorDepth(16);
    spr.createSprite(DISP_W, DISP_H);

    instance.lockSPI();
    lcd.init();
    lcd.setRotation(7);
    lcd.startWrite();
    lcd.writeCommand(0xB6); lcd.writeData(0x80); lcd.writeData(0x02); lcd.writeData(0x3B);
    lcd.endWrite();
    lcd.fillScreen(COL_BG);
    lcd.startWrite();
    lcd.setFont(&fonts::FreeSans9pt7b);
    lcd.setTextColor(0xFFFFu); lcd.setTextSize(2);
    lcd.setTextDatum(lgfx::middle_center);
    drawStr("Starte rMesh Pager", DISP_W / 2, DISP_H / 2);
    lcd.setTextDatum(lgfx::top_left);
    lcd.setTextSize(1);
    lcd.setFont(&fonts::Font0);
    lcd.endWrite();
    instance.unlockSPI();
    logPrintf(LOG_INFO, "Display", "lcd.init() done"); Serial.flush();

    instance.setBrightness((uint8_t)dispBrightness);
    delay(600);
    needRedraw = true;
}

void displayOnNewMessage(const char* srcCall, const char* text, const char* dstGroup, const char* dstCall) {
    const char* grp = (dstGroup && strlen(dstGroup) > 0) ? dstGroup : "";
    const char* dst = (dstCall  && strlen(dstCall)  > 0) ? dstCall  : "";

    // Collection group redirect: sort message into collection group
    const char* storeGrp = grp;
    if (strlen(grp) > 0 && sammelGroupIdx >= 0) {
        for (int i = 0; i < groupCount; i++) {
            if (strcmp(groupNames[i], grp) == 0 && groupInSammel[i]) {
                storeGrp = groupNames[sammelGroupIdx];
                break;
            }
        }
    }
    addLine(srcCall, text, false, storeGrp, dst);

    // Unread counter: only for normal, non-muted groups
    if (strlen(grp) > 0) {
        for (int i = 0; i < groupCount; i++) {
            if (strcmp(groupNames[i], grp) == 0) {
                if (!groupMute[i] && !groupInSammel[i] && activeGroup != i) {
                    groupUnread[i]++;
                }
                // Collection group itself does not get an unread counter
                break;
            }
        }
    }
    if (uiMode == UI_CHAT) needRedraw = true;
}

void displayTxFrame(const char* dstCall, const char* text) {
    char label[MAX_CALLSIGN_LENGTH + 8];
    snprintf(label, sizeof(label), "%s\xBB%s", settings.mycall, dstCall);
    char grp[MAX_CALLSIGN_LENGTH + 1] = {0};
    if (activeGroup >= 0) strncpy(grp, groupNames[activeGroup], MAX_CALLSIGN_LENGTH);
    addLine(label, text, true, grp);
    if (uiMode == UI_CHAT) needRedraw = true;
}

void displayMonitorFrame(const Frame& f) {
    // Format a compact log line for the monitor view
    char ts[8] = {0};
    time_t t = f.timestamp ? f.timestamp : time(NULL);
    struct tm* tm_ = localtime(&t);
    if (tm_) strftime(ts, sizeof(ts), "%H:%M", tm_);

    const char* dir = f.tx ? "TX" : "RX";
    char line[MON_LINE_W];

    // Decide frame-type label
    const char* ftype;
    switch (f.frameType) {
        case 0x00: ftype = "ANN"; break;
        case 0x01: ftype = "ACK"; break;
        case 0x02: ftype = "TUN"; break;
        case 0x04: ftype = "MAC"; break;
        default:   ftype = "MSG"; break;
    }

    bool isMsg = (f.frameType == 0x03 || f.frameType == 0x05 || f.frameType == 0x00);
    if (isMsg && (strlen(f.srcCall) > 0)) {
        snprintf(line, MON_LINE_W, "%s %s %s %-8s>%-8s H%d %.0fdB",
                 ts, dir, ftype,
                 f.srcCall, strlen(f.dstCall) ? f.dstCall : "*",
                 f.hopCount, f.rssi);
    } else {
        snprintf(line, MON_LINE_W, "%s %s %s %-9s H%d %.0fdB",
                 ts, dir, ftype, f.nodeCall, f.hopCount, f.rssi);
    }

    // Store in ring buffer
    strncpy(monLines[monHead % MON_HISTORY], line, MON_LINE_W - 1);
    monLines[monHead % MON_HISTORY][MON_LINE_W - 1] = '\0';
    monHead++;
    if (monCount < MON_HISTORY) monCount++;
    monNewData = true;
}

// ─── Main loop ────────────────────────────────────────────────────────
void displayUpdateLoop() {
    RotaryMsg_t rot = instance.getRotary();
    bool btnNow = (digitalRead(ROTARY_C) == LOW);
    bool shortPress = false, longPress = false;

    if (btnNow && !prevBtn)   { btnDownMs = millis(); longHandled = false; }
    if (btnNow && !longHandled && (millis() - btnDownMs >= 600)) { longPress = true; longHandled = true; }
    if (!btnNow && prevBtn && !longHandled) shortPress = true;
    prevBtn = btnNow;

    char c = 0;
    bool keyAvail = (instance.getKeyChar(&c) == 1 && c != 0);

    if (uiMode == UI_CHAT) {
        if (rot.dir == ROTARY_DIR_UP || rot.dir == ROTARY_DIR_DOWN) {
            if (chatScrollMode) {
                // Scroll mode: UP = older messages, DOWN = newer
                if (rot.dir == ROTARY_DIR_UP) chatScrollOff++;
                else if (chatScrollOff > 0)   chatScrollOff--;
                needRedraw = true;
            } else {
                // Group mode: rotate switches active group
                int tabList[MAX_GROUPS + 1];
                int tabCount = buildTabList(tabList);
                if (tabCount > 1) {
                    int cur = currentTabIndex();
                    cur = (rot.dir == ROTARY_DIR_UP)
                        ? (cur + 1) % tabCount
                        : (cur - 1 + tabCount) % tabCount;
                    activeGroup = tabList[cur];
                    if (activeGroup >= 0) groupUnread[activeGroup] = 0;
                    chatScrollOff = 0;
                    needRedraw = true;
                }
            }
        }
        // Single click: toggle between group-switch and scroll mode
        if (shortPress) { chatScrollMode = !chatScrollMode; needRedraw = true; }
        // Long press: open settings menu
        if (longPress)  openMenu();

        // Keyboard → chat input
        if (keyAvail) {
            if (c == '\n' || c == '\r') {
                if (inputLen > 0) {
                    inputBuf[inputLen] = '\0';
                    if (activeGroup >= 0 && strlen(groupNames[activeGroup]) > 0) {
                        sendGroup(groupNames[activeGroup], inputBuf);
                        displayTxFrame(groupNames[activeGroup], inputBuf);
                    } else {
                        // Broadcast: empty dstCall = no DST_CALL_HEADER in frame
                        sendMessage("", inputBuf);
                        displayTxFrame("", inputBuf);
                    }
                    inputLen = 0; inputBuf[0] = '\0'; needRedraw = true;
                }
            } else if (c == '\b') {
                if (inputLen > 0) { inputLen--; inputBuf[inputLen] = '\0'; needRedraw = true; }
            } else if (inputLen < INPUT_MAX_LEN) {
                inputBuf[inputLen++] = c; inputBuf[inputLen] = '\0'; needRedraw = true;
            }
        }
    }
    else if (uiMode == UI_MENU_TOP) {
        auto clampTopScroll = [&]() {
            if (topSel < topScroll) topScroll = topSel;
            if (topSel >= topScroll + TOP_MENU_VIS) topScroll = topSel - TOP_MENU_VIS + 1;
        };
        if (rot.dir == ROTARY_DIR_UP) {
            topSel = (topSel + 1) % TOP_MENU_N; clampTopScroll(); needRedraw = true;
        }
        if (rot.dir == ROTARY_DIR_DOWN) {
            topSel = (topSel + TOP_MENU_N - 1) % TOP_MENU_N; clampTopScroll(); needRedraw = true;
        }
        if (shortPress) enterSubmenu(topSel);
        if (longPress)  { uiMode = UI_CHAT; needRedraw = true; }
    }
    else if (uiMode == UI_MENU_LIST) {
        if (rot.dir == ROTARY_DIR_UP   && listSel < curMenuLen-1) scrollListTo(listSel+1);
        if (rot.dir == ROTARY_DIR_DOWN && listSel > 0)            scrollListTo(listSel-1);
        if (shortPress) activateItem();
        if (longPress)  { uiMode = UI_MENU_TOP; needRedraw = true; }
    }
    else if (uiMode == UI_EDIT_STR) {
        if (keyAvail) {
            int maxLen = 64;
            if (newGroupSlot < 0) {
                maxLen = curMenu[editItemIdx].aux;
                if (maxLen <= 0 || maxLen > 64) maxLen = 64;
            }
            if (c == '\n' || c == '\r') { confirmEditStr(); }
            else if (c == '\b') {
                int len = strlen(editStrBuf);
                if (len > 0) { editStrBuf[len-1] = 0; needRedraw = true; }
            } else if ((int)strlen(editStrBuf) < maxLen) {
                int len = strlen(editStrBuf);
                editStrBuf[len] = c; editStrBuf[len+1] = 0; needRedraw = true;
            }
        }
        if (shortPress) confirmEditStr();
        if (longPress) {
            // Cancel: discard new group
            if (newGroupSlot >= 0) {
                groupCount--;
                newGroupSlot = -1;
                buildGroupMenu();
                curMenuLen = groupItemsLen;
            }
            uiMode = UI_MENU_LIST; needRedraw = true;
        }
    }
    else if (uiMode == UI_EDIT_NUM) {
        MenuItem& item = curMenu[editItemIdx];
        float step = (item.step > 0.0f) ? item.step : 1.0f;
        if (rot.dir == ROTARY_DIR_UP)   { editFloat += step; needRedraw = true; }
        if (rot.dir == ROTARY_DIR_DOWN) { editFloat -= step; needRedraw = true; }
        if (item.minVal > 0.0f && editFloat < item.minVal) { editFloat = item.minVal; needRedraw = true; }
        if (item.maxVal > 0.0f && editFloat > item.maxVal) { editFloat = item.maxVal; needRedraw = true; }
        if (shortPress) confirmEditNum();
        if (longPress)  { uiMode = UI_MENU_LIST; needRedraw = true; }
    }
    else if (uiMode == UI_EDIT_DROP) {
        int numOpts = curMenu[editItemIdx].aux;
        if (rot.dir == ROTARY_DIR_UP   && editDropIdx < numOpts-1) { editDropIdx++; needRedraw = true; }
        if (rot.dir == ROTARY_DIR_DOWN && editDropIdx > 0)         { editDropIdx--; needRedraw = true; }
        if (shortPress) confirmEditDrop();
        if (longPress)  { uiMode = UI_MENU_LIST; needRedraw = true; }
    }
    else if (uiMode == UI_CONFIRM_POWER) {
        if (rot.dir != ROTARY_DIR_NONE) { powerConfirmSel = !powerConfirmSel; needRedraw = true; }
        if (shortPress) {
            if (powerConfirmSel == 1) doPowerOff();
            else { uiMode = UI_MENU_TOP; needRedraw = true; }
        }
        if (longPress) { uiMode = UI_MENU_TOP; needRedraw = true; }
    }
    else if (uiMode == UI_ROUTING || uiMode == UI_PEERS || uiMode == UI_MONITOR || uiMode == UI_ABOUT) {
        if (rot.dir == ROTARY_DIR_UP)   { infoScroll++; needRedraw = true; }
        if (rot.dir == ROTARY_DIR_DOWN && infoScroll > 0) { infoScroll--; needRedraw = true; }
        if (monNewData && uiMode == UI_MONITOR) { needRedraw = true; monNewData = false; }
        if (shortPress) { uiMode = UI_MENU_TOP; needRedraw = true; }
    }

    instance.loop();

    // Refresh header time every minute
    if (uiMode == UI_CHAT) {
        time_t nowT = time(NULL);
        struct tm* tmNow = localtime(&nowT);
        if (tmNow && tmNow->tm_min != lastMinute) {
            lastMinute = tmNow->tm_min;
            needRedraw = true;
        }
    }

    // Refresh battery state every 10 s
    if (uiMode == UI_CHAT && (battPct < 0 || millis() - lastBattMs >= 10000)) {
        if (instance.gauge.refresh()) {
            int pct = (int)instance.gauge.getStateOfCharge();
            bool chg = !instance.gauge.getBatteryStatus().isInDischargeMode();
            if (pct != battPct || chg != battCharging) {
                battPct      = pct;
                battCharging = chg;
                needRedraw   = true;
            }
        }
        lastBattMs = millis();
    }

    if (needRedraw) {
        switch (uiMode) {
            case UI_CHAT:       redraw();        break;
            case UI_MENU_TOP:   drawMenuTop();   break;
            case UI_MENU_LIST:  drawMenuList();  break;
            case UI_EDIT_STR:   drawEditStr();   break;
            case UI_EDIT_NUM:   drawEditNum();   break;
            case UI_EDIT_DROP:  drawEditDrop();  break;
            case UI_ROUTING:    drawRouting();   break;
            case UI_PEERS:      drawPeers();     break;
            case UI_MONITOR:    drawMonitor();   break;
            case UI_ABOUT:         drawAbout();        break;
            case UI_CONFIRM_POWER: drawConfirmPower(); break;
        }
        needRedraw = false;
    }
}

#endif // LILYGO_T_LORA_PAGER
