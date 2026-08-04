// Native unit tests for the EU868 duty-cycle accounting (src/hal/dutycycle.cpp).
// Runs on the host via `pio test -e native` — no hardware required.
#include <unity.h>
#include "Arduino.h"          // mock: controllable millis()

unsigned long _mock_millis = 0;

// Compile the real implementation into this test program.
#include "hal/dutycycle.cpp"

// Reset the (file-static) window by letting a full window elapse.
static void reset_dc() {
    _mock_millis += 120000;   // > DC_WINDOW_MS (60 s)
    dutyCycleAllowed(0);      // triggers resetIfExpired() -> bucket cleared
}

void setUp(void)    { reset_dc(); }
void tearDown(void) {}

// After a reset the full 6000 ms (10% of 60 s) budget is available.
void test_fresh_budget(void) {
    TEST_ASSERT_TRUE(dutyCycleAllowed(6000));
    TEST_ASSERT_FALSE(dutyCycleAllowed(6001));
}

// Tracked airtime consumes the budget.
void test_track_consumes_budget(void) {
    dutyCycleTrackTx(6000);
    TEST_ASSERT_FALSE(dutyCycleAllowed(1));   // 6000 + 1 > 6000
    TEST_ASSERT_TRUE(dutyCycleAllowed(0));    // exactly full still allows a 0 ms TX
}

// The bucket decays proportionally to elapsed time within the window.
void test_decay_over_half_window(void) {
    dutyCycleTrackTx(6000);
    _mock_millis += 30000;                    // half of the 60 s window
    // decay = 6000 * 30000 / 60000 = 3000 -> 3000 remaining
    TEST_ASSERT_TRUE(dutyCycleAllowed(3000));
    TEST_ASSERT_FALSE(dutyCycleAllowed(3001));
}

// Regression test for the drain bug: with frequent polling and tiny time steps
// the per-call decay truncates to 0. The OLD code advanced the window start on
// every call regardless, so the elapsed time was discarded and the bucket froze
// near-full (~5999) -> all LoRa TX stayed blocked. The FIX only advances the
// window when it actually drains, so elapsed accumulates and the bucket decays.
// (Decay is proportional/exponential, so after one window it sits well below full
// but not at zero — we assert it drained *meaningfully*, which the bug never does.)
void test_frequent_polling_still_drains(void) {
    dutyCycleTrackTx(6000);                   // fill the bucket
    for (int i = 0; i < 6000; i++) {
        _mock_millis += 10;                   // 6000 * 10 ms = 60000 ms = one window
        dutyCycleAllowed(0);                  // frequent polling with tiny steps
    }
    // Fixed: bucket decayed to ~1/3 -> a 1000 ms TX fits. Buggy: stuck ~5999 -> false.
    TEST_ASSERT_TRUE(dutyCycleAllowed(1000));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fresh_budget);
    RUN_TEST(test_track_consumes_budget);
    RUN_TEST(test_decay_over_half_window);
    RUN_TEST(test_frequent_polling_still_drains);
    return UNITY_END();
}
