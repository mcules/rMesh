#pragma once

#ifdef HAS_WIFI

void reportTopology();
void reportTopologyIfChanged();
void markTopologyChanged();
bool logRemoteCommand(const char* sender, const char* command);

extern volatile bool topologyChanged;

#else
// ── No-op stubs for non-WiFi builds ─────────────────────────────────────────

inline void reportTopology() {}
inline void reportTopologyIfChanged() {}
inline void markTopologyChanged() {}
inline bool logRemoteCommand(const char*, const char*) { return false; }

#endif
