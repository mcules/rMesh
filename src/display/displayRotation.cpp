#include <Arduino.h>

#include "displayRotation.h"

void DisplayRotator::begin(uint8_t count, AvailabilityFn fn) {
    pageCount = count > 0 ? count : 1;
    isAvail = fn;
    currentIdx = 0;
    switchAt = 0;
    holdUntil = 0;
}

void DisplayRotator::advanceToAvailable() {
    for (uint8_t i = 0; i < pageCount; ++i) {
        currentIdx = (currentIdx + 1) % pageCount;
        if (!isAvail || isAvail(currentIdx)) return;
    }
}

uint8_t DisplayRotator::tick(uint32_t intervalMs) {
    uint32_t now = millis();
    if (now < holdUntil) {
        return currentIdx;
    }
    if (intervalMs > 0) {
        if (switchAt != 0 && now >= switchAt) {
            advanceToAvailable();
        }
        switchAt = now + intervalMs;
    }
    if (isAvail && !isAvail(currentIdx)) {
        advanceToAvailable();
    }
    return currentIdx;
}

void DisplayRotator::next() {
    advanceToAvailable();
    switchAt = 0;
    holdUntil = 0;
}

void DisplayRotator::forcePage(uint8_t idx, uint32_t holdMs) {
    if (idx < pageCount) currentIdx = idx;
    holdUntil = millis() + holdMs;
    switchAt = holdUntil;
}
