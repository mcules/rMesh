#include "statusDisplay.h"
#include "bsp/IBoardConfig.h"
#include "mesh/frame.h"

// Weak no-op fallbacks.  Display drivers that own a screen override these
// with strong symbols in their own translation unit; on boards without
// such a driver the linker keeps these defaults so that platform-agnostic
// call sites still link.

__attribute__((weak)) bool initStatusDisplay(const IBoardConfig* /*board*/) { return false; }
__attribute__((weak)) void showStatusDisplaySplash(uint32_t /*holdMs*/) {}
__attribute__((weak)) void showStatusDisplayFlashing(const char* /*what*/) {}
__attribute__((weak)) void updateStatusDisplay() {}
__attribute__((weak)) void enableStatusDisplay() {}
__attribute__((weak)) void disableStatusDisplay() {}
__attribute__((weak)) bool hasStatusDisplay() { return false; }
__attribute__((weak)) void onStatusDisplayMessage(const char*, const char*, const char*, const char*) {}

__attribute__((weak)) void initDisplay() {}
__attribute__((weak)) void displayUpdateLoop() {}
__attribute__((weak)) void displayOnNewMessage(const char*, const char*, const char*, const char*) {}
__attribute__((weak)) void displayTxFrame(const char*, const char*) {}
__attribute__((weak)) void displayMonitorFrame(const Frame&) {}

__attribute__((weak)) void displayButtonPoll() {}

// ── Shared utilities ────────────────────────────────────────────────────────

#include "hal/settings.h"
#include <string.h>

void utf8ToCP437(const char* src, char* dst, size_t dstLen) {
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

bool matchesDisplayGroup(const char* dstGroup, const char* dstCall) {
    if (oledDisplayGroup[0] == '\0') return false;
    if (strcmp(oledDisplayGroup, "*") == 0)
        return (dstGroup[0] == '\0' && dstCall[0] == '\0');
    if (strcmp(oledDisplayGroup, "@DM") == 0)
        return (strcmp(dstCall, settings.mycall) == 0);
    return (strcmp(dstGroup, oledDisplayGroup) == 0);
}

__attribute__((weak)) bool pagerSdAvailable() { return false; }
__attribute__((weak)) void pagerAddMessageToSD(const char*, size_t) {}
