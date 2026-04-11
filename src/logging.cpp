#include "logging.h"
#include <time.h>

extern bool serialDebug;

static const char* levelToStr(LogLevel level) {
    switch (level) {
        case LOG_DEBUG: return "debug";
        case LOG_INFO:  return "info";
        case LOG_WARN:  return "warn";
        case LOG_ERROR: return "error";
        default:        return "info";
    }
}

// Format a timestamp prefix. Always produces the same YYYY-MM-DD HH:MM:SS
// shape so every log line is visually consistent. Before NTP sync we fall
// back to the 1970 epoch + uptime, which is immediately recognisable as
// "pre-sync" without breaking column alignment.
static void formatTimestamp(char* out, size_t outLen) {
    time_t now = time(nullptr);
    if (now <= 1577836800) { // < 2020-01-01 → NTP not synced yet
        now = (time_t)(millis() / 1000);
    }
    struct tm tmv;
    localtime_r(&now, &tmv);
    snprintf(out, outLen, "%04d-%02d-%02d %02d:%02d:%02d",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
}

void logPrintf(LogLevel level, const char* tag, const char* fmt, ...) {
    if (!serialDebug && level < LOG_INFO) return;

    char msg[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    char ts[24];
    formatTimestamp(ts, sizeof(ts));

    if (serialDebug) {
        JsonDocument doc;
        doc["ts"]    = ts;
        doc["level"] = levelToStr(level);
        doc["tag"]   = tag;
        doc["msg"]   = msg;
        Serial.print("DBG:");
        serializeJson(doc, Serial);
        Serial.print("\r\n");
    } else {
        Serial.printf("[%s] [%s] %s\r\n", ts, tag, msg);
    }
}

void logRaw(const char* fmt, ...) {
    if (serialDebug) return;  // raw output is for humans, not structured debug
    char msg[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    Serial.printf("%s\r\n", msg);
}

void logJson(const JsonDocument& doc) {
    if (!serialDebug) return;
    Serial.print("DBG:");
    serializeJson(doc, Serial);
    Serial.print("\r\n");
}
