// Native unit tests for CLI argument parsing (src/util/cli_parse.cpp).
#include <unity.h>
#include "util/cli_parse.cpp"

void setUp(void)    {}
void tearDown(void) {}

static char ssid[64];
static char pw[64];

void test_plain_ssid_and_pw(void) {
    TEST_ASSERT_TRUE(parseWifiAddArgs("MyNet secret", ssid, sizeof(ssid), pw, sizeof(pw)));
    TEST_ASSERT_EQUAL_STRING("MyNet", ssid);
    TEST_ASSERT_EQUAL_STRING("secret", pw);
}

void test_plain_ssid_without_pw(void) {
    TEST_ASSERT_TRUE(parseWifiAddArgs("OpenNet", ssid, sizeof(ssid), pw, sizeof(pw)));
    TEST_ASSERT_EQUAL_STRING("OpenNet", ssid);
    TEST_ASSERT_EQUAL_STRING("", pw);
}

void test_quoted_ssid_with_spaces(void) {
    TEST_ASSERT_TRUE(parseWifiAddArgs("\"My Home Net\" secret", ssid, sizeof(ssid), pw, sizeof(pw)));
    TEST_ASSERT_EQUAL_STRING("My Home Net", ssid);
    TEST_ASSERT_EQUAL_STRING("secret", pw);
}

void test_quoted_ssid_without_pw(void) {
    TEST_ASSERT_TRUE(parseWifiAddArgs("\"My Home Net\"", ssid, sizeof(ssid), pw, sizeof(pw)));
    TEST_ASSERT_EQUAL_STRING("My Home Net", ssid);
    TEST_ASSERT_EQUAL_STRING("", pw);
}

void test_quoted_ssid_and_quoted_pw(void) {
    TEST_ASSERT_TRUE(parseWifiAddArgs("\"My Net\" \"pass with space\"", ssid, sizeof(ssid), pw, sizeof(pw)));
    TEST_ASSERT_EQUAL_STRING("My Net", ssid);
    TEST_ASSERT_EQUAL_STRING("pass with space", pw);
}

void test_plain_ssid_pw_with_spaces(void) {
    // Previous behaviour: unquoted password takes the rest of the line
    TEST_ASSERT_TRUE(parseWifiAddArgs("MyNet pass with space", ssid, sizeof(ssid), pw, sizeof(pw)));
    TEST_ASSERT_EQUAL_STRING("MyNet", ssid);
    TEST_ASSERT_EQUAL_STRING("pass with space", pw);
}

void test_unterminated_quote_takes_rest(void) {
    TEST_ASSERT_TRUE(parseWifiAddArgs("\"My Home Net", ssid, sizeof(ssid), pw, sizeof(pw)));
    TEST_ASSERT_EQUAL_STRING("My Home Net", ssid);
    TEST_ASSERT_EQUAL_STRING("", pw);
}

void test_leading_and_extra_spaces(void) {
    TEST_ASSERT_TRUE(parseWifiAddArgs("  \"My Net\"   secret", ssid, sizeof(ssid), pw, sizeof(pw)));
    TEST_ASSERT_EQUAL_STRING("My Net", ssid);
    TEST_ASSERT_EQUAL_STRING("secret", pw);
}

void test_empty_input_fails(void) {
    TEST_ASSERT_FALSE(parseWifiAddArgs("", ssid, sizeof(ssid), pw, sizeof(pw)));
    TEST_ASSERT_FALSE(parseWifiAddArgs("   ", ssid, sizeof(ssid), pw, sizeof(pw)));
    TEST_ASSERT_FALSE(parseWifiAddArgs(nullptr, ssid, sizeof(ssid), pw, sizeof(pw)));
}

void test_empty_quotes_fail(void) {
    TEST_ASSERT_FALSE(parseWifiAddArgs("\"\" secret", ssid, sizeof(ssid), pw, sizeof(pw)));
}

void test_overlong_values_truncated(void) {
    char tiny[8];
    TEST_ASSERT_TRUE(parseWifiAddArgs("\"A very long network name\" longpassword", tiny, sizeof(tiny), pw, sizeof(pw)));
    TEST_ASSERT_EQUAL_STRING("A very ", tiny);
    TEST_ASSERT_EQUAL_STRING("longpassword", pw);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_plain_ssid_and_pw);
    RUN_TEST(test_plain_ssid_without_pw);
    RUN_TEST(test_quoted_ssid_with_spaces);
    RUN_TEST(test_quoted_ssid_without_pw);
    RUN_TEST(test_quoted_ssid_and_quoted_pw);
    RUN_TEST(test_plain_ssid_pw_with_spaces);
    RUN_TEST(test_unterminated_quote_takes_rest);
    RUN_TEST(test_leading_and_extra_spaces);
    RUN_TEST(test_empty_input_fails);
    RUN_TEST(test_empty_quotes_fail);
    RUN_TEST(test_overlong_values_truncated);
    return UNITY_END();
}
