#pragma once
#include <Arduino.h>

#ifdef HAS_WIFI

#define MAX_AUTH_SESSIONS 6

struct AuthSession {
    uint32_t clientId    = 0;
    bool     authenticated = false;
    char     nonce[33]   = {0};  // 16 random bytes als 32-char Hex + \0
};

extern AuthSession authSessions[MAX_AUTH_SESSIONS];
extern String      webPasswordHash;  // SHA-256(passwort) als Hex, leer = kein Schutz

void   loadPasswordHash();
void   savePasswordHash(const String& hash);
String generateNonce(uint32_t clientId);
bool   isAuthenticated(uint32_t clientId);
void   setClientAuth(uint32_t clientId, bool auth);
void   removeClientAuth(uint32_t clientId);
bool   verifyAuthResponse(uint32_t clientId, const String& response);

#else
// ── Stubs for non-WiFi builds ────────────────────────────────────────────────

extern String webPasswordHash;

inline void loadPasswordHash() {}
inline void savePasswordHash(const String&) {}

#endif
