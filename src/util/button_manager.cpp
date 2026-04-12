#include "button_manager.h"
#include <Arduino.h>
#include "main.h"

// ── Timing thresholds ───────────────────────────────────────────────────────
static constexpr uint16_t DEBOUNCE_MS       = 40;
static constexpr uint16_t DOUBLE_WINDOW_MS  = 300;
static constexpr uint16_t LONG_PRESS_MS     = 3000;
static constexpr uint16_t VERY_LONG_PRESS_MS = 8000;

// ── State machine ───────────────────────────────────────────────────────────
enum class State : uint8_t {
    IDLE,               // waiting for press
    PRESSED,            // button held down
    SINGLE_PENDING,     // released once, waiting for potential second click
};

static State    state        = State::IDLE;
static uint32_t pressedAt    = 0;
static uint32_t releasedAt   = 0;
static bool     longFired    = false;
static bool     veryLongFired = false;
static int      buttonPin    = -1;

static bool buttonIsDown() {
    return digitalRead(buttonPin) == LOW;  // active-low (pull-up)
}

void buttonManagerInit() {
    if (!board || !board->hasUserButton()) return;
    buttonPin = board->pinUserButton();
    pinMode(buttonPin, INPUT_PULLUP);
}

ButtonEvent buttonManagerTick() {
    if (buttonPin < 0) return ButtonEvent::NONE;

    bool down = buttonIsDown();
    uint32_t now = millis();

    switch (state) {

        case State::IDLE:
            if (down) {
                state = State::PRESSED;
                pressedAt = now;
                longFired = false;
                veryLongFired = false;
            }
            break;

        case State::PRESSED:
            if (!down) {
                // released — was it a short press?
                if ((now - pressedAt) < LONG_PRESS_MS) {
                    state = State::SINGLE_PENDING;
                    releasedAt = now;
                } else {
                    state = State::IDLE;
                }
            } else {
                // still held
                uint32_t held = now - pressedAt;
                if (held >= VERY_LONG_PRESS_MS && !veryLongFired) {
                    veryLongFired = true;
                    return ButtonEvent::VERY_LONG_PRESS;
                }
                if (held >= LONG_PRESS_MS && !longFired) {
                    longFired = true;
                    return ButtonEvent::LONG_PRESS;
                }
            }
            break;

        case State::SINGLE_PENDING:
            if (down && (now - releasedAt) < DOUBLE_WINDOW_MS) {
                // second press within window → double click
                state = State::IDLE;
                // wait for release (simple debounce)
                return ButtonEvent::DOUBLE_CLICK;
            }
            if ((now - releasedAt) >= DOUBLE_WINDOW_MS) {
                // timeout — no second click → single click
                state = State::IDLE;
                return ButtonEvent::SINGLE_CLICK;
            }
            break;
    }

    return ButtonEvent::NONE;
}
