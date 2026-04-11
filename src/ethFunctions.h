#pragma once

#ifdef HAS_ETHERNET

#include <ETH.h>

void ethInit();

extern bool ethConnected;

#else
// ── No-op stubs for boards without Ethernet ─────────────────────────────────

inline void ethInit() {}
static bool ethConnected = false;

#endif
