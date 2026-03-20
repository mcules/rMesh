#include <Arduino.h>
#include "dutycycle.h"

// Rollendes 60-s-Fenster, max. 10 % Sendezeit = 6000 ms
static const uint32_t DC_WINDOW_MS = 60000;
static const uint32_t DC_MAX_MS    = 6000;

static uint32_t dcWindowStart = 0;
static uint32_t dcAirtimeMs   = 0;

static void resetIfExpired() {
    if (millis() - dcWindowStart >= DC_WINDOW_MS) {
        dcWindowStart = millis();
        dcAirtimeMs   = 0;
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
