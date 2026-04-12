#pragma once

#include <stdint.h>

/**
 * @file led_feedback.h
 * @brief Non-blocking LED blink pattern system.
 *
 * Queues a blink pattern (count, on/off duration) that plays out over
 * successive ledFeedbackTick() calls without blocking the main loop.
 * Uses the board's WiFi LED via setWiFiLED().
 */

/// Start a blink pattern.  count=255 for continuous blink until next call.
void ledFeedback(uint8_t count, uint16_t onMs, uint16_t offMs);

/// Cancel any running pattern and turn LED off.
void ledFeedbackCancel();

/// Returns true while a pattern is playing.
bool ledFeedbackActive();

/// Call from loop() to advance the pattern.
void ledFeedbackTick();
