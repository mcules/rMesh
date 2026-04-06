#pragma once

#include "frame.h"

#ifdef HAS_WIFI

void initUDP();
bool checkUDP(Frame &f);
void sendUDP(Frame &f);

#else
// ── No-op stubs for non-WiFi builds ─────────────────────────────────────────

inline void initUDP() {}
inline bool checkUDP(Frame &f) { (void)f; return false; }
inline void sendUDP(Frame &f) { (void)f; }

#endif
