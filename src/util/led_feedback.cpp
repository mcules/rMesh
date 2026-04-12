#include "led_feedback.h"
#include <Arduino.h>
#include "hal/hal.h"

static uint8_t  fbCount     = 0;
static uint8_t  fbRemaining = 0;
static uint16_t fbOnMs      = 0;
static uint16_t fbOffMs     = 0;
static uint32_t fbDeadline  = 0;
static bool     fbLedOn     = false;

void ledFeedback(uint8_t count, uint16_t onMs, uint16_t offMs) {
    fbCount     = count;
    fbRemaining = count;
    fbOnMs      = onMs;
    fbOffMs     = offMs;
    fbLedOn     = true;
    fbDeadline  = millis() + onMs;
    setWiFiLED(true);
}

void ledFeedbackCancel() {
    fbRemaining = 0;
    fbLedOn = false;
    setWiFiLED(false);
}

bool ledFeedbackActive() {
    return fbRemaining > 0;
}

void ledFeedbackTick() {
    if (fbRemaining == 0) return;
    if ((int32_t)(millis() - fbDeadline) < 0) return;

    if (fbLedOn) {
        // LED was on → turn off
        setWiFiLED(false);
        fbLedOn = false;
        if (fbOffMs > 0) {
            fbDeadline = millis() + fbOffMs;
        } else {
            // no off phase → done with this blink
            if (fbCount != 255) fbRemaining--;
            if (fbRemaining > 0) {
                fbLedOn = true;
                setWiFiLED(true);
                fbDeadline = millis() + fbOnMs;
            }
        }
    } else {
        // LED was off → next blink or done
        if (fbCount != 255) fbRemaining--;
        if (fbRemaining > 0) {
            fbLedOn = true;
            setWiFiLED(true);
            fbDeadline = millis() + fbOnMs;
        }
    }
}
