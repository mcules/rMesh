#include <Arduino.h>
#include "hal/dutycycle.h"

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
        dcWindowStart = now;
    } else {
        uint32_t decay = (uint32_t)((uint64_t)dcAirtimeMs * elapsed / DC_WINDOW_MS);
        // Only advance the window when we actually drained ≥1 ms. This function is
        // called on every ready TX (sub-ms apart under load); with integer division
        // `decay` truncates to 0 for small `elapsed`, so unconditionally resetting
        // dcWindowStart would discard that elapsed time and the bucket would never
        // drain — postponing all LoRa TX indefinitely exactly when the mesh is busy.
        if (decay > 0) {
            dcAirtimeMs -= decay;
            dcWindowStart = now;
        }
    }
}

bool dutyCycleAllowed(uint32_t toaMs) {
    resetIfExpired();
    return (dcAirtimeMs + toaMs) <= DC_MAX_MS;
}

void dutyCycleTrackTx(uint32_t toaMs) {
    resetIfExpired();
    dcAirtimeMs += toaMs;
}
