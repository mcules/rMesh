#include "auth.h"

#ifdef HAS_WIFI

#include <Preferences.h>
#include "mbedtls/md.h"
#include <esp_random.h>

AuthSession authSessions[MAX_AUTH_SESSIONS];
String      webPasswordHash = "";

// ── Passwort-Hash aus NVS laden ───────────────────────────────────────────────
void loadPasswordHash() {
    Preferences p;
    p.begin("rmesh_auth", true);
    char buf[65] = {0};
    p.getString("webPwdHash", buf, sizeof(buf));
    p.end();
    webPasswordHash = String(buf);
    Serial.printf("[Auth] loadPasswordHash: '%s'\n", webPasswordHash.isEmpty() ? "(leer)" : "(gesetzt)");
}

// ── Passwort-Hash in NVS speichern ────────────────────────────────────────────
void savePasswordHash(const String& hash) {
    webPasswordHash = hash;
    Preferences p;
    p.begin("rmesh_auth", false);
    p.putString("webPwdHash", hash.c_str());
    p.end();
}

// ── Zufällige Nonce erzeugen und in der Session speichern ────────────────────
String generateNonce(uint32_t clientId) {
    uint8_t bytes[16];
    esp_fill_random(bytes, sizeof(bytes));
    char buf[33];
    for (int i = 0; i < 16; i++) sprintf(buf + 2 * i, "%02x", bytes[i]);
    buf[32] = '\0';

    for (int i = 0; i < MAX_AUTH_SESSIONS; i++) {
        if (authSessions[i].clientId == clientId) {
            memcpy(authSessions[i].nonce, buf, 33);
            break;
        }
    }
    return String(buf);
}

// ── Ist ein Client authentifiziert? ──────────────────────────────────────────
bool isAuthenticated(uint32_t clientId) {
    if (webPasswordHash.isEmpty()) return true;  // kein Passwort gesetzt
    for (int i = 0; i < MAX_AUTH_SESSIONS; i++) {
        if (authSessions[i].clientId == clientId)
            return authSessions[i].authenticated;
    }
    return false;
}

// ── Session anlegen oder aktualisieren ────────────────────────────────────────
void setClientAuth(uint32_t clientId, bool auth) {
    // vorhandene Session aktualisieren
    for (int i = 0; i < MAX_AUTH_SESSIONS; i++) {
        if (authSessions[i].clientId == clientId) {
            authSessions[i].authenticated = auth;
            return;
        }
    }
    // freien Slot belegen
    for (int i = 0; i < MAX_AUTH_SESSIONS; i++) {
        if (authSessions[i].clientId == 0) {
            authSessions[i].clientId      = clientId;
            authSessions[i].authenticated = auth;
            memset(authSessions[i].nonce, 0, sizeof(authSessions[i].nonce));
            return;
        }
    }
    // All slots full: evict the first unauthenticated session, or slot 0 as fallback
    int evict = 0;
    for (int i = 0; i < MAX_AUTH_SESSIONS; i++) {
        if (!authSessions[i].authenticated) { evict = i; break; }
    }
    authSessions[evict].clientId      = clientId;
    authSessions[evict].authenticated = auth;
    memset(authSessions[evict].nonce, 0, sizeof(authSessions[evict].nonce));
}

// ── Session entfernen (bei Disconnect) ───────────────────────────────────────
void removeClientAuth(uint32_t clientId) {
    for (int i = 0; i < MAX_AUTH_SESSIONS; i++) {
        if (authSessions[i].clientId == clientId) {
            authSessions[i].clientId      = 0;
            authSessions[i].authenticated = false;
            memset(authSessions[i].nonce, 0, sizeof(authSessions[i].nonce));
            return;
        }
    }
}

// ── HMAC-Antwort prüfen ───────────────────────────────────────────────────────
// Erwartet: HMAC-SHA256(key=SHA256(passwort), data=nonce) als 64-char Hex
bool verifyAuthResponse(uint32_t clientId, const String& response) {
    if (webPasswordHash.isEmpty()) return true;
    if (response.length() != 64)  return false;

    // Nonce für diesen Client suchen
    char nonce[33] = {0};
    for (int i = 0; i < MAX_AUTH_SESSIONS; i++) {
        if (authSessions[i].clientId == clientId) {
            memcpy(nonce, authSessions[i].nonce, 33);
            break;
        }
    }
    if (nonce[0] == '\0') return false;

    // Validate hash length before accessing individual characters
    if (webPasswordHash.length() < 64) return false;

    // gespeicherten Hash (Hex) → Bytes (= HMAC-Schlüssel)
    uint8_t keyBytes[32];
    for (int i = 0; i < 32; i++) {
        char hex[3] = {webPasswordHash[i * 2], webPasswordHash[i * 2 + 1], '\0'};
        keyBytes[i] = (uint8_t)strtoul(hex, nullptr, 16);
    }

    // HMAC-SHA256(key=gespeicherterHash, data=nonce) berechnen
    uint8_t hmacResult[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, keyBytes, 32);
    mbedtls_md_hmac_update(&ctx, (const uint8_t*)nonce, strlen(nonce));
    mbedtls_md_hmac_finish(&ctx, hmacResult);
    mbedtls_md_free(&ctx);

    // Ergebnis → Hex-String
    char expected[65];
    for (int i = 0; i < 32; i++) sprintf(expected + 2 * i, "%02x", hmacResult[i]);
    expected[64] = '\0';

    return response.equalsIgnoreCase(String(expected));
}

#else
// ── Stubs for non-WiFi builds ────────────────────────────────────────────────
String webPasswordHash = "";
#endif
