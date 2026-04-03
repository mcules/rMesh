#include "logging.h"

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

void logPrintf(LogLevel level, const char* tag, const char* fmt, ...) {
    if (!serialDebug && level < LOG_INFO) return;

    char msg[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    if (serialDebug) {
        JsonDocument doc;
        doc["level"] = levelToStr(level);
        doc["tag"]   = tag;
        doc["msg"]   = msg;
        Serial.print("DBG:");
        serializeJson(doc, Serial);
        Serial.print("\r\n");
    } else {
        Serial.printf("[%s] %s\r\n", tag, msg);
    }
}

void logJson(const JsonDocument& doc) {
    if (!serialDebug) return;
    Serial.print("DBG:");
    serializeJson(doc, Serial);
    Serial.print("\r\n");
}
