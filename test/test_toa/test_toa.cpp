// Native unit tests for the LoRa Time-on-Air math (src/util/lora_math.cpp).
#include <unity.h>
#include "util/lora_math.cpp"

void setUp(void)    {}
void tearDown(void) {}

// Out-of-range parameters must return 0 (never feed garbage into scheduling / DC).
void test_out_of_range_returns_zero(void) {
    TEST_ASSERT_EQUAL_UINT32(0, computeToA(20, 0,  125.0f, 5, 8));  // SF too low
    TEST_ASSERT_EQUAL_UINT32(0, computeToA(20, 5,  125.0f, 5, 8));  // SF 5 (below 6)
    TEST_ASSERT_EQUAL_UINT32(0, computeToA(20, 13, 125.0f, 5, 8));  // SF too high
    TEST_ASSERT_EQUAL_UINT32(0, computeToA(20, 99, 125.0f, 5, 8));  // SF absurd
    TEST_ASSERT_EQUAL_UINT32(0, computeToA(20, 9,  0.0f,   5, 8));  // BW 0
}

// Larger payload -> longer airtime.
void test_monotonic_in_payload(void) {
    uint32_t a = computeToA(10,  9, 125.0f, 5, 8);
    uint32_t b = computeToA(200, 9, 125.0f, 5, 8);
    TEST_ASSERT_GREATER_THAN(0, a);
    TEST_ASSERT_GREATER_THAN(a, b);
}

// Higher spreading factor -> longer airtime.
void test_higher_sf_longer(void) {
    uint32_t sf7  = computeToA(50, 7,  125.0f, 5, 8);
    uint32_t sf12 = computeToA(50, 12, 125.0f, 5, 8);
    TEST_ASSERT_GREATER_THAN(sf7, sf12);
}

// Wider bandwidth -> shorter airtime.
void test_wider_bw_shorter(void) {
    uint32_t bw125 = computeToA(50, 9, 125.0f, 5, 8);
    uint32_t bw500 = computeToA(50, 9, 500.0f, 5, 8);
    TEST_ASSERT_GREATER_THAN(bw500, bw125);
}

// Hand-computed reference: 20 B @ SF7/BW125/CR4-5/preamble 8 ~= 57 ms.
void test_reference_ballpark(void) {
    uint32_t t = computeToA(20, 7, 125.0f, 5, 8);
    TEST_ASSERT_GREATER_THAN(40, t);
    TEST_ASSERT_LESS_THAN(80, t);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_out_of_range_returns_zero);
    RUN_TEST(test_monotonic_in_payload);
    RUN_TEST(test_higher_sf_longer);
    RUN_TEST(test_wider_bw_shorter);
    RUN_TEST(test_reference_ballpark);
    return UNITY_END();
}
