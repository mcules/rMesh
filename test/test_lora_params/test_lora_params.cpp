// Native unit tests for LoRa parameter clamping (src/util/lora_math.cpp).
#include <unity.h>
#include "util/lora_math.cpp"

void setUp(void)    {}
void tearDown(void) {}

void test_sf_clamped(void) {
    uint8_t sf; uint8_t cr = 5; float bw = 125.0f; int16_t pre = 8;
    sf = 0;  clampLoraParams(sf, cr, bw, pre); TEST_ASSERT_EQUAL_UINT8(6, sf);
    sf = 5;  clampLoraParams(sf, cr, bw, pre); TEST_ASSERT_EQUAL_UINT8(6, sf);
    sf = 99; clampLoraParams(sf, cr, bw, pre); TEST_ASSERT_EQUAL_UINT8(12, sf);
    sf = 9;  clampLoraParams(sf, cr, bw, pre); TEST_ASSERT_EQUAL_UINT8(9, sf);  // valid untouched
}

void test_cr_clamped(void) {
    uint8_t sf = 9; uint8_t cr; float bw = 125.0f; int16_t pre = 8;
    cr = 0;  clampLoraParams(sf, cr, bw, pre); TEST_ASSERT_EQUAL_UINT8(5, cr);
    cr = 99; clampLoraParams(sf, cr, bw, pre); TEST_ASSERT_EQUAL_UINT8(8, cr);
    cr = 7;  clampLoraParams(sf, cr, bw, pre); TEST_ASSERT_EQUAL_UINT8(7, cr);
}

void test_preamble_min(void) {
    uint8_t sf = 9; uint8_t cr = 5; float bw = 125.0f; int16_t pre;
    pre = 0;   clampLoraParams(sf, cr, bw, pre); TEST_ASSERT_EQUAL_INT16(6, pre);
    pre = 100; clampLoraParams(sf, cr, bw, pre); TEST_ASSERT_EQUAL_INT16(100, pre); // valid untouched
}

void test_bw_snapped_to_valid(void) {
    uint8_t sf = 9; uint8_t cr = 5; int16_t pre = 8; float bw;
    bw = 123.0f; clampLoraParams(sf, cr, bw, pre); TEST_ASSERT_FLOAT_WITHIN(0.01f, 125.0f, bw); // invalid -> default
    bw = 0.0f;   clampLoraParams(sf, cr, bw, pre); TEST_ASSERT_FLOAT_WITHIN(0.01f, 125.0f, bw);
    bw = 62.5f;  clampLoraParams(sf, cr, bw, pre); TEST_ASSERT_FLOAT_WITHIN(0.01f, 62.5f, bw);  // valid untouched
    bw = 500.0f; clampLoraParams(sf, cr, bw, pre); TEST_ASSERT_FLOAT_WITHIN(0.01f, 500.0f, bw);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_sf_clamped);
    RUN_TEST(test_cr_clamped);
    RUN_TEST(test_preamble_min);
    RUN_TEST(test_bw_snapped_to_valid);
    return UNITY_END();
}
