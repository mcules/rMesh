#pragma once

#ifdef HAS_WIFI

void reportTopology();
void reportTopologyIfChanged();
void markTopologyChanged();

extern bool topologyChanged;

#else
// ── No-op stubs for non-WiFi builds ─────────────────────────────────────────

inline void reportTopology() {}
inline void reportTopologyIfChanged() {}
inline void markTopologyChanged() {}

#endif
