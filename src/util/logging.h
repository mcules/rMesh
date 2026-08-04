#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <stdarg.h>

enum LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
};

/**
 * @brief Printf-style logging with level and tag.
 *
 * - serialDebug=false: LOG_INFO+ printed as "[tag] message\r\n", LOG_DEBUG suppressed.
 * - serialDebug=true:  all levels as DBG:{"level":"…","tag":"…","msg":"…"}\r\n
 *
 * Callers must NOT include a trailing \n — the function appends \r\n automatically.
 */
void logPrintf(LogLevel level, const char* tag, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));

/**
 * @brief Printf-style output without timestamp or tag prefix.
 *
 * Used for human-readable output like help text and settings display.
 * In serialDebug mode the output is suppressed (use logPrintf for structured output).
 */
void logRaw(const char* fmt, ...)
    __attribute__((format(printf, 1, 2)));

/**
 * @brief Emit a structured JSON debug object.
 *
 * Only produces output when serialDebug is true.
 * Format: DBG:{…}\r\n
 */
void logJson(const JsonDocument& doc);
