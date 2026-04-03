#include <Arduino.h>
#include "dutycycle.h"

// Rolling 60 s window, max 10% airtime = 6000 ms
static const uint32_t DC_WINDOW_MS = 60000;
static const uint32_t DC_MAX_MS    = 6000;

static uint32_t dcWindowStart = 0;
static uint32_t dcAirtimeMs   = 0;

static void resetIfExpired() {
    uint32_t now = millis();
    uint32_t elapsed = now - dcWindowStart;
    if (elapsed >= DC_WINDOW_MS) {
        dcAirtimeMs = 0;
    } else {
        uint32_t decay = (uint32_t)((uint64_t)dcAirtimeMs * elapsed / DC_WINDOW_MS);
        dcAirtimeMs = (dcAirtimeMs > decay) ? dcAirtimeMs - decay : 0;
    }
    dcWindowStart = now;
}

bool dutyCycleAllowed(uint32_t toaMs) {
    resetIfExpired();
    return (dcAirtimeMs + toaMs) <= DC_MAX_MS;
}

void dutyCycleTrackTx(uint32_t toaMs) {
    resetIfExpired();
    dcAirtimeMs += toaMs;
}
