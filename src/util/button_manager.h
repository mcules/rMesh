#pragma once

#include <stdint.h>

/**
 * @file button_manager.h
 * @brief Gesture detection for the user button (single/double/long/very-long press).
 *
 * Call buttonManagerInit() once in setup() and buttonManagerTick() every loop
 * iteration.  The tick returns the detected gesture (or NONE).  The caller
 * dispatches the action (display toggle, BT mode cycle, AP switch, factory reset).
 */

enum class ButtonEvent : uint8_t {
    NONE,
    SINGLE_CLICK,      ///< Short press (<300 ms), no second click within 300 ms
    DOUBLE_CLICK,       ///< Two short presses within 300 ms
    LONG_PRESS,         ///< Held >= 3 s (fires once on threshold, not on release)
    VERY_LONG_PRESS     ///< Held >= 8 s (fires once on threshold)
};

/// Initialise the button manager.  Reads pin from board->pinUserButton().
void buttonManagerInit();

/// Poll the button state.  Returns the detected event (NONE most of the time).
ButtonEvent buttonManagerTick();
