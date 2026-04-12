#pragma once

#ifdef HAS_WIFI

#include <WiFi.h>

void wifiInit();
void showWiFiStatus();
void checkForUpdates(bool force = false, uint8_t forceChannel = 0);
void onWiFiScanDone(WiFiEvent_t event, WiFiEventInfo_t info);
void onWiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);
void onWiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info);

// Flag: scan triggered for reconnect to pick best network from list
extern bool pendingReconnectScan;

// WiFi diagnostics counters (used by /api/diagnostics)
extern uint32_t wifiDisconnectCount;
extern uint8_t  lastWifiDisconnectReason;
extern uint32_t lastWifiDisconnectTime;

#else
// ── No-op stubs for non-WiFi builds ─────────────────────────────────────────

inline void wifiInit() {}
inline void showWiFiStatus() {}
inline void checkForUpdates(bool force = false, uint8_t forceChannel = 0) {
    (void)force; (void)forceChannel;
}

static bool pendingReconnectScan = false;

#endif
