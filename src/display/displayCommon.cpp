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
