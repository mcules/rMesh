#pragma once

#ifdef HAS_WIFI

void wifiInit();
void showWiFiStatus();
void checkForUpdates(bool force = false, uint8_t forceChannel = 0);

// Flag: scan triggered for reconnect to pick best network from list
extern bool pendingReconnectScan;

#else
// ── No-op stubs for non-WiFi builds ─────────────────────────────────────────

inline void wifiInit() {}
inline void showWiFiStatus() {}
inline void checkForUpdates(bool force = false, uint8_t forceChannel = 0) {
    (void)force; (void)forceChannel;
}

static bool pendingReconnectScan = false;

#endif
