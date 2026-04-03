/*
 * display_SEEED_SenseCAP_Indicator.cpp
 *
 * Vollständige Pager-UI für den Seeed SenseCAP Indicator (ST7701S 480×480 RGB).
 * Software-Tastatur, Menüsystem, Touch-Navigation – angepasst von T-LoraPager.
 */

#ifdef SEEED_SENSECAP_INDICATOR

#include "display_SEEED_SenseCAP_Indicator.h"
#include "hal_SEEED_SenseCAP_Indicator.h"
#include "settings.h"
#include "helperFunctions.h"
#include "config.h"
#include "main.h"
#include "frame.h"
#include "routing.h"
#include "peer.h"
#include "wifiFunctions.h"
#include "logging.h"

#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <Preferences.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

// ─── UTF-8 → CP437 (LovyanGFX Font0 = CP437) ─────────────────────────────────
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
                case 0x00C4: dst[di++] = (char)0x8E; break;
                case 0x00D6: dst[di++] = (char)0x99; break;
                case 0x00DC: dst[di++] = (char)0x9A; break;
                case 0x00E4: dst[di++] = (char)0x84; break;
                case 0x00F6: dst[di++] = (char)0x94; break;
                case 0x00FC: dst[di++] = (char)0x81; break;
                case 0x00DF: dst[di++] = (char)0xE1; break;
                case 0x00E9: dst[di++] = (char)0x82; break;
                case 0x00E8: dst[di++] = (char)0x8A; break;
                case 0x00E0: dst[di++] = (char)0x85; break;
                default:     dst[di++] = '?';         break;
            }
        } else if ((*s & 0xF0) == 0xE0 && s[1] && s[2]) { s += 3; dst[di++] = '?'; }
        else if ((*s & 0xF8) == 0xF0 && s[1] && s[2] && s[3]) { s += 4; dst[di++] = '?'; }
        else { s++; }
    }
    dst[di] = '\0';
}

// ─── Display constants ───────────────────────────────────────────────────────
#define DISP_W      480
#define DISP_H      480

// Base layout
#define HDR_H       18      // Header bar
#define TAB_H       32      // Group tabs
#define NAV_H       50      // Navigation bar at bottom
#define NAV_Y       (DISP_H - NAV_H)                    // 430

// Chat without keyboard
#define MSG_Y       (HDR_H + TAB_H)                     // 50
#define INPUT_H     36
#define INPUT_Y     (NAV_Y - INPUT_H)                   // 394
#define MSG_H       (INPUT_Y - MSG_Y)                   // 344

// Chat with keyboard
#define KBD_ROW_H   41
#define KBD_ROWS    4
#define KBD_H       (KBD_ROWS * KBD_ROW_H)              // 164
#define KBD_Y       (NAV_Y - KBD_H)                     // 266
#define INPUT_Y_KBD (KBD_Y - INPUT_H)                   // 230
#define MSG_H_KBD   (INPUT_Y_KBD - MSG_Y)               // 180

// Menu (modal, fullscreen)
#define MENU_HDR_H      40
#define MENU_ITEM_H     48
#define MENU_AREA_H     (NAV_Y - MENU_HDR_H)            // 390
#define MENU_ITEMS_VIS  (MENU_AREA_H / MENU_ITEM_H)     // 8

// Touch
#define TOUCH_ADDR  0x48

// Limits
#define MAX_HISTORY     80
#define MAX_GROUPS      10
#define INPUT_MAX_LEN   200
#define MON_HISTORY     80
#define MON_LINE_W      72

// Farben
#define COL_BG           0x0000u
#define COL_SEPARATOR    0x4A49u
#define COL_OWN          0x07E0u
#define COL_RX           0xFFE0u
#define COL_HDR_BG       0x2104u
#define COL_HDR_FG       0xFFFFu
#define COL_TAB_BG       0x1082u
#define COL_TAB_ACT_BG   0x07BEu
#define COL_TAB_FG       0xCE79u
#define COL_TAB_ACT_FG   0x0000u
#define COL_UNREAD_BG    0x8400u
#define COL_UNREAD_FG    0xFFE0u
#define COL_NAV_BG       0x2104u
#define COL_NAV_ACT_BG   0xFCC0u
#define COL_NAV_FG       0x8410u
#define COL_NAV_ACT_FG   0x0000u
#define COL_INPUT_BG     0x1082u
#define COL_INPUT_FG     0xFFFFu
#define COL_CURSOR       0xF800u
#define COL_KBD_BG       0x18C3u
#define COL_KBD_KEY_BG   0x39E7u
#define COL_KBD_KEY_FG   0xFFFFu
#define COL_KBD_SP_BG    0x1294u
#define COL_KBD_SEND_BG  0x03A0u
#define COL_MENU_HDR     0x2104u
#define COL_MENU_HDR_FG  0xFFFFu
#define COL_MENU_SEL     0x1294u
#define COL_MENU_SEL_FG  0xFFFFu
#define COL_MENU_FG      0xCE79u
#define COL_MENU_VAL     0x07FFu
#define COL_MENU_EDIT_BG 0x18C3u
#define COL_DELETE_BG    0x6000u
#define COL_DELETE_FG    0xFFFFu

// ─── LovyanGFX: Panel_ST7701 + Bus_RGB ───────────────────────────────────────
class LGFX_SenseCAP : public lgfx::LGFX_Device {
public:
    lgfx::Panel_ST7701 _st7701;
    lgfx::Bus_RGB      _rgb;
public:
    LGFX_SenseCAP() {
        {
            auto cfg = _rgb.config();
            cfg.panel        = &_st7701;
            cfg.freq_write   = 16000000;
            cfg.pin_pclk    = 21;
            cfg.pin_vsync   = 17;
            cfg.pin_hsync   = 16;
            cfg.pin_henable = 18;
            cfg.pin_d0  = 15; cfg.pin_d1  = 14; cfg.pin_d2  = 13;
            cfg.pin_d3  = 12; cfg.pin_d4  = 11; cfg.pin_d5  = 10;
            cfg.pin_d6  =  9; cfg.pin_d7  =  8; cfg.pin_d8  =  7;
            cfg.pin_d9  =  6; cfg.pin_d10 =  5; cfg.pin_d11 =  4;
            cfg.pin_d12 =  3; cfg.pin_d13 =  2; cfg.pin_d14 =  1;
            cfg.pin_d15 =  0;
            cfg.hsync_polarity    = 0; cfg.hsync_front_porch = 10;
            cfg.hsync_pulse_width = 8; cfg.hsync_back_porch  = 50;
            cfg.vsync_polarity    = 0; cfg.vsync_front_porch = 10;
            cfg.vsync_pulse_width = 8; cfg.vsync_back_porch  = 20;
            cfg.pclk_active_neg   = 0; cfg.de_idle_high      = 0;
            cfg.pclk_idle_high    = 0;
            _rgb.config(cfg);
            _st7701.setBus(&_rgb);
        }
        {
            auto cfg = _st7701.config();
            cfg.pin_cs = -1; cfg.pin_rst = -1; cfg.pin_busy = -1;
            cfg.panel_width  = 480; cfg.panel_height = 480;
            cfg.readable = false; cfg.invert = false;
            cfg.rgb_order = false; cfg.dlen_16bit = false;
            cfg.bus_shared = false;
            _st7701.config(cfg);
        }
        {
            auto cfg = _st7701.config_detail();
            cfg.pin_sclk  = 41;
            cfg.pin_mosi  = 48;
            cfg.pin_cs    = -1;
            cfg.use_psram = 2;
            _st7701.config_detail(cfg);
        }
        setPanel(&_st7701);
    }
};

static LGFX_SenseCAP lcd;

// ─── PCA9535 ──────────────────────────────────────────────────────────────────
uint8_t pca9535_out0 = 0xFF;

static void pca9535_write() {
    Wire.beginTransmission(PCA9535_ADDR);
    Wire.write(0x02);
    Wire.write(pca9535_out0);
    Wire.endTransmission();
}

static void pca9535_init() {
    Wire.beginTransmission(PCA9535_ADDR);
    Wire.write(0x06);
    Wire.write(~(PCA9535_LCD_CS_BIT | PCA9535_LCD_RST_BIT | PCA9535_TP_RST_BIT));
    Wire.endTransmission();

    pca9535_out0 &= ~PCA9535_TP_RST_BIT; pca9535_write(); delay(10);
    pca9535_out0 |=  PCA9535_TP_RST_BIT; pca9535_write(); delay(200);
    pca9535_out0 &= ~PCA9535_LCD_RST_BIT; pca9535_write(); delay(40);
    pca9535_out0 |=  PCA9535_LCD_RST_BIT; pca9535_write(); delay(140);
    pca9535_out0 &= ~PCA9535_LCD_CS_BIT;  pca9535_write();
}

void pca9535_write_bit(int bit, bool value) {
    if (value) pca9535_out0 |=  (1 << bit);
    else       pca9535_out0 &= ~(1 << bit);
    pca9535_write();
}

bool pca9535_read_bit(int bit) {
    Wire.beginTransmission(PCA9535_ADDR);
    Wire.write(0x00);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)PCA9535_ADDR, (uint8_t)1);
    if (Wire.available()) return ((Wire.read() >> bit) & 1);
    return false;
}

// ─── UiMode & menu types ─────────────────────────────────────────────────────
enum UiMode {
    UI_CHAT, UI_MENU_TOP, UI_MENU_LIST,
    UI_EDIT_STR, UI_EDIT_NUM, UI_EDIT_DROP,
    UI_ROUTING, UI_PEERS, UI_MONITOR, UI_ABOUT,
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

// ─── Datenstrukturen ──────────────────────────────────────────────────────────
struct ChatLine {
    char call[MAX_CALLSIGN_LENGTH + 8];
    char text[INPUT_MAX_LEN + 1];
    char group[MAX_CALLSIGN_LENGTH + 1];
    bool own;
};

static ChatLine history[MAX_HISTORY];
static int      historyCount = 0;
static int      historyHead  = 0;

static char monLines[MON_HISTORY][MON_LINE_W];
static int  monHead  = 0;
static int  monCount = 0;

// Groups
static int  groupCount  = 0;
static int  groupUnread[MAX_GROUPS]  = {0};
static bool groupMute[MAX_GROUPS]    = {false};
static bool groupInSammel[MAX_GROUPS]= {false};
static int  sammelGroupIdx           = -1;   // Index of collection group, -1 = none
static int  activeGroup = -1;

// Keyboard & input
static bool keyboardVisible = false;
static bool kbdNumMode      = false;

static char inputBuf[INPUT_MAX_LEN + 1] = {0};
static int  inputLen = 0;

static char editStrBuf[INPUT_MAX_LEN + 1] = {0};
static int  editStrLen = 0;

// Pointer to the active buffer (chat or edit)
static char* kbdBuf = inputBuf;
static int*  kbdLen = &inputLen;
static int   kbdMax = INPUT_MAX_LEN;

// Menu state
static UiMode    uiMode      = UI_CHAT;
static int       topSel      = 0;
static int       listSel     = 0;
static int       listScroll  = 0;
static MenuItem* curMenu     = nullptr;
static int       curMenuLen  = 0;
static int       editItemIdx = -1;
static float     editFloat   = 0.0f;
static int       editDropIdx = 0;
static int       infoScroll  = 0;
static int       newGroupSlot = -1;
static char      setupChipId[13] = {0};

static bool     needRedraw = true;
static int      lastMinute = -1;

// View state: 0=Messages 1=Peers 2=Routes (only in UI_CHAT)
static int currentView = 0;

// ─── Forward Declarations ─────────────────────────────────────────────────────
static void doSave();
static void doSaveSetup();
static void doReboot();
static void doUpdate();
static void doForceUpdateRelease();
static void doForceUpdateDev();
static void doSaveGroups();
static void doNewGroup();
static void doAnnounce();
static void buildGroupMenu();
static void deleteGroup(int idx);
static void fullRedraw();

// ─── Dropdown options ────────────────────────────────────────────────────────
static const DropF bwOpts[] = {
    {"7 kHz",7.0f},{"10,4 kHz",10.4f},{"15,6 kHz",15.6f},
    {"20,8 kHz",20.8f},{"31,25 kHz",31.25f},{"62,5 kHz",62.5f},
    {"125 kHz",125.0f},{"250 kHz",250.0f},{"500 kHz",500.0f},
};
static const DropI crOpts[] = {
    {"5 - Standard",5},{"6 - Erhoehter Schutz",6},
    {"7 - Hoher Schutz",7},{"8 - Maximaler Schutz",8},
};
static const DropI sfOpts[] = {
    {"6 - Gering (Sichtverbindung)",6},{"7 - Gut (Standard)",7},
    {"8 - Besser",8},{"9 - Sehr gut",9},{"10 - Exzellent",10},
    {"11 - Maximum",11},{"12 - Deep Indoor",12},
};

// ─── IP helper functions ─────────────────────────────────────────────────────
static char tmpPeerIP[5][16];

static void ipToStr(IPAddress& ip, char* buf, int len) {
    snprintf(buf, len, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}
static void strToIP(const char* s, IPAddress& ip) {
    unsigned a=0,b=0,c=0,d=0;
    sscanf(s, "%u.%u.%u.%u", &a, &b, &c, &d);
    ip = IPAddress(a,b,c,d);
}

// ─── Menu arrays ─────────────────────────────────────────────────────────────
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
    {"Frequenz",           FTYPE_FLOAT,   &settings.loraFrequency,       0, nullptr, "MHz",    0.01f, 0.f, 0.f, nullptr},
    {"Sendeleistung",      FTYPE_INT8,    &settings.loraOutputPower,     0, nullptr, "dBm",     1.0f, 0.f, 0.f, nullptr},
    {"Bandbreite",         FTYPE_DROP_F,  &settings.loraBandwidth,       9, bwOpts,  nullptr,   0.f, 0.f, 0.f, nullptr},
    {"Coding Rate",        FTYPE_DROP_I,  &settings.loraCodingRate,      4, crOpts,  nullptr,   0.f, 0.f, 0.f, nullptr},
    {"Spreading Factor",   FTYPE_DROP_I,  &settings.loraSpreadingFactor, 7, sfOpts,  nullptr,   0.f, 0.f, 0.f, nullptr},
    {"Sync Word",          FTYPE_HEX8,    &settings.loraSyncWord,        0, nullptr, nullptr,   0.f, 0.f, 0.f, nullptr},
    {"Preamble Laenge",    FTYPE_INT16,   &settings.loraPreambleLength,  0, nullptr, nullptr,   1.f, 0.f, 0.f, nullptr},
    {"Nachr. wiederholen", FTYPE_BOOL,    &settings.loraRepeat,          0, nullptr, nullptr,   0.f, 0.f, 0.f, nullptr},
    {"Max. Nachr.-Laenge", FTYPE_READONLY,&settings.loraMaxMessageLength,0, nullptr, "Zeichen", 0.f, 0.f, 0.f, nullptr},
    {"Speichern",          FTYPE_ACTION,  nullptr,                       0, nullptr, nullptr,   0.f, 0.f, 0.f, doSave},
};
static MenuItem setupItems[] = {
    {"Rufzeichen",      FTYPE_STRING,       settings.mycall,   16, nullptr, nullptr, 0.f, 0.f, 0.f, nullptr},
    {"Position",        FTYPE_STRING,       settings.position, 23, nullptr, nullptr, 0.f, 0.f, 0.f, nullptr},
    {"Chip ID",         FTYPE_READONLY_STR, setupChipId,        0, nullptr, nullptr, 0.f, 0.f, 0.f, nullptr},
    {"Speichern",       FTYPE_ACTION,       nullptr,            0, nullptr, nullptr, 0.f, 0.f, 0.f, doSaveSetup},
    {"Neustart",        FTYPE_ACTION,       nullptr,            0, nullptr, nullptr, 0.f, 0.f, 0.f, doReboot},
    {"Update",          FTYPE_ACTION,       nullptr,            0, nullptr, nullptr, 0.f, 0.f, 0.f, doUpdate},
    {"Update Release",  FTYPE_ACTION,       nullptr,            0, nullptr, nullptr, 0.f, 0.f, 0.f, doForceUpdateRelease},
    {"Update Dev",      FTYPE_ACTION,       nullptr,            0, nullptr, nullptr, 0.f, 0.f, 0.f, doForceUpdateDev},
    {"Nachr. loeschen", FTYPE_ACTION,       nullptr,            0, nullptr, nullptr, 0.f, 0.f, 0.f, nullptr},
};

static MenuItem groupItemsBuf[MAX_GROUPS * 4 + 2];
static char     groupLabelBufs[MAX_GROUPS][16];
static char     groupMuteLbls [MAX_GROUPS][16];
static char     groupSamLbls  [MAX_GROUPS][16];
static int      groupItemsLen = 0;

// ─── Ring-Buffer ──────────────────────────────────────────────────────────────
static void addLine(const char* call, const char* text, bool own, const char* grp) {
    int idx = historyHead % MAX_HISTORY;
    strncpy(history[idx].call,  call,  sizeof(history[idx].call) - 1);
    strncpy(history[idx].text,  text,  sizeof(history[idx].text) - 1);
    strncpy(history[idx].group, grp ? grp : "", sizeof(history[idx].group) - 1);
    history[idx].call[sizeof(history[idx].call)-1]   = '\0';
    history[idx].text[sizeof(history[idx].text)-1]   = '\0';
    history[idx].group[sizeof(history[idx].group)-1] = '\0';
    history[idx].own = own;
    historyHead++;
    if (historyCount < MAX_HISTORY) historyCount++;
    needRedraw = true;
}

static void addMonLine(const char* line) {
    int idx = monHead % MON_HISTORY;
    strncpy(monLines[idx], line, MON_LINE_W - 1);
    monLines[idx][MON_LINE_W - 1] = '\0';
    monHead++;
    if (monCount < MON_HISTORY) monCount++;
}

static void fmtAge(time_t t, char* buf, size_t bufLen) {
    time_t age = time(NULL) - t;
    if (age < 0) age = 0;
    if      (age < 60)    snprintf(buf, bufLen, "%llds", (long long)age);
    else if (age < 3600)  snprintf(buf, bufLen, "%lldm", (long long)(age/60));
    else if (age < 86400) snprintf(buf, bufLen, "%lldh", (long long)(age/3600));
    else                  snprintf(buf, bufLen, "%lldd", (long long)(age/86400));
}

// ─── Menu actions ────────────────────────────────────────────────────────────
static void doSave() {
    // Transfer first 5 entries from display UI into dynamic vector
    // Trim empty / 0.0.0.0 entries at the end, keep existing legacy flags
    IPAddress parsedIPs[5];
    for (int i = 0; i < 5; i++) strToIP(tmpPeerIP[i], parsedIPs[i]);
    // Rebuild vector: remember existing legacy flags for the first 5 slots
    bool legacyBak[5] = {};
    for (int i = 0; i < 5 && (size_t)i < udpPeerLegacy.size(); i++) legacyBak[i] = (bool)udpPeerLegacy[i];
    // Rescue peers >5 from old vector
    std::vector<IPAddress> tail;
    std::vector<bool> tailLegacy;
    for (size_t i = 5; i < udpPeers.size(); i++) { tail.push_back(udpPeers[i]); tailLegacy.push_back((bool)udpPeerLegacy[i]); }
    udpPeers.clear(); udpPeerLegacy.clear();
    for (int i = 0; i < 5; i++) {
        if (parsedIPs[i] != IPAddress(0,0,0,0)) { udpPeers.push_back(parsedIPs[i]); udpPeerLegacy.push_back(legacyBak[i]); }
    }
    for (size_t i = 0; i < tail.size(); i++) { udpPeers.push_back(tail[i]); udpPeerLegacy.push_back(tailLegacy[i]); }
    saveSettings();
    uiMode = UI_CHAT; needRedraw = true;
}
static void doSaveSetup() {
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
        char key[8];   snprintf(key,  sizeof(key),  "grp%d",    i); prefs.putString(key, groupNames[i]);
        char mkey[10]; snprintf(mkey, sizeof(mkey), "grpMute%d", i); prefs.putUChar(mkey, groupMute[i] ? 1 : 0);
        char skey[10]; snprintf(skey, sizeof(skey), "grpInSm%d", i); prefs.putUChar(skey, groupInSammel[i] ? 1 : 0);
    }
    for (int i = groupCount; i < MAX_GROUPS; i++) {
        char key[8];   snprintf(key,  sizeof(key),  "grp%d",    i); prefs.remove(key);
        char mkey[10]; snprintf(mkey, sizeof(mkey), "grpMute%d", i); prefs.remove(mkey);
        char skey[10]; snprintf(skey, sizeof(skey), "grpInSm%d", i); prefs.remove(skey);
    }
    uiMode = UI_CHAT; needRedraw = true;
}
static void doAnnounce() {
    Frame f;
    f.frameType = Frame::FrameTypes::ANNOUNCE_FRAME;
    f.transmitMillis = 0;
    f.port = 0; txBuffer.push_back(f);
    f.port = 1; txBuffer.push_back(f);
    announceTimer = millis() + ANNOUNCE_TIME;
    uiMode = UI_CHAT; needRedraw = true;
}

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
    for (int i = idx; i < groupCount - 1; i++) {
        strncpy(groupNames[i], groupNames[i+1], MAX_CALLSIGN_LENGTH);
        groupUnread[i]   = groupUnread[i+1];
        groupMute[i]     = groupMute[i+1];
        groupInSammel[i] = groupInSammel[i+1];
    }
    groupCount--;
    groupNames[groupCount][0] = '\0';
    groupUnread[groupCount]   = 0;
    groupMute[groupCount]     = false;
    groupInSammel[groupCount] = false;
    // Adjust sammelGroupIdx
    if (sammelGroupIdx == idx)     sammelGroupIdx = -1;
    else if (sammelGroupIdx > idx) sammelGroupIdx--;
    if (activeGroup == idx)     activeGroup = -1;
    else if (activeGroup > idx) activeGroup--;
    doSaveGroups();
    buildGroupMenu();
    curMenuLen = groupItemsLen;
    if (listSel >= curMenuLen) listSel = max(0, curMenuLen - 1);
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
    editItemIdx = newGroupSlot * 4;
    editStrBuf[0] = '\0'; editStrLen = 0;
    kbdBuf = editStrBuf; kbdLen = &editStrLen; kbdMax = MAX_CALLSIGN_LENGTH;
    keyboardVisible = true; kbdNumMode = false;
    uiMode = UI_EDIT_STR; needRedraw = true;
}

// ─── Touch-Polling ────────────────────────────────────────────────────────────
static uint8_t touchAddr = TOUCH_ADDR;

static bool touchRead(int16_t &tx, int16_t &ty) {
    Wire.beginTransmission(touchAddr);
    Wire.write(0x00);
    if (Wire.endTransmission(false) != 0) return false;
    Wire.requestFrom(touchAddr, (uint8_t)7);
    if (Wire.available() < 7) { while (Wire.available()) Wire.read(); return false; }
    Wire.read(); Wire.read();
    uint8_t td = Wire.read();
    uint8_t xh = Wire.read(); uint8_t xl = Wire.read();
    uint8_t yh = Wire.read(); uint8_t yl = Wire.read();
    if ((td & 0x0F) == 0) return false;
    int16_t rx = ((int16_t)(xh & 0x0F) << 8) | xl;
    int16_t ry = ((int16_t)(yh & 0x0F) << 8) | yl;
    tx = DISP_W - 1 - rx;
    ty = DISP_H - 1 - ry;
    return true;
}

// ─── Zeichenhilfen ────────────────────────────────────────────────────────────
static void drawStr(const char* str, int32_t x, int32_t y) {
    char buf[300]; utf8ToCP437(str, buf, sizeof(buf));
    lcd.drawString(buf, x, y);
}

static void fmtValue(char* buf, int buflen, MenuItem& item) {
    switch (item.type) {
        case FTYPE_BOOL:
            snprintf(buf, buflen, "%s", *(bool*)item.ptr ? "[X]" : "[ ]"); break;
        case FTYPE_STRING: case FTYPE_IP:
            snprintf(buf, buflen, "%s", (char*)item.ptr); break;
        case FTYPE_FLOAT: {
            int dec = (item.step >= 1.0f) ? 0 : (fabsf(item.step-0.1f)<0.05f) ? 1 : 3;
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
        default: buf[0] = 0; break;
    }
}

// ─── Header bar ──────────────────────────────────────────────────────────────
static void drawHeader(bool menuOpen) {
    lcd.fillRect(0, 0, DISP_W, HDR_H, COL_HDR_BG);
    lcd.setTextColor(COL_HDR_FG, COL_HDR_BG);
    lcd.setTextSize(1);
    drawStr(settings.mycall[0] ? settings.mycall : "---", 2, 1);

    const char* wifiSt = (WiFi.status() == WL_CONNECTED) ? "WiFi" : "---";
    int wx = (DISP_W - (int)strlen(wifiSt) * 6) / 2;
    lcd.drawString(wifiSt, wx, 1);

    time_t now = time(nullptr); struct tm tm; localtime_r(&now, &tm);
    char tbuf[6]; snprintf(tbuf, sizeof(tbuf), "%02d:%02d", tm.tm_hour, tm.tm_min);
    lcd.drawString(tbuf, DISP_W - 70, 1);

    // Menu button (hamburger icon) — drawn as pixel bars
    uint32_t mbg = menuOpen ? COL_NAV_ACT_BG : COL_HDR_BG;
    uint32_t mfg = menuOpen ? COL_NAV_ACT_FG : 0xFFFFu;
    lcd.fillRect(DISP_W - 28, 0, 28, HDR_H, mbg);
    // 3 horizontal bars at y = 3, 8, 13
    lcd.fillRect(DISP_W - 24, 3,  20, 3, mfg);
    lcd.fillRect(DISP_W - 24, 8,  20, 3, mfg);
    lcd.fillRect(DISP_W - 24, 13, 20, 2, mfg);
}

// ─── Group tabs ──────────────────────────────────────────────────────────────
static void drawTabs() {
    lcd.fillRect(0, HDR_H, DISP_W, TAB_H, COL_TAB_BG);
    lcd.setTextSize(2);  // larger for touch

    int tabList[MAX_GROUPS + 1]; int tabCount = 0;
    tabList[tabCount++] = -1;
    for (int i = 0; i < groupCount; i++)
        if (strlen(groupNames[i]) > 0 && !groupInSammel[i]) tabList[tabCount++] = i;

    int tabW = DISP_W / tabCount;
    for (int i = 0; i < tabCount; i++) {
        int gIdx = tabList[i];
        bool active = (currentView == 0) && (gIdx == activeGroup);
        bool unread = (gIdx >= 0 && groupUnread[gIdx] > 0);

        int tx = i * tabW;
        uint32_t bg = unread ? COL_UNREAD_BG : (active ? COL_TAB_ACT_BG : COL_TAB_BG);
        uint32_t fg = unread ? COL_UNREAD_FG : (active ? COL_TAB_ACT_FG : COL_TAB_FG);
        lcd.fillRect(tx, HDR_H, tabW - 1, TAB_H, bg);
        lcd.setTextColor(fg, bg);

        const char* label = (gIdx < 0) ? "Alle" : groupNames[gIdx];
        char lbuf[16]; utf8ToCP437(label, lbuf, sizeof(lbuf));
        int lw = strlen(lbuf) * 12;  // textSize 2 = 12px per char
        int lx = tx + (tabW - lw) / 2;
        lcd.drawString(lbuf, lx, HDR_H + (TAB_H - 16) / 2);
    }
    lcd.setTextSize(1);
}

// ─── Navigation bar ──────────────────────────────────────────────────────────
static void drawNavBar() {
    const int third = DISP_W / 3;  // 160px
    lcd.setTextSize(1);

    // Peers
    bool peersAct = (currentView == 1);
    uint32_t bg0 = peersAct ? COL_NAV_ACT_BG : COL_NAV_BG;
    uint32_t fg0 = peersAct ? COL_NAV_ACT_FG : COL_NAV_FG;
    lcd.fillRect(0, NAV_Y, third - 1, NAV_H, bg0);
    lcd.setTextColor(fg0, bg0);
    lcd.setTextSize(2);
    { int lw = 5*12; lcd.drawString("Peers", (third-lw)/2, NAV_Y+(NAV_H-16)/2); }

    lcd.drawFastVLine(third, NAV_Y, NAV_H, COL_SEPARATOR);

    // Routen
    bool routesAct = (currentView == 2);
    uint32_t bg1 = routesAct ? COL_NAV_ACT_BG : COL_NAV_BG;
    uint32_t fg1 = routesAct ? COL_NAV_ACT_FG : COL_NAV_FG;
    lcd.fillRect(third+1, NAV_Y, third-1, NAV_H, bg1);
    lcd.setTextColor(fg1, bg1);
    { int lw = 6*12; lcd.drawString("Routen", third+1+(third-lw)/2, NAV_Y+(NAV_H-16)/2); }

    lcd.drawFastVLine(2*third, NAV_Y, NAV_H, COL_SEPARATOR);

    // Keyboard toggle
    lcd.fillRect(2*third+1, NAV_Y, third-1, NAV_H, COL_NAV_BG);
    lcd.setTextColor(keyboardVisible ? COL_NAV_ACT_FG : COL_NAV_FG,
                     keyboardVisible ? COL_NAV_ACT_BG : COL_NAV_BG);
    if (keyboardVisible) lcd.fillRect(2*third+1, NAV_Y, third-1, NAV_H, COL_NAV_ACT_BG);
    // Keyboard icon: simple symbol
    lcd.setTextSize(1);
    const char* kbdIcon = keyboardVisible ? "[ v ]" : "[ ^ ]";
    int kw = strlen(kbdIcon) * 6;
    lcd.drawString(kbdIcon, 2*third+1+(third-kw)/2, NAV_Y+(NAV_H-8)/2);

    lcd.setTextSize(1);
}

// ─── Menu navigation bar (back) ──────────────────────────────────────────────
static void drawMenuNavBack(const char* hint = "< Zurueck") {
    lcd.fillRect(0, NAV_Y, DISP_W, NAV_H, COL_MENU_HDR);
    lcd.drawFastHLine(0, NAV_Y, DISP_W, COL_SEPARATOR);
    lcd.setTextColor(COL_MENU_FG, COL_MENU_HDR);
    lcd.setTextSize(2);
    int lw = strlen(hint) * 12;
    lcd.drawString(hint, (DISP_W - lw) / 2, NAV_Y + (NAV_H - 16) / 2);
    lcd.setTextSize(1);
}

// ─── Input line ──────────────────────────────────────────────────────────────
static void drawInputBar(int iy) {
    lcd.fillRect(0, iy, DISP_W, INPUT_H, COL_INPUT_BG);
    lcd.drawFastHLine(0, iy, DISP_W, COL_SEPARATOR);
    lcd.setTextColor(COL_INPUT_FG, COL_INPUT_BG);
    lcd.setTextSize(1);

    char prompt[20];
    if (uiMode == UI_EDIT_STR && editItemIdx >= 0)
        snprintf(prompt, sizeof(prompt), "%s: ", curMenu[editItemIdx].label);
    else if (activeGroup >= 0 && strlen(groupNames[activeGroup]) > 0)
        snprintf(prompt, sizeof(prompt), "[%s]>", groupNames[activeGroup]);
    else
        snprintf(prompt, sizeof(prompt), ">");

    char dispLine[INPUT_MAX_LEN + 24];
    char* buf = kbdBuf;
    snprintf(dispLine, sizeof(dispLine), "%s%s", prompt, buf);
    // Show only the last ~78 characters (480px / 6px = 80 characters)
    int dispLen = strlen(dispLine);
    int maxChars = (DISP_W - 8) / 6;
    const char* showStart = dispLine;
    if (dispLen > maxChars) showStart = dispLine + dispLen - maxChars;
    lcd.drawString(showStart, 4, iy + (INPUT_H - 8) / 2);

    // Cursor
    int promptLen = strlen(prompt);
    int cursorX = 4 + (promptLen + *kbdLen) * 6;
    if (cursorX > DISP_W - 8) cursorX = DISP_W - 8;
    lcd.drawFastVLine(cursorX, iy + 4, INPUT_H - 8, COL_CURSOR);
}

// ─── Software keyboard ───────────────────────────────────────────────────────
static const char* kbdRowsQWERTY[3] = {"QWERTYUIOP", "ASDFGHJKL\x7F", "ZXCVBNM,.~"};
static const char* kbdRowsNum[3]    = {"1234567890", "-_+=/:.,!\x7F", "@#$%&*()?~"};
// \x7F = Backspace marker,  ~ = Enter marker

static void drawKeyboard() {
    const int kx = 0;
    lcd.fillRect(kx, KBD_Y, DISP_W, KBD_H, COL_KBD_BG);
    lcd.setTextSize(2);

    const char** rows = kbdNumMode ? kbdRowsNum : kbdRowsQWERTY;
    const int keyW = 48;  // DISP_W/10 = 48px

    for (int row = 0; row < 3; row++) {
        int y = KBD_Y + row * KBD_ROW_H;
        const char* r = rows[row];
        int len = strlen(r);
        for (int k = 0; k < len; k++) {
            int x = kx + k * keyW;
            bool isBs = (r[k] == '\x7F');
            bool isEnt = (r[k] == '~');
            uint32_t kbg = (isBs || isEnt) ? COL_KBD_SP_BG : COL_KBD_KEY_BG;
            lcd.fillRect(x+1, y+1, keyW-2, KBD_ROW_H-2, kbg);
            lcd.setTextColor(COL_KBD_KEY_FG, kbg);
            char label[4];
            if (isBs) { label[0]='\x7F'; label[1]=0; }  // CP437 DEL symbol ≈ ⌫
            else if (isEnt) { strcpy(label, "<-"); }
            else { label[0]=r[k]; label[1]=0; }
            int lw = strlen(label) * 12;
            lcd.drawString(label, x + (keyW - lw) / 2, y + (KBD_ROW_H - 16) / 2);
        }
    }

    // Row 3: [123/ABC] [SPACE x5] [DEL] [SEND]
    int y3 = KBD_Y + 3 * KBD_ROW_H;
    // 123/ABC (96px)
    lcd.fillRect(kx+1,   y3+1, 95, KBD_ROW_H-2, COL_KBD_SP_BG);
    lcd.setTextColor(COL_KBD_KEY_FG, COL_KBD_SP_BG);
    const char* modeLbl = kbdNumMode ? "ABC" : "123";
    int mw = strlen(modeLbl)*12;
    lcd.drawString(modeLbl, kx + (96-mw)/2, y3+(KBD_ROW_H-16)/2);
    // SPACE (240px)
    lcd.fillRect(kx+97,  y3+1, 238, KBD_ROW_H-2, COL_KBD_KEY_BG);
    lcd.setTextColor(COL_KBD_KEY_FG, COL_KBD_KEY_BG);
    lcd.drawString("___", kx+97+(238-36)/2, y3+(KBD_ROW_H-16)/2);
    // DEL (48px)
    lcd.fillRect(kx+336, y3+1, 46, KBD_ROW_H-2, COL_KBD_SP_BG);
    lcd.setTextColor(COL_KBD_KEY_FG, COL_KBD_SP_BG);
    lcd.drawString("\x7F", kx+336+(48-12)/2, y3+(KBD_ROW_H-16)/2);
    // SEND/OK (96px)
    lcd.fillRect(kx+383, y3+1, 96, KBD_ROW_H-2, COL_KBD_SEND_BG);
    lcd.setTextColor(COL_KBD_KEY_FG, COL_KBD_SEND_BG);
    const char* sendLbl = (uiMode == UI_EDIT_STR) ? "OK" : "SEND";
    int sw = strlen(sendLbl)*12;
    lcd.drawString(sendLbl, kx+383+(96-sw)/2, y3+(KBD_ROW_H-16)/2);

    lcd.setTextSize(1);
}

// ─── Process keyboard input ──────────────────────────────────────────────────
static void handleKbdChar(char c) {
    if (*kbdLen >= kbdMax) return;
    kbdBuf[(*kbdLen)++] = c;
    kbdBuf[*kbdLen] = '\0';
    needRedraw = true;
}
static void handleKbdBackspace() {
    if (*kbdLen > 0) { kbdBuf[--(*kbdLen)] = '\0'; needRedraw = true; }
}

static void handleKbdSend();  // forward decl

static void handleKbdTap(int16_t tx, int16_t ty) {
    if (ty < KBD_Y || ty >= NAV_Y) return;
    int row = (ty - KBD_Y) / KBD_ROW_H;
    if (row < 0 || row >= 4) return;

    if (row == 3) {
        // Sonderzeile
        if (tx < 96) { kbdNumMode = !kbdNumMode; needRedraw = true; }
        else if (tx < 336) { handleKbdChar(' '); }
        else if (tx < 384) { handleKbdBackspace(); }
        else { handleKbdSend(); }
        return;
    }

    const char** rows = kbdNumMode ? kbdRowsNum : kbdRowsQWERTY;
    const char* r = rows[row];
    int col = tx / 48;
    int len = strlen(r);
    if (col < 0 || col >= len) return;
    char c = r[col];
    if (c == '\x7F') { handleKbdBackspace(); }
    else if (c == '~') { handleKbdSend(); }
    else { handleKbdChar(c); }
}

static void confirmEditStr() {
    if (newGroupSlot >= 0) {
        strncpy(groupNames[newGroupSlot], editStrBuf, MAX_CALLSIGN_LENGTH);
        groupNames[newGroupSlot][MAX_CALLSIGN_LENGTH] = '\0';
        if (strlen(groupNames[newGroupSlot]) == 0) groupCount--;
        else doSaveGroups();
        newGroupSlot = -1;
        buildGroupMenu(); curMenuLen = groupItemsLen;
        if (listSel >= curMenuLen) listSel = max(0, curMenuLen - 1);
    } else if (editItemIdx >= 0 && curMenu) {
        MenuItem& item = curMenu[editItemIdx];
        if (item.type == FTYPE_HEX8) {
            unsigned v = 0;
            sscanf(editStrBuf, "%x", &v);
            *(uint8_t*)item.ptr = (uint8_t)v;
        } else {
            int maxLen = item.aux;
            strncpy((char*)item.ptr, editStrBuf, maxLen);
            ((char*)item.ptr)[maxLen] = '\0';
        }
    }
    kbdBuf = inputBuf; kbdLen = &inputLen; kbdMax = INPUT_MAX_LEN;
    keyboardVisible = false;
    uiMode = UI_MENU_LIST; needRedraw = true;
}

static void handleKbdSend() {
    if (uiMode == UI_EDIT_STR) {
        confirmEditStr();
        return;
    }
    // Chat-Modus: Nachricht senden
    if (inputLen == 0) return;
    inputBuf[inputLen] = '\0';

    // Check callsign
    if (strlen(settings.mycall) == 0) {
        addLine("!", "Kein Rufzeichen! Bitte in Setup konfigurieren.", false, "");
        needRedraw = true;
        return;
    }

    if (activeGroup >= 0 && strlen(groupNames[activeGroup]) > 0) {
        sendGroup(groupNames[activeGroup], inputBuf);
        char lbl[MAX_CALLSIGN_LENGTH + 10];
        snprintf(lbl, sizeof(lbl), "%s:", settings.mycall);
        addLine(lbl, inputBuf, true, groupNames[activeGroup]);
    } else {
        sendMessage("", inputBuf);
        char lbl[MAX_CALLSIGN_LENGTH + 10];
        snprintf(lbl, sizeof(lbl), "%s:", settings.mycall);
        addLine(lbl, inputBuf, true, "");
    }
    inputBuf[0] = '\0'; inputLen = 0;
    needRedraw = true;
}

// ─── Message area ────────────────────────────────────────────────────────────
static void drawMessages(int msgH) {
    lcd.fillRect(0, MSG_Y, DISP_W, msgH, COL_BG);
    lcd.setTextSize(1);
    const int lineH = 10;
    int maxLines = msgH / lineH;

    int indices[MAX_HISTORY]; int count = 0;
    for (int i = historyCount - 1; i >= 0 && count < MAX_HISTORY; i--) {
        int idx = ((historyHead - 1 - i) % MAX_HISTORY + MAX_HISTORY) % MAX_HISTORY;
        if (activeGroup < 0) {
            indices[count++] = idx;
        } else {
            const char* grpName = groupNames[activeGroup];
            if (strcmp(history[idx].group, grpName) == 0) indices[count++] = idx;
        }
    }

    int start = (count > maxLines) ? count - maxLines : 0;
    int y = MSG_Y + (maxLines - (count - start)) * lineH;

    for (int i = start; i < count; i++, y += lineH) {
        int idx = indices[count - 1 - i];
        const ChatLine& cl = history[idx];
        lcd.setTextColor(cl.own ? COL_OWN : COL_RX, COL_BG);
        char cbuf[MAX_CALLSIGN_LENGTH + 3]; utf8ToCP437(cl.call, cbuf, sizeof(cbuf));
        lcd.drawString(cbuf, 2, y);
        int callW = strlen(cbuf) * 6 + 4;
        lcd.setTextColor(0xFFFFu, COL_BG);
        char tbuf[INPUT_MAX_LEN + 1]; utf8ToCP437(cl.text, tbuf, sizeof(tbuf));
        // Wrapping: at 480px - callW (max 78 characters per line)
        int avail = (DISP_W - 2 - callW) / 6;
        lcd.drawString(tbuf, 2 + callW, y);
    }
}

// ─── Peer list ───────────────────────────────────────────────────────────────
static void drawPeerList() {
    lcd.fillRect(0, MSG_Y, DISP_W, MSG_H, COL_BG);
    lcd.setTextSize(1);
    const int lineH = 20;
    int y = MSG_Y + 4;
    time_t now = time(nullptr);

    // Spaltenheader
    lcd.setTextColor(0x7BEFu, COL_BG);
    lcd.drawString("Rufzeichen   Typ    RSSI    SNR   Alter", 4, y); y += lineH;
    lcd.drawFastHLine(0, y - 2, DISP_W, COL_SEPARATOR);

    if (peerList.empty()) {
        lcd.setTextColor(COL_NAV_FG, COL_BG);
        lcd.drawString("(keine Peers bekannt)", 4, y); return;
    }
    char buf[80], tbuf[80];
    for (size_t i = 0; i < peerList.size() && y < MSG_Y + MSG_H - lineH; i++) {
        const Peer& p = peerList[i];
        char age[10]; fmtAge(p.timestamp, age, sizeof(age));
        lcd.setTextColor(p.available ? 0x07E0u : 0xAD55u, COL_BG);
        snprintf(buf, sizeof(buf), "%-12s %-6s %6.1f %6.1f %s",
                 p.nodeCall, p.port==0?"LoRa":"WiFi", p.rssi, p.snr, age);
        utf8ToCP437(buf, tbuf, sizeof(tbuf));
        lcd.drawString(tbuf, 4, y); y += lineH;
    }
}

// ─── Route list ──────────────────────────────────────────────────────────────
static void drawRouteList() {
    lcd.fillRect(0, MSG_Y, DISP_W, MSG_H, COL_BG);
    lcd.setTextSize(1);
    const int lineH = 20;
    int y = MSG_Y + 4;

    lcd.setTextColor(0x7BEFu, COL_BG);
    lcd.drawString("Rufzeichen   Via          Hops  Alter", 4, y); y += lineH;
    lcd.drawFastHLine(0, y - 2, DISP_W, COL_SEPARATOR);

    if (routingList.empty()) {
        lcd.setTextColor(COL_NAV_FG, COL_BG);
        lcd.drawString("(keine Routen bekannt)", 4, y); return;
    }
    char buf[80], tbuf[80];
    time_t now = time(nullptr);
    for (size_t i = 0; i < routingList.size() && y < MSG_Y + MSG_H - lineH; i++) {
        const Route& r = routingList[i];
        char age[10]; fmtAge(r.timestamp, age, sizeof(age));
        lcd.setTextColor(0xFFFFu, COL_BG);
        snprintf(buf, sizeof(buf), "%-12s %-12s %4d  %s",
                 r.srcCall, r.viaCall, r.hopCount, age);
        utf8ToCP437(buf, tbuf, sizeof(tbuf));
        lcd.drawString(tbuf, 4, y); y += lineH;
    }
}

// ─── Monitor-Ansicht ──────────────────────────────────────────────────────────
static void drawMonitor() {
    lcd.fillScreen(COL_BG);
    // Header
    lcd.fillRect(0, 0, DISP_W, MENU_HDR_H, COL_MENU_HDR);
    lcd.setTextColor(COL_MENU_HDR_FG, COL_MENU_HDR);
    lcd.setTextSize(2);
    lcd.drawString("rMesh > Monitor", 4, (MENU_HDR_H-16)/2);
    lcd.setTextSize(1);

    const int areaY = MENU_HDR_H;
    const int areaH = NAV_Y - areaY;
    const int lineH = 12;
    const int vis   = areaH / lineH;

    int clampedScroll = max(0, min(infoScroll, monCount > vis ? monCount - vis : 0));
    int visEnd  = monCount - clampedScroll;
    int start   = max(0, visEnd - vis);

    lcd.setTextColor(COL_MENU_FG, COL_BG);
    for (int i = start; i < visEnd; i++) {
        int ri = (monHead - monCount + i + MON_HISTORY) % MON_HISTORY;
        lcd.drawString(monLines[ri], 4, areaY + (i - start) * lineH);
    }
    if (monCount == 0) {
        lcd.setTextColor(COL_NAV_FG, COL_BG);
        lcd.drawString("(kein Traffic)", 4, areaY + areaH/2);
    }
    drawMenuNavBack("< Zurueck");
}

// ─── About-Ansicht ────────────────────────────────────────────────────────────
static void drawAbout() {
    lcd.fillScreen(COL_BG);
    lcd.fillRect(0, 0, DISP_W, MENU_HDR_H, COL_MENU_HDR);
    lcd.setTextColor(COL_MENU_HDR_FG, COL_MENU_HDR);
    lcd.setTextSize(2);
    lcd.drawString("rMesh > About", 4, (MENU_HDR_H-16)/2);
    lcd.setTextSize(1);

    const int lineH = 22;
    int y = MENU_HDR_H + 10;

    lcd.setTextColor(0x7BEFu, COL_BG);
    lcd.drawString("Version:", 4, y);
    lcd.setTextColor(COL_MENU_FG, COL_BG);
    lcd.drawString(VERSION, 90, y); y += lineH;

    lcd.setTextColor(0x7BEFu, COL_BG);
    lcd.drawString("Hardware:", 4, y);
    lcd.setTextColor(COL_MENU_FG, COL_BG);
    lcd.drawString(PIO_ENV_NAME, 90, y); y += lineH;

    char ipBuf[20] = "-";
    if (WiFi.status() == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    }
    lcd.setTextColor(0x7BEFu, COL_BG);
    lcd.drawString("WiFi IP:", 4, y);
    lcd.setTextColor(COL_MENU_FG, COL_BG);
    lcd.drawString(ipBuf, 90, y); y += lineH;

    lcd.setTextColor(0x7BEFu, COL_BG);
    lcd.drawString("Chip ID:", 4, y);
    lcd.setTextColor(COL_MENU_FG, COL_BG);
    lcd.drawString(setupChipId, 90, y); y += lineH + 6;

    lcd.drawFastHLine(4, y, DISP_W-8, COL_SEPARATOR); y += 10;
    lcd.setTextColor(0x07FFu, COL_BG);
    lcd.drawString("www.rMesh.de", 4, y); y += lineH;
    lcd.drawString("github.com/DN9KGB/rMesh", 4, y);

    drawMenuNavBack("< Zurueck");
}

// ─── Menu top level (3x3 grid) ───────────────────────────────────────────────
#define TOP_MENU_N  9
static const char* topMenuLabels[TOP_MENU_N] = {
    "Network","LoRa","Setup","Gruppen","Routing","Peers","Monitor","Announce","About"
};
#define TILE_W  (DISP_W / 3)    // 160px
#define TILE_H  80

static void drawMenuTop() {
    lcd.fillScreen(COL_BG);
    lcd.fillRect(0, 0, DISP_W, MENU_HDR_H, COL_MENU_HDR);
    lcd.setTextColor(COL_MENU_HDR_FG, COL_MENU_HDR);
    lcd.setTextSize(2);
    lcd.drawString("rMesh  Menue", 4, (MENU_HDR_H-16)/2);
    lcd.setTextSize(1);

    for (int i = 0; i < TOP_MENU_N; i++) {
        int col = i % 3;
        int row = i / 3;
        int x = col * TILE_W;
        int y = MENU_HDR_H + row * TILE_H;

        bool sel = (i == topSel);
        uint32_t bg = sel ? COL_MENU_SEL : COL_MENU_EDIT_BG;
        uint32_t fg = sel ? COL_MENU_SEL_FG : COL_MENU_FG;
        lcd.fillRect(x+2, y+2, TILE_W-4, TILE_H-4, bg);
        lcd.drawRect(x+2, y+2, TILE_W-4, TILE_H-4, sel ? COL_MENU_SEL_FG : COL_SEPARATOR);
        lcd.setTextColor(fg, bg);
        lcd.setTextSize(1);
        const char* lbl = topMenuLabels[i];
        int lw = strlen(lbl) * 6;
        lcd.drawString(lbl, x + (TILE_W-lw)/2, y + (TILE_H-8)/2);
    }

    drawMenuNavBack("< Zurueck");
}

// ─── Menu item list ──────────────────────────────────────────────────────────
static void drawMenuList() {
    lcd.fillScreen(COL_BG);
    lcd.fillRect(0, 0, DISP_W, MENU_HDR_H, COL_MENU_HDR);
    lcd.setTextColor(COL_MENU_HDR_FG, COL_MENU_HDR);
    lcd.setTextSize(2);
    char title[48];
    snprintf(title, sizeof(title), "rMesh > %s", topMenuLabels[topSel]);
    char tbuf[48]; utf8ToCP437(title, tbuf, sizeof(tbuf));
    lcd.drawString(tbuf, 4, (MENU_HDR_H-16)/2);
    lcd.setTextSize(1);

    int end = min(listScroll + MENU_ITEMS_VIS, curMenuLen);
    for (int i = listScroll; i < end; i++) {
        int y = MENU_HDR_H + (i - listScroll) * MENU_ITEM_H;
        bool sel = (i == listSel);
        if (sel) lcd.fillRect(0, y, DISP_W, MENU_ITEM_H, COL_MENU_SEL);
        else     lcd.fillRect(0, y, DISP_W, MENU_ITEM_H, COL_BG);
        lcd.drawFastHLine(0, y + MENU_ITEM_H - 1, DISP_W, COL_SEPARATOR);
        MenuItem& item = curMenu[i];

        if (item.type == FTYPE_ACTION) {
            lcd.setTextColor(sel ? COL_MENU_SEL_FG : COL_MENU_HDR_FG, sel ? COL_MENU_SEL : COL_BG);
            lcd.setTextSize(2);
            char albl[32]; utf8ToCP437(item.label, albl, sizeof(albl));
            int lw = strlen(albl) * 12;
            lcd.drawString(albl, (DISP_W-lw)/2, y + (MENU_ITEM_H-16)/2);
            lcd.setTextSize(1);
        } else if (item.type == FTYPE_DELETE_GROUP) {
            lcd.fillRect(DISP_W-120, y+4, 116, MENU_ITEM_H-8, sel ? 0xF800u : COL_DELETE_BG);
            lcd.setTextColor(COL_DELETE_FG, sel ? 0xF800u : COL_DELETE_BG);
            lcd.setTextSize(2);
            lcd.drawString("Losch.", DISP_W-110, y+(MENU_ITEM_H-16)/2);
            lcd.setTextSize(1);
        } else {
            lcd.setTextColor(sel ? COL_MENU_SEL_FG : COL_MENU_FG, sel ? COL_MENU_SEL : COL_BG);
            lcd.setTextSize(2);
            char lbl[48]; utf8ToCP437(item.label, lbl, sizeof(lbl));
            lcd.drawString(lbl, 6, y + (MENU_ITEM_H-16)/2);
            char val[80]; fmtValue(val, sizeof(val), item);
            if (strlen(val) > 0) {
                char vbuf[80]; utf8ToCP437(val, vbuf, sizeof(vbuf));
                int vw = strlen(vbuf) * 12;
                int vx = DISP_W - vw - 6;
                if (vx < DISP_W/2) vx = DISP_W/2;
                lcd.setTextColor(sel ? COL_MENU_SEL_FG : COL_MENU_VAL, sel ? COL_MENU_SEL : COL_BG);
                lcd.drawString(vbuf, vx, y + (MENU_ITEM_H-16)/2);
            }
            lcd.setTextSize(1);
        }
    }

    // Scrollbar
    if (curMenuLen > MENU_ITEMS_VIS) {
        int sbH = MENU_AREA_H * MENU_ITEMS_VIS / curMenuLen;
        int sbY = MENU_HDR_H + MENU_AREA_H * listScroll / curMenuLen;
        lcd.fillRect(DISP_W-4, sbY, 4, sbH, COL_MENU_SEL);
    }

    drawMenuNavBack("< Zurueck");
}

// ─── Edit: string (with keyboard) ────────────────────────────────────────────
static void drawEditStr() {
    lcd.fillScreen(COL_BG);
    lcd.fillRect(0, 0, DISP_W, MENU_HDR_H, COL_MENU_HDR);
    lcd.setTextColor(COL_MENU_HDR_FG, COL_MENU_HDR);
    lcd.setTextSize(2);
    char title[64];
    if (newGroupSlot >= 0) snprintf(title, sizeof(title), "Neue Gruppe");
    else if (editItemIdx >= 0) snprintf(title, sizeof(title), "Bearb.: %s", curMenu[editItemIdx].label);
    else snprintf(title, sizeof(title), "Eingabe");
    char tbuf[64]; utf8ToCP437(title, tbuf, sizeof(tbuf));
    lcd.drawString(tbuf, 4, (MENU_HDR_H-16)/2);
    lcd.setTextSize(1);

    // Input field
    int iy = INPUT_Y_KBD;
    drawInputBar(iy);
    drawKeyboard();
    drawMenuNavBack("Abbr.");
}

// ─── Edit: number (+/- buttons) ──────────────────────────────────────────────
static void drawEditNum() {
    lcd.fillScreen(COL_BG);
    lcd.fillRect(0, 0, DISP_W, MENU_HDR_H, COL_MENU_HDR);
    lcd.setTextColor(COL_MENU_HDR_FG, COL_MENU_HDR);
    lcd.setTextSize(2);
    if (editItemIdx >= 0) {
        char lbl[48]; utf8ToCP437(curMenu[editItemIdx].label, lbl, sizeof(lbl));
        lcd.drawString(lbl, 4, (MENU_HDR_H-16)/2);
    }
    lcd.setTextSize(1);

    // Display value
    MenuItem& item = curMenu[editItemIdx];
    int dec = (item.step >= 1.0f) ? 0 : (fabsf(item.step-0.1f)<0.05f) ? 1 : 3;
    char fmt[12]; snprintf(fmt, sizeof(fmt), "%%.%df", dec);
    char numStr[32]; snprintf(numStr, sizeof(numStr), fmt, editFloat);
    char valStr[48];
    if (item.unit) snprintf(valStr, sizeof(valStr), "%s %s", numStr, item.unit);
    else           snprintf(valStr, sizeof(valStr), "%s", numStr);

    lcd.setTextColor(COL_MENU_VAL, COL_BG);
    lcd.setTextSize(3);
    int vw = strlen(valStr) * 18;
    lcd.drawString(valStr, (DISP_W-vw)/2, DISP_H/2 - 24);
    lcd.setTextSize(1);

    // +/- buttons
    const int btnW = 140, btnH = 60, btnY = DISP_H/2 + 20;
    // --- (large step)
    lcd.fillRoundRect(10, btnY, btnW, btnH, 8, COL_MENU_EDIT_BG);
    lcd.setTextColor(COL_MENU_FG, COL_MENU_EDIT_BG);
    lcd.setTextSize(3);
    lcd.drawString("--", 10+(btnW-36)/2, btnY+(btnH-24)/2);
    // -
    lcd.fillRoundRect(10+btnW+10, btnY, btnW, btnH, 8, COL_MENU_EDIT_BG);
    lcd.setTextColor(COL_MENU_FG, COL_MENU_EDIT_BG);
    lcd.drawString("-", 10+btnW+10+(btnW-18)/2, btnY+(btnH-24)/2);
    // +
    lcd.fillRoundRect(DISP_W-10-btnW-10-btnW, btnY, btnW, btnH, 8, COL_MENU_EDIT_BG);
    lcd.setTextColor(COL_MENU_FG, COL_MENU_EDIT_BG);
    lcd.drawString("+", DISP_W-10-btnW-10-btnW+(btnW-18)/2, btnY+(btnH-24)/2);
    // ++ (large step)
    lcd.fillRoundRect(DISP_W-10-btnW, btnY, btnW, btnH, 8, COL_MENU_EDIT_BG);
    lcd.setTextColor(COL_MENU_FG, COL_MENU_EDIT_BG);
    lcd.drawString("++", DISP_W-10-btnW+(btnW-36)/2, btnY+(btnH-24)/2);
    lcd.setTextSize(1);

    // Nav bar: Abbrechen | OK
    lcd.fillRect(0, NAV_Y, DISP_W, NAV_H, COL_MENU_HDR);
    lcd.drawFastHLine(0, NAV_Y, DISP_W, COL_SEPARATOR);
    lcd.setTextSize(2);
    lcd.setTextColor(COL_MENU_FG, COL_MENU_HDR);
    lcd.drawString("Abbr.", 10, NAV_Y+(NAV_H-16)/2);
    lcd.setTextColor(COL_KBD_SEND_BG-1, COL_MENU_HDR);
    lcd.setTextColor(0x07E0u, COL_MENU_HDR);
    lcd.drawString("OK", DISP_W-40, NAV_Y+(NAV_H-16)/2);
    lcd.setTextSize(1);
}

// ─── Edit: dropdown ──────────────────────────────────────────────────────────
static void drawEditDrop() {
    lcd.fillScreen(COL_BG);
    lcd.fillRect(0, 0, DISP_W, MENU_HDR_H, COL_MENU_HDR);
    lcd.setTextColor(COL_MENU_HDR_FG, COL_MENU_HDR);
    lcd.setTextSize(2);
    if (editItemIdx >= 0) {
        char lbl[48]; utf8ToCP437(curMenu[editItemIdx].label, lbl, sizeof(lbl));
        lcd.drawString(lbl, 4, (MENU_HDR_H-16)/2);
    }
    lcd.setTextSize(1);

    MenuItem& item = curMenu[editItemIdx];
    int numOpts = item.aux;
    const int itemH = 56;
    int vis = (NAV_Y - MENU_HDR_H) / itemH;

    int scroll = editDropIdx - vis/2;
    if (scroll < 0) scroll = 0;
    if (scroll > numOpts - vis) scroll = max(0, numOpts - vis);

    for (int i = scroll; i < scroll+vis && i < numOpts; i++) {
        int y = MENU_HDR_H + (i-scroll)*itemH;
        bool sel = (i == editDropIdx);
        lcd.fillRect(0, y, DISP_W, itemH-1, sel ? COL_MENU_SEL : COL_BG);
        lcd.drawFastHLine(0, y+itemH-1, DISP_W, COL_SEPARATOR);
        const char* lbl = (item.type == FTYPE_DROP_F)
            ? ((const DropF*)item.opts)[i].label
            : ((const DropI*)item.opts)[i].label;
        lcd.setTextColor(sel ? COL_MENU_SEL_FG : COL_MENU_FG, sel ? COL_MENU_SEL : COL_BG);
        lcd.setTextSize(2);
        char lb[48]; utf8ToCP437(lbl, lb, sizeof(lb));
        lcd.drawString(lb, 10, y+(itemH-16)/2);
        lcd.setTextSize(1);
    }

    drawMenuNavBack("Abbr.");
}

// ─── Menu navigation ─────────────────────────────────────────────────────────
static void openMenu() {
    for (int i = 0; i < 5; i++) {
        if ((size_t)i < udpPeers.size()) ipToStr(udpPeers[i], tmpPeerIP[i], sizeof(tmpPeerIP[i]));
        else strncpy(tmpPeerIP[i], "0.0.0.0", sizeof(tmpPeerIP[i]));
    }
    topSel = 0; uiMode = UI_MENU_TOP; needRedraw = true;
}

static void enterSubmenu(int idx) {
    topSel = idx;
    switch (idx) {
        case 0: curMenu = netItems;   curMenuLen = sizeof(netItems)/sizeof(netItems[0]);    break;
        case 1: curMenu = loraItems;  curMenuLen = sizeof(loraItems)/sizeof(loraItems[0]);   break;
        case 2: curMenu = setupItems; curMenuLen = sizeof(setupItems)/sizeof(setupItems[0]); break;
        case 3:
            buildGroupMenu();
            curMenu = groupItemsBuf; curMenuLen = groupItemsLen; break;
        case 4: infoScroll = 0; uiMode = UI_ROUTING; needRedraw = true; return;
        case 5: infoScroll = 0; uiMode = UI_PEERS;   needRedraw = true; return;
        case 6: infoScroll = 0; uiMode = UI_MONITOR; needRedraw = true; return;
        case 7: doAnnounce(); return;
        case 8: uiMode = UI_ABOUT;   needRedraw = true; return;
        default: return;
    }
    listSel = 0; listScroll = 0; uiMode = UI_MENU_LIST; needRedraw = true;
}

static void activateItem() {
    if (!curMenu || listSel < 0 || listSel >= curMenuLen) return;
    MenuItem& item = curMenu[listSel];
    editItemIdx = listSel;
    switch (item.type) {
        case FTYPE_BOOL:
            *(bool*)item.ptr = !*(bool*)item.ptr; needRedraw = true; break;
        case FTYPE_STRING: case FTYPE_IP:
            strncpy(editStrBuf, (char*)item.ptr, item.aux);
            editStrBuf[item.aux] = '\0'; editStrLen = strlen(editStrBuf);
            kbdBuf = editStrBuf; kbdLen = &editStrLen; kbdMax = item.aux;
            keyboardVisible = true; kbdNumMode = (item.type == FTYPE_IP);
            uiMode = UI_EDIT_STR; needRedraw = true; break;
        case FTYPE_HEX8:
            snprintf(editStrBuf, sizeof(editStrBuf), "%02X", *(uint8_t*)item.ptr);
            editStrLen = strlen(editStrBuf);
            kbdBuf = editStrBuf; kbdLen = &editStrLen; kbdMax = 2;
            keyboardVisible = true; kbdNumMode = true;
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
        case FTYPE_DELETE_GROUP: deleteGroup(item.aux); break;
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
        case FTYPE_ACTION: if (item.action) item.action(); break;
    }
}

static void confirmEditNum() {
    if (!curMenu || editItemIdx < 0) return;
    MenuItem& item = curMenu[editItemIdx];
    if (item.minVal != 0.f || item.maxVal != 0.f) {
        if (editFloat < item.minVal) editFloat = item.minVal;
        if (editFloat > item.maxVal) editFloat = item.maxVal;
    }
    switch (item.type) {
        case FTYPE_FLOAT:  *(float*)  item.ptr = editFloat; break;
        case FTYPE_INT8:   *(int8_t*) item.ptr = (int8_t)editFloat;  break;
        case FTYPE_INT16:  *(int16_t*)item.ptr = (int16_t)editFloat; break;
        case FTYPE_UINT8:  *(uint8_t*)item.ptr = (uint8_t)editFloat; break;
        default: break;
    }
    uiMode = UI_MENU_LIST; needRedraw = true;
}

static void confirmEditDrop() {
    if (!curMenu || editItemIdx < 0) return;
    MenuItem& item = curMenu[editItemIdx];
    if (item.type == FTYPE_DROP_F) {
        *(float*)item.ptr = ((const DropF*)item.opts)[editDropIdx].v;
    } else {
        *(uint8_t*)item.ptr = (uint8_t)((const DropI*)item.opts)[editDropIdx].v;
    }
    uiMode = UI_MENU_LIST; needRedraw = true;
}

// ─── Full redraw ─────────────────────────────────────────────────────────────
static void fullRedraw() {
    switch (uiMode) {
        case UI_MENU_TOP:
            drawMenuTop();
            break;
        case UI_MENU_LIST:
            drawMenuList();
            break;
        case UI_EDIT_STR:
            drawEditStr();
            break;
        case UI_EDIT_NUM:
            drawEditNum();
            break;
        case UI_EDIT_DROP:
            drawEditDrop();
            break;
        case UI_ROUTING:
            // Compact routing list
            lcd.fillScreen(COL_BG);
            lcd.fillRect(0, 0, DISP_W, MENU_HDR_H, COL_MENU_HDR);
            lcd.setTextColor(COL_MENU_HDR_FG, COL_MENU_HDR);
            lcd.setTextSize(2); lcd.drawString("rMesh > Routing", 4, (MENU_HDR_H-16)/2); lcd.setTextSize(1);
            {
                const int lineH = 20; int y = MENU_HDR_H + 4;
                lcd.setTextColor(0x7BEFu, COL_BG);
                lcd.drawString("Rufzeichen   Via          Hops  Alter", 4, y); y += lineH;
                lcd.drawFastHLine(0, y-2, DISP_W, COL_SEPARATOR);
                if (routingList.empty()) {
                    lcd.setTextColor(COL_NAV_FG, COL_BG); lcd.drawString("(keine Eintraege)", 4, y);
                }
                char buf[80], tbuf[80];
                for (size_t i = infoScroll; i < routingList.size() && y < NAV_Y - lineH; i++) {
                    const Route& r = routingList[i];
                    char age[10]; fmtAge(r.timestamp, age, sizeof(age));
                    lcd.setTextColor(0xFFFFu, COL_BG);
                    snprintf(buf, sizeof(buf), "%-12s %-12s %4d  %s", r.srcCall, r.viaCall, r.hopCount, age);
                    utf8ToCP437(buf, tbuf, sizeof(tbuf));
                    lcd.drawString(tbuf, 4, y); y += lineH;
                }
            }
            drawMenuNavBack("< Zurueck");
            break;
        case UI_PEERS:
            lcd.fillScreen(COL_BG);
            lcd.fillRect(0, 0, DISP_W, MENU_HDR_H, COL_MENU_HDR);
            lcd.setTextColor(COL_MENU_HDR_FG, COL_MENU_HDR);
            lcd.setTextSize(2); lcd.drawString("rMesh > Peers", 4, (MENU_HDR_H-16)/2); lcd.setTextSize(1);
            {
                const int lineH = 20; int y = MENU_HDR_H + 4;
                lcd.setTextColor(0x7BEFu, COL_BG);
                lcd.drawString("Rufzeichen   Typ    RSSI    SNR   Alter", 4, y); y += lineH;
                lcd.drawFastHLine(0, y-2, DISP_W, COL_SEPARATOR);
                if (peerList.empty()) {
                    lcd.setTextColor(COL_NAV_FG, COL_BG); lcd.drawString("(keine Eintraege)", 4, y);
                }
                char buf[80], tbuf[80];
                for (size_t i = infoScroll; i < peerList.size() && y < NAV_Y - lineH; i++) {
                    const Peer& p = peerList[i];
                    char age[10]; fmtAge(p.timestamp, age, sizeof(age));
                    lcd.setTextColor(p.available ? 0x07E0u : 0xAD55u, COL_BG);
                    snprintf(buf, sizeof(buf), "%-12s %-6s %6.1f %6.1f %s",
                             p.nodeCall, p.port==0?"LoRa":"WiFi", p.rssi, p.snr, age);
                    utf8ToCP437(buf, tbuf, sizeof(tbuf));
                    lcd.drawString(tbuf, 4, y); y += lineH;
                }
            }
            drawMenuNavBack("< Zurueck");
            break;
        case UI_MONITOR:
            drawMonitor();
            break;
        case UI_ABOUT:
            drawAbout();
            break;
        default:  // UI_CHAT
            drawHeader(false);
            drawTabs();
            switch (currentView) {
                case 1: drawPeerList();  break;
                case 2: drawRouteList(); break;
                default:
                    drawMessages(keyboardVisible ? MSG_H_KBD : MSG_H);
                    break;
            }
            drawInputBar(keyboardVisible ? INPUT_Y_KBD : INPUT_Y);
            if (keyboardVisible && currentView == 0) drawKeyboard();
            drawNavBar();
            break;
    }
    needRedraw = false;
}

// ─── Touch-Handler ────────────────────────────────────────────────────────────
static void handleTouch(int16_t tx, int16_t ty) {
    switch (uiMode) {

    case UI_CHAT:
        // Nav-Leiste
        if (ty >= NAV_Y) {
            const int third = DISP_W / 3;
            if (tx < third) {
                // Peers
                if (currentView != 1) { currentView = 1; needRedraw = true; }
                else { currentView = 0; needRedraw = true; }
            } else if (tx < 2*third) {
                // Routen
                if (currentView != 2) { currentView = 2; needRedraw = true; }
                else { currentView = 0; needRedraw = true; }
            } else {
                // Keyboard toggle (only in messages view)
                if (currentView == 0) {
                    keyboardVisible = !keyboardVisible;
                    kbdBuf = inputBuf; kbdLen = &inputLen; kbdMax = INPUT_MAX_LEN;
                    needRedraw = true;
                }
            }
            return;
        }
        // Keyboard
        if (keyboardVisible && ty >= KBD_Y && ty < NAV_Y && currentView == 0) {
            handleKbdTap(tx, ty);
            needRedraw = true;
            return;
        }
        // Groups-Tabs
        if (ty >= HDR_H && ty < HDR_H + TAB_H) {
            int tabList[MAX_GROUPS + 1]; int tabCount = 0;
            tabList[tabCount++] = -1;
            for (int i = 0; i < groupCount; i++)
                if (strlen(groupNames[i]) > 0) tabList[tabCount++] = i;
            int tabW = DISP_W / tabCount;
            int tapped = tx / tabW;
            if (tapped < tabCount) {
                int gIdx = tabList[tapped];
                currentView = 0;
                if (gIdx != activeGroup) {
                    activeGroup = gIdx;
                    if (gIdx >= 0) groupUnread[gIdx] = 0;
                }
                needRedraw = true;
            }
            return;
        }
        // Header: menu button
        if (ty < HDR_H && tx >= DISP_W - 28) {
            openMenu();
            return;
        }
        break;

    case UI_MENU_TOP:
        // Kachel antippen
        if (ty >= MENU_HDR_H && ty < NAV_Y) {
            int col = tx / TILE_W;
            int row = (ty - MENU_HDR_H) / TILE_H;
            int idx = row * 3 + col;
            if (idx >= 0 && idx < TOP_MENU_N) {
                topSel = idx;
                enterSubmenu(idx);
            }
        } else if (ty >= NAV_Y) {
            // Back
            uiMode = UI_CHAT; needRedraw = true;
        }
        break;

    case UI_MENU_LIST:
        if (ty >= NAV_Y) {
            uiMode = UI_MENU_TOP; needRedraw = true;
        } else if (ty >= MENU_HDR_H) {
            int tappedItem = listScroll + (ty - MENU_HDR_H) / MENU_ITEM_H;
            if (tappedItem >= 0 && tappedItem < curMenuLen) {
                if (tappedItem == listSel) {
                    activateItem();
                } else {
                    listSel = tappedItem;
                    if (listSel < listScroll) listScroll = listSel;
                    if (listSel >= listScroll + MENU_ITEMS_VIS)
                        listScroll = listSel - MENU_ITEMS_VIS + 1;
                    needRedraw = true;
                }
            }
        }
        break;

    case UI_EDIT_STR:
        if (ty >= NAV_Y) {
            // Abbrechen
            newGroupSlot = -1;
            kbdBuf = inputBuf; kbdLen = &inputLen; kbdMax = INPUT_MAX_LEN;
            keyboardVisible = false;
            uiMode = UI_MENU_LIST; needRedraw = true;
        } else if (ty >= KBD_Y) {
            handleKbdTap(tx, ty);
            needRedraw = true;
        }
        break;

    case UI_EDIT_NUM: {
        const int btnW = 140, btnH = 60, btnY = DISP_H/2 + 20;
        MenuItem& item = curMenu[editItemIdx];
        float step = item.step > 0.f ? item.step : 1.f;
        if (ty >= btnY && ty < btnY + btnH) {
            if      (tx < 10+btnW)             editFloat -= step * 10;
            else if (tx < 10+btnW+10+btnW)     editFloat -= step;
            else if (tx < DISP_W-10-btnW)      editFloat += step;
            else                               editFloat += step * 10;
            if (item.minVal != 0.f || item.maxVal != 0.f) {
                if (editFloat < item.minVal) editFloat = item.minVal;
                if (editFloat > item.maxVal) editFloat = item.maxVal;
            }
            needRedraw = true;
        } else if (ty >= NAV_Y) {
            if (tx < DISP_W/2) {
                // Abbrechen
                uiMode = UI_MENU_LIST; needRedraw = true;
            } else {
                confirmEditNum();
            }
        }
        break;
    }

    case UI_EDIT_DROP: {
        MenuItem& item = curMenu[editItemIdx];
        const int itemH = 56;
        int vis = (NAV_Y - MENU_HDR_H) / itemH;
        int scroll = editDropIdx - vis/2;
        if (scroll < 0) scroll = 0;
        if (scroll > item.aux - vis) scroll = max(0, item.aux - vis);

        if (ty >= NAV_Y) {
            uiMode = UI_MENU_LIST; needRedraw = true;
        } else if (ty >= MENU_HDR_H) {
            int tapped = scroll + (ty - MENU_HDR_H) / itemH;
            if (tapped >= 0 && tapped < item.aux) {
                if (tapped == editDropIdx) {
                    confirmEditDrop();
                } else {
                    editDropIdx = tapped;
                    needRedraw = true;
                }
            }
        }
        break;
    }

    case UI_ROUTING:
    case UI_PEERS:
    case UI_MONITOR:
    case UI_ABOUT:
        if (ty >= NAV_Y) {
            uiMode = UI_CHAT; currentView = 0; needRedraw = true;
        }
        break;
    }
}

// ─── Load groups from NVS ────────────────────────────────────────────────────
static void loadGroups() {
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
}

// ─── Public API ──────────────────────────────────────────────────────────────
void initDisplay() {
    Wire.begin(SENSECAP_I2C_SDA, SENSECAP_I2C_SCL, 400000);
    pca9535_init();

    // I2C-Scan: Touch-Controller-Adresse
    logPrintf(LOG_INFO, "Touch", "I2C-Scan starting...");
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            logPrintf(LOG_INFO, "Touch", "Found device at 0x%02X", addr);
            if (addr == 0x38 || addr == 0x3B || addr == 0x48) touchAddr = addr;
        }
    }
    logPrintf(LOG_INFO, "Touch", "Touch@0x%02X", touchAddr);

    lcd.init();
    lcd.setRotation(2);
    lcd.setBrightness(displayBrightness);

    pinMode(LCD_BL_PIN, OUTPUT);
    digitalWrite(LCD_BL_PIN, HIGH);

    lcd.fillScreen(COL_BG);
    lcd.setFont(&fonts::Font0);
    lcd.setTextSize(1);

    // Determine chip ID
    uint64_t mac = ESP.getEfuseMac();
    snprintf(setupChipId, sizeof(setupChipId), "%02X%02X%02X%02X%02X%02X",
        (uint8_t)(mac>>40),(uint8_t)(mac>>32),(uint8_t)(mac>>24),
        (uint8_t)(mac>>16),(uint8_t)(mac>>8),(uint8_t)mac);

    // Groups laden
    loadGroups();

    // Initiales UI zeichnen
    fullRedraw();

    // LCD_CS high: LoRa-SPI nutzt GPIO41/48 — Kollision vermeiden
    pca9535_write_bit(4, true);

    logPrintf(LOG_INFO, "Display", "SenseCAP Indicator ST7701S 480x480 bereit.");
}

void displayUpdateLoop() {
    // Uhrzeitaktualisierung
    time_t now = time(nullptr); struct tm tmNow; localtime_r(&now, &tmNow);
    if (tmNow.tm_min != lastMinute) {
        lastMinute = tmNow.tm_min;
        if (uiMode == UI_CHAT) needRedraw = true;
    }

    // Touch polling every 20 ms
    static uint32_t lastTouchMs = 0;
    static bool     wasTouched  = false;
    uint32_t ms = millis();
    if (ms - lastTouchMs >= 20) {
        lastTouchMs = ms;
        int16_t tx, ty;
        bool touched = touchRead(tx, ty);
        if (touched && !wasTouched) {
            handleTouch(tx, ty);
        }
        wasTouched = touched;
    }

    // Update peers/routes every 5 s
    static uint32_t lastInfoRefreshMs = 0;
    if ((currentView != 0 || uiMode == UI_ROUTING || uiMode == UI_PEERS || uiMode == UI_MONITOR)
        && ms - lastInfoRefreshMs >= 5000) {
        lastInfoRefreshMs = ms;
        needRedraw = true;
    }

    if (needRedraw) fullRedraw();
}

void displayOnNewMessage(const char* srcCall, const char* text,
                         const char* dstGroup, const char* dstCall) {
    char label[MAX_CALLSIGN_LENGTH + 8];
    if (dstCall && strlen(dstCall) > 0)
        snprintf(label, sizeof(label), "%s>%s:", srcCall, dstCall);
    else if (dstGroup && strlen(dstGroup) > 0)
        snprintf(label, sizeof(label), "%s[%s]:", srcCall, dstGroup);
    else
        snprintf(label, sizeof(label), "%s:", srcCall);

    const char* grp = (dstGroup && strlen(dstGroup) > 0) ? dstGroup : "";

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
    addLine(label, text, false, storeGrp);

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
    if (dstCall && strlen(dstCall) > 0)
        snprintf(label, sizeof(label), ">%s:", dstCall);
    else
        snprintf(label, sizeof(label), "TX:");
    addLine(label, text, true, "");
}

void displayMonitorFrame(const Frame& f) {
    char line[MON_LINE_W];
    char typeStr[8];
    switch (f.frameType) {
        case Frame::MESSAGE_FRAME:      strcpy(typeStr, "MSG"); break;
        case Frame::MESSAGE_ACK_FRAME:  strcpy(typeStr, "ACK"); break;
        case Frame::ANNOUNCE_FRAME:     strcpy(typeStr, "ANN"); break;
        case Frame::ANNOUNCE_ACK_FRAME: strcpy(typeStr, "ANK"); break;
        default:                        strcpy(typeStr, "UNK"); break;
    }
    if (f.tx)
        snprintf(line, sizeof(line), "[TX %s] %s>%s", typeStr, f.nodeCall, f.dstCall);
    else
        snprintf(line, sizeof(line), "[RX %s] %s>%s R:%.0f", typeStr, f.nodeCall, f.dstCall, f.rssi);
    addMonLine(line);
}

#endif // SEEED_SENSECAP_INDICATOR
