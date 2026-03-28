#pragma once

/**
 * @file platform_nrf52.h
 * @brief Platform abstraction for nRF52840-based boards.
 */

#ifdef NRF52_PLATFORM

#include <Arduino.h>
#include <vector>
#include <algorithm>
#include <time.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

using namespace Adafruit_LittleFS_Namespace;

// ── LittleFS Compatibility Wrapper ───────────────────────────────────────────
// ESP32 LittleFS.open(path, "r") uses string modes; Adafruit uses uint8_t flags.

class NRF52_LittleFSCompat {
public:
    bool begin(bool formatOnFail = false) {
        (void)formatOnFail;
        return InternalFS.begin();
    }

    File open(const char* path, const char* mode = "r") {
        uint8_t flags = FILE_O_READ;
        if (mode && (mode[0] == 'w' || mode[0] == 'a')) flags = FILE_O_WRITE;
        return InternalFS.open(path, flags);
    }

    bool mkdir(const char* path) { return InternalFS.mkdir(path); }
    bool remove(const char* path) { return InternalFS.remove(path); }
    bool rmdir(const char* path) { return InternalFS.rmdir(path); }
    bool rmdir_r(const char* path) { return InternalFS.rmdir_r(path); }
    bool exists(const char* path) { return InternalFS.exists(path); }
    bool rename(const char* oldPath, const char* newPath) {
        // Adafruit LittleFS doesn't have rename; copy + delete
        File src(InternalFS);
        if (!src.open(oldPath, FILE_O_READ)) return false;
        File dst(InternalFS);
        if (!dst.open(newPath, FILE_O_WRITE)) { src.close(); return false; }
        uint8_t buf[128];
        while (src.available()) {
            int n = src.read(buf, sizeof(buf));
            if (n > 0) dst.write(buf, n);
        }
        src.close();
        dst.close();
        InternalFS.remove(oldPath);
        return true;
    }
};

extern NRF52_LittleFSCompat LittleFSNRF;
#define LittleFS LittleFSNRF

// ── settimeofday stub ────────────────────────────────────────────────────────
// nRF52 Arduino core doesn't provide settimeofday; provide a minimal version.

static time_t _nrf52_time_offset = 0;

inline int settimeofday(const struct timeval* tv, const void* tz) {
    (void)tz;
    if (tv) {
        _nrf52_time_offset = tv->tv_sec - (millis() / 1000);
    }
    return 0;
}

// ── System Functions ─────────────────────────────────────────────────────────

inline void platformRestart() {
    NVIC_SystemReset();
}

inline void platformDeepSleep() {
    NRF_POWER->SYSTEMOFF = 1;
}

inline uint32_t platformGetFreeHeap() {
    // FreeRTOS heap query
    return (uint32_t)xPortGetFreeHeapSize();
}

inline uint64_t platformGetChipId() {
    return ((uint64_t)NRF_FICR->DEVICEADDR[1] << 32) | NRF_FICR->DEVICEADDR[0];
}

// ── Preferences Class (NVS-compatible, backed by InternalFS) ─────────────────

class Preferences {
public:
    bool begin(const char* name, bool readOnly = false) {
        (void)readOnly;
        snprintf(_nsPath, sizeof(_nsPath), "/prefs/%s", name);
        InternalFS.mkdir("/prefs");
        InternalFS.mkdir(_nsPath);
        return true;
    }

    void end() { _nsPath[0] = '\0'; }

    size_t putBytes(const char* key, const void* value, size_t len) {
        char path[80];
        _makePath(path, sizeof(path), key);
        File f(InternalFS);
        if (f.open(path, FILE_O_WRITE)) {
            f.seek(0);
            size_t written = f.write((const uint8_t*)value, len);
            f.close();
            return written;
        }
        return 0;
    }

    size_t getBytes(const char* key, void* buf, size_t maxLen) {
        char path[80];
        _makePath(path, sizeof(path), key);
        File f(InternalFS);
        if (f.open(path, FILE_O_READ)) {
            size_t sz = f.size();
            if (sz > maxLen) sz = maxLen;
            size_t rd = f.read((uint8_t*)buf, sz);
            f.close();
            return rd;
        }
        return 0;
    }

    size_t getBytesLength(const char* key) {
        char path[80];
        _makePath(path, sizeof(path), key);
        File f(InternalFS);
        if (f.open(path, FILE_O_READ)) {
            size_t sz = f.size();
            f.close();
            return sz;
        }
        return 0;
    }

    size_t putString(const char* key, const String& value) {
        return putBytes(key, value.c_str(), value.length() + 1);
    }

    size_t putString(const char* key, const char* value) {
        return putBytes(key, value, strlen(value) + 1);
    }

    String getString(const char* key, const String& defaultValue = String()) {
        size_t len = getBytesLength(key);
        if (len == 0) return defaultValue;
        char* buf = new char[len + 1];
        if (!buf) return defaultValue;
        memset(buf, 0, len + 1);
        getBytes(key, buf, len);
        buf[len] = '\0';
        String result(buf);
        delete[] buf;
        return result;
    }

    size_t getString(const char* key, char* buf, size_t maxLen) {
        size_t len = getBytesLength(key);
        if (len == 0) { if (maxLen > 0) buf[0] = '\0'; return 0; }
        return getBytes(key, buf, maxLen);
    }

    size_t putUChar(const char* key, uint8_t value) { return putBytes(key, &value, 1); }
    uint8_t getUChar(const char* key, uint8_t def = 0) {
        uint8_t v = def; if (getBytesLength(key) == 1) getBytes(key, &v, 1); return v;
    }

    size_t putBool(const char* key, bool value) { uint8_t v = value ? 1 : 0; return putBytes(key, &v, 1); }
    bool getBool(const char* key, bool def = false) {
        uint8_t v = def ? 1 : 0; if (getBytesLength(key) >= 1) getBytes(key, &v, 1); return v != 0;
    }

    size_t putFloat(const char* key, float value) { return putBytes(key, &value, sizeof(float)); }
    float getFloat(const char* key, float def = 0.0f) {
        float v = def; if (getBytesLength(key) == sizeof(float)) getBytes(key, &v, sizeof(float)); return v;
    }

    bool remove(const char* key) {
        char path[80]; _makePath(path, sizeof(path), key);
        return InternalFS.remove(path);
    }

private:
    char _nsPath[48] = {0};
    void _makePath(char* out, size_t outLen, const char* key) {
        snprintf(out, outLen, "%s/%s", _nsPath, key);
    }
};

// ── NVS Flash Stubs ─────────────────────────────────────────────────────────

inline void nvs_flash_erase_nrf52() {
    InternalFS.rmdir_r("/prefs");
    InternalFS.mkdir("/prefs");
}

inline void nvs_flash_init_nrf52() {
    InternalFS.mkdir("/prefs");
}

#endif // NRF52_PLATFORM
