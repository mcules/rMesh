#pragma once

#ifdef HAS_WIFI
#include <ESPAsyncWebServer.h>
#include "network/auth.h"

extern AsyncWebSocket ws;

void startWebServer();
void wsBroadcast(const char* buf, size_t len);

// Lightweight WebSocket notifications — tell the UI "data changed, re-fetch via API"
void notifyPeerListChanged();
void notifyRoutingChanged();
void notifySettingsChanged();
void notifyNewMessage();
void processBleJson(const char* data, size_t len);

#else
// ── No-op stubs for non-WiFi builds ─────────────────────────────────────────

inline void startWebServer() {}
inline void wsBroadcast(const char*, size_t) {}
inline void notifyPeerListChanged() {}
inline void notifyRoutingChanged() {}
inline void notifySettingsChanged() {}
inline void notifyNewMessage() {}
inline void processBleJson(const char*, size_t) {}

// Minimal stub so code using ws.textAll() can compile with #ifdef HAS_WIFI guards
#endif
