#ifdef HAS_WIFI

#include "apiAuth.h"
#include "auth.h"
#include "settings.h"
#include "ethFunctions.h"
#include "logging.h"
#include <WiFi.h>
#include <mbedtls/md.h>
#include <time.h>
#ifdef HAS_ETHERNET
#include <ETH.h>
#endif

// Convert hex string to byte array
static void hexToBytes(const char* hex, uint8_t* bytes, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char h[3] = {hex[i * 2], hex[i * 2 + 1], '\0'};
        bytes[i] = (uint8_t)strtoul(h, nullptr, 16);
    }
}

// Constant-time comparison of two hex strings (prevents timing attacks)
static bool constantTimeCompare(const char* a, const char* b, size_t len) {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        // Normalize to lowercase for case-insensitive compare
        uint8_t ca = (a[i] >= 'A' && a[i] <= 'F') ? (a[i] | 0x20) : a[i];
        uint8_t cb = (b[i] >= 'A' && b[i] <= 'F') ? (b[i] | 0x20) : b[i];
        diff |= ca ^ cb;
    }
    return diff == 0;
}

static bool verifyHmac(const char* authHeader, const char* method, const char* path) {
    // Parse "HMAC timestamp:signature"
    if (strncmp(authHeader, "HMAC ", 5) != 0) return false;
    const char* payload = authHeader + 5;
    const char* colon = strchr(payload, ':');
    if (!colon) return false;

    // Extract timestamp (max 12 digits)
    size_t tsLen = colon - payload;
    if (tsLen == 0 || tsLen > 12) return false;
    char timestamp[16];
    memcpy(timestamp, payload, tsLen);
    timestamp[tsLen] = '\0';

    // Extract signature (must be exactly 64 hex chars)
    const char* clientSig = colon + 1;
    if (strlen(clientSig) != 64) return false;

    // Check timestamp window (+-60 seconds)
    long ts = atol(timestamp);
    long now = (long)time(nullptr);
    long delta = now - ts;
    if (delta < 0) delta = -delta;
    if (now > 1000000000L && delta > 60) return false;

    // Compute expected signature: HMAC-SHA256(webPasswordHash bytes, "timestamp:METHOD:path")
    // Build message into stack buffer: "timestamp:METHOD:path"
    char message[256];
    int msgLen = snprintf(message, sizeof(message), "%s:%s:%s", timestamp, method, path);
    if (msgLen < 0 || msgLen >= (int)sizeof(message)) return false;

    if (webPasswordHash.length() < 64) return false;
    uint8_t key[32];
    hexToBytes(webPasswordHash.c_str(), key, 32);

    uint8_t hmacResult[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, key, 32);
    mbedtls_md_hmac_update(&ctx, (const uint8_t*)message, msgLen);
    mbedtls_md_hmac_finish(&ctx, hmacResult);
    mbedtls_md_free(&ctx);

    // Convert to hex
    char expected[65];
    for (int i = 0; i < 32; i++) sprintf(expected + 2 * i, "%02x", hmacResult[i]);
    expected[64] = '\0';

    return constantTimeCompare(expected, clientSig, 64);
}

static bool verifyBearer(const char* authHeader) {
    if (strncmp(authHeader, "Bearer ", 7) != 0) return false;
    const char* token = authHeader + 7;
    size_t tokenLen = strlen(token);
    if (tokenLen != webPasswordHash.length()) return false;
    return constantTimeCompare(token, webPasswordHash.c_str(), tokenLen);
}

bool checkApiAuth(AsyncWebServerRequest *request) {
    // Per-interface WebUI filter (before auth, so blocked interfaces can't even try)
    if (!checkIfaceWebUI(request)) return false;

    // No password set -> open access
    if (webPasswordHash.isEmpty()) return true;

    // Allow requests from IPs that have an authenticated WebSocket session
    // (the WebUI fetches API data after authenticating via WebSocket)
    uint32_t reqIP = (uint32_t)request->client()->remoteIP();
    for (int i = 0; i < MAX_AUTH_SESSIONS; i++) {
        if (authSessions[i].clientId != 0 && authSessions[i].authenticated
            && authSessions[i].ipAddr == reqIP) {
            return true;
        }
    }

    if (!request->hasHeader("Authorization")) {
        request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
        return false;
    }

    String auth = request->header("Authorization");

    bool ok = false;
    if (strncmp(auth.c_str(), "HMAC ", 5) == 0) {
        ok = verifyHmac(auth.c_str(), request->methodToString(), request->url().c_str());
    } else if (strncmp(auth.c_str(), "Bearer ", 7) == 0) {
        ok = verifyBearer(auth.c_str());
    }

    if (!ok) {
        request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
    }
    return ok;
}

bool checkIfaceWebUI(AsyncWebServerRequest *request) {
#ifdef HAS_ETHERNET
    if (!ethConnected) return true;  // Single interface — always allow
    IPAddress local = request->client()->localIP();
    if (local == WiFi.localIP() || local == WiFi.softAPIP()) {
        if (!wifiWebUI) {
            request->send(403, "application/json", "{\"error\":\"WebUI disabled on WiFi\"}");
            return false;
        }
    } else if (local == ETH.localIP()) {
        if (!ethWebUI) {
            request->send(403, "application/json", "{\"error\":\"WebUI disabled on Ethernet\"}");
            return false;
        }
    }
#endif
    return true;
}

#endif
