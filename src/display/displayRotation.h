#pragma once

#include <stdint.h>

/**
 * @brief Tiny page rotator shared by all status-display drivers.
 *
 * Drivers register a fixed number of pages plus an optional availability
 * callback that decides whether a given page should currently take part in
 * the rotation (e.g. messages page only when a buffer is non-empty, or pages
 * masked out via the WebUI). The rotator owns the timing/hold state but no
 * rendering — drivers ask for the current page index in their refresh hook
 * and draw it themselves.
 */
class DisplayRotator {
public:
    using AvailabilityFn = bool (*)(uint8_t pageIdx);

    /// Initialise with a fixed number of pages and an optional availability
    /// filter. Resets the current page to 0 and clears any pending hold.
    void begin(uint8_t pageCount, AvailabilityFn isAvailable);

    /// Advance to the next available page if @p intervalMs has elapsed since
    /// the last switch and no hold is active. Pass intervalMs == 0 to disable
    /// auto-advance entirely (manual / button-driven only). Returns the
    /// current page index.
    uint8_t tick(uint32_t intervalMs);

    /// Manually advance to the next available page; clears any pending hold.
    void next();

    /// Jump to a specific page and hold it for @p holdMs before the next
    /// auto-advance is allowed.
    void forcePage(uint8_t idx, uint32_t holdMs);

    uint8_t current() const { return currentIdx; }

private:
    uint8_t pageCount = 1;
    uint8_t currentIdx = 0;
    AvailabilityFn isAvail = nullptr;
    uint32_t switchAt = 0;
    uint32_t holdUntil = 0;

    void advanceToAvailable();
};