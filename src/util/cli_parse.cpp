// Pure CLI argument parsing (no Arduino deps) so it can run in the native
// unit-test env (test/test_cli_parse).
#include "util/cli_parse.h"
#include <string.h>

// Bounded copy of [src, src+len) with guaranteed NUL termination.
static void copyToken(char* dst, size_t cap, const char* src, size_t len) {
    if (cap == 0) return;
    if (len >= cap) len = cap - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

// Read one token starting at *p: either "quoted" (may contain spaces) or
// bare (ends at the next space). Advances *p past the token.
static void readToken(const char** p, char* dst, size_t cap) {
    const char* s = *p;
    if (*s == '"') {
        s++;
        const char* end = strchr(s, '"');
        if (end == nullptr) {           // unterminated quote: rest of line
            copyToken(dst, cap, s, strlen(s));
            *p = s + strlen(s);
            return;
        }
        copyToken(dst, cap, s, (size_t)(end - s));
        *p = end + 1;
    } else {
        const char* end = strchr(s, ' ');
        if (end == nullptr) end = s + strlen(s);
        copyToken(dst, cap, s, (size_t)(end - s));
        *p = end;
    }
}

bool parseWifiAddArgs(const char* args, char* ssid, size_t ssidCap,
                      char* password, size_t pwCap) {
    if (ssidCap > 0) ssid[0] = '\0';
    if (pwCap > 0) password[0] = '\0';
    if (args == nullptr) return false;

    const char* p = args;
    while (*p == ' ') p++;
    if (*p == '\0') return false;

    readToken(&p, ssid, ssidCap);
    while (*p == ' ') p++;

    if (*p != '\0') {
        if (*p == '"') {
            readToken(&p, password, pwCap);
        } else {
            // Unquoted password: take the rest of the line verbatim
            // (previous behaviour; passwords may contain spaces).
            copyToken(password, pwCap, p, strlen(p));
        }
    }
    return ssid[0] != '\0';
}
