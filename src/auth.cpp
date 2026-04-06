#include "auth.h"

#ifdef HAS_WIFI

#include <Preferences.h>
#include "mbedtls/md.h"
#include <esp_random.h>
#include "logging.h"

AuthSession authSessions[MAX_AUTH_SESSIONS];
String      webPasswordHash = "";

// ── Load password hash from NVS ───────────────────────────────────────────────
void loadPasswordHash() {
    Preferences p;
    p.begin("rmesh_auth", true);
    char buf[65] = {0};
    p.getString("webPwdHash", buf, sizeof(buf));
    p.end();
    webPasswordHash = String(buf);
    logPrintf(LOG_INFO, "Auth", "loadPasswordHash: '%s'", webPasswordHash.isEmpty() ? "(empty)" : "(set)");
}

// ── Store password hash in NVS ────────────────────────────────────────────
void savePasswordHash(const char* hash) {
    webPasswordHash = hash;
    Preferences p;
    p.begin("rmesh_auth", false);
    p.putString("webPwdHash", hash);
    p.end();
}

// ── Generate random nonce and store in session ────────────────────
void generateNonce(uint32_t clientId, char* buf) {
    uint8_t bytes[16];
    esp_fill_random(bytes, sizeof(bytes));
    for (int i = 0; i < 16; i++) sprintf(buf + 2 * i, "%02x", bytes[i]);
    buf[32] = '\0';

    for (int i = 0; i < MAX_AUTH_SESSIONS; i++) {
        if (authSessions[i].clientId == clientId) {
            memcpy(authSessions[i].nonce, buf, 33);
            break;
        }
    }
}

// ── Is a client authenticated? ──────────────────────────────────────────
bool isAuthenticated(uint32_t clientId) {
    if (webPasswordHash.isEmpty()) return true;  // no password set
    for (int i = 0; i < MAX_AUTH_SESSIONS; i++) {
        if (authSessions[i].clientId == clientId)
            return authSessions[i].authenticated;
    }
    return false;
}

// ── Create or update session ────────────────────────────────────────
void setClientAuth(uint32_t clientId, bool auth, uint32_t ipAddr) {
    // update existing session
    for (int i = 0; i < MAX_AUTH_SESSIONS; i++) {
        if (authSessions[i].clientId == clientId) {
            authSessions[i].authenticated = auth;
            if (ipAddr) authSessions[i].ipAddr = ipAddr;
            return;
        }
    }
    // occupy free slot
    for (int i = 0; i < MAX_AUTH_SESSIONS; i++) {
        if (authSessions[i].clientId == 0) {
            authSessions[i].clientId      = clientId;
            authSessions[i].authenticated = auth;
            authSessions[i].ipAddr        = ipAddr;
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
    authSessions[evict].ipAddr        = ipAddr;
    memset(authSessions[evict].nonce, 0, sizeof(authSessions[evict].nonce));
}

// ── Remove session (on disconnect) ───────────────────────────────────────
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

// ── Verify HMAC response ───────────────────────────────────────────────────────
// Expected: HMAC-SHA256(key=SHA256(password), data=nonce) as 64-char hex
bool verifyAuthResponse(uint32_t clientId, const char* response) {
    if (webPasswordHash.isEmpty()) return true;
    if (strlen(response) != 64)   return false;

    // Find nonce for this client
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

    // stored hash (hex) → bytes (= HMAC key)
    uint8_t keyBytes[32];
    for (int i = 0; i < 32; i++) {
        char hex[3] = {webPasswordHash[i * 2], webPasswordHash[i * 2 + 1], '\0'};
        keyBytes[i] = (uint8_t)strtoul(hex, nullptr, 16);
    }

    // compute HMAC-SHA256(key=storedHash, data=nonce)
    uint8_t hmacResult[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, keyBytes, 32);
    mbedtls_md_hmac_update(&ctx, (const uint8_t*)nonce, strlen(nonce));
    mbedtls_md_hmac_finish(&ctx, hmacResult);
    mbedtls_md_free(&ctx);

    // result → hex string
    char expected[65];
    for (int i = 0; i < 32; i++) sprintf(expected + 2 * i, "%02x", hmacResult[i]);
    expected[64] = '\0';

    return strncasecmp(response, expected, 64) == 0;
}

#else
// ── Stubs for non-WiFi builds ────────────────────────────────────────────────
String webPasswordHash = "";
#endif
