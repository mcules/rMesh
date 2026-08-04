// Native unit tests for LoRa frame (de)serialization (src/mesh/frame.cpp).
// Exercises the attacker-controlled parse path and the export bounds/truncation
// fixes. Runs on the host via `pio test -e native` — no hardware required.
#include <unity.h>
#include "Arduino.h"          // mock

unsigned long _mock_millis = 1234;   // used by Frame's default id = millis()

// Faithful-enough stand-in for the real safeUtf8Copy: clamp to the destination
// and null-terminate. (The real one additionally avoids splitting a UTF-8
// sequence; the length-clamp + termination behaviour under test is preserved.)
void safeUtf8Copy(char* dest, const uint8_t* src, size_t srcLen, size_t dstSize) {
    if (dstSize == 0) return;
    size_t n = (srcLen < dstSize - 1) ? srcLen : (dstSize - 1);
    memcpy(dest, src, n);
    dest[n] = '\0';
}

// Compile the real implementation into this test program.
#include "mesh/frame.cpp"

void setUp(void)    {}
void tearDown(void) {}

// A MESSAGE frame survives an export -> import round trip with all fields intact.
void test_roundtrip_message(void) {
    Frame f;
    f.frameType   = Frame::MESSAGE_FRAME;
    f.messageType = Frame::TEXT_MESSAGE;
    strcpy(f.srcCall, "AB1CDE");
    strcpy(f.dstCall, "XY2Z");
    const char* txt = "hello mesh";
    memcpy(f.message, txt, strlen(txt));
    f.messageLength = strlen(txt);
    f.id       = 0x11223344u;
    f.hopCount = 2;

    uint8_t buf[255];
    size_t n = f.exportBinary(buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, n);

    Frame g;
    g.importBinary(buf, n);
    TEST_ASSERT_EQUAL_STRING("AB1CDE", g.srcCall);
    TEST_ASSERT_EQUAL_STRING("XY2Z", g.dstCall);
    TEST_ASSERT_EQUAL_UINT8(Frame::MESSAGE_FRAME, g.frameType);
    TEST_ASSERT_EQUAL_UINT32(0x11223344u, g.id);
    TEST_ASSERT_EQUAL_UINT8(2, g.hopCount);
    TEST_ASSERT_EQUAL_size_t(strlen(txt), g.messageLength);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(txt, g.message, strlen(txt));
}

// A malicious/oversized callsign length field is clamped to MAX_CALLSIGN_LENGTH.
void test_oversized_callsign_clamped(void) {
    uint8_t buf[32];
    buf[0] = (uint8_t)(Frame::MESSAGE_FRAME & 0x0F);          // frametype, hop 0
    buf[1] = (uint8_t)((Frame::SRC_CALL_HEADER << 4) | 0x0F); // src header, len 15
    for (int i = 0; i < 15; i++) buf[2 + i] = 'A';
    Frame g;
    g.importBinary(buf, 2 + 15);
    TEST_ASSERT_LESS_OR_EQUAL(MAX_CALLSIGN_LENGTH, (int)strlen(g.srcCall));
}

// A too-short buffer is handled without crashing or writing fields.
void test_short_buffer(void) {
    uint8_t b[1] = {0x03};
    Frame g;
    g.importBinary(b, 1);        // length <= 1 -> early return
    TEST_ASSERT_EQUAL_size_t(0, strlen(g.srcCall));
}

// exportBinary never writes past the supplied buffer length.
void test_export_tiny_buffer_no_overflow(void) {
    Frame f;
    f.frameType = Frame::MESSAGE_FRAME;
    strcpy(f.srcCall, "ABCDEFGHI");
    f.messageLength = 100;
    memset(f.message, 'X', 100);
    uint8_t buf[5];
    size_t n = f.exportBinary(buf, sizeof(buf));
    TEST_ASSERT_LESS_OR_EQUAL(sizeof(buf), n);
}

// Regression: exporting into a buffer too small for the payload must NOT mutate
// the Frame's messageLength member (the truncation clamp uses a local variable),
// otherwise later retransmits from the TX buffer would carry a shortened length.
void test_export_preserves_messageLength(void) {
    Frame f;
    f.frameType = Frame::MESSAGE_FRAME;
    strcpy(f.srcCall, "ABC");
    f.messageLength = 260;
    memset(f.message, 'Y', 260);
    uint8_t buf[64];             // far too small for the full payload
    f.exportBinary(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(260, f.messageLength);
}

// An ANNOUNCE frame (no payload, carries nodeCall) survives a round trip.
void test_roundtrip_announce(void) {
    Frame f;
    f.frameType = Frame::ANNOUNCE_FRAME;
    strcpy(f.nodeCall, "NODE1");
    f.hopCount = 1;
    uint8_t buf[64];
    size_t n = f.exportBinary(buf, sizeof(buf));
    Frame g;
    g.importBinary(buf, n);
    TEST_ASSERT_EQUAL_UINT8(Frame::ANNOUNCE_FRAME, g.frameType);
    TEST_ASSERT_EQUAL_STRING("NODE1", g.nodeCall);
    TEST_ASSERT_EQUAL_UINT8(1, g.hopCount);
}

// hopCount is a 4-bit wire field: values >15 are masked, never overflow.
void test_hopcount_wire_masking(void) {
    Frame f;
    f.frameType = Frame::MESSAGE_FRAME;
    strcpy(f.srcCall, "AB1CD");
    const char* t = "x";
    memcpy(f.message, t, 1); f.messageLength = 1;
    f.hopCount = 200;
    uint8_t buf[64];
    size_t n = f.exportBinary(buf, sizeof(buf));
    Frame g;
    g.importBinary(buf, n);
    TEST_ASSERT_LESS_OR_EQUAL(15, g.hopCount);
    TEST_ASSERT_EQUAL_UINT8(200 & 0x0F, g.hopCount);
}

// Fuzz: thousands of random buffers must never overflow a field or crash
// (with -fsanitize=address any OOB access aborts the test).
void test_fuzz_import_invariants(void) {
    unsigned seed = 0x1234567u;
    for (int iter = 0; iter < 20000; iter++) {
        seed = seed * 1103515245u + 12345u;
        size_t len = (seed >> 8) % 301;          // 0..300 bytes
        uint8_t buf[301];
        for (size_t i = 0; i < len; i++) {
            seed = seed * 1103515245u + 12345u;
            buf[i] = (uint8_t)(seed >> 16);
        }
        Frame g;
        g.importBinary(buf, len);
        TEST_ASSERT_TRUE(g.messageLength <= sizeof(g.message));
        TEST_ASSERT_TRUE(strlen(g.srcCall)  <= (size_t)MAX_CALLSIGN_LENGTH);
        TEST_ASSERT_TRUE(strlen(g.nodeCall) <= (size_t)MAX_CALLSIGN_LENGTH);
        TEST_ASSERT_TRUE(strlen(g.viaCall)  <= (size_t)MAX_CALLSIGN_LENGTH);
        TEST_ASSERT_TRUE(strlen(g.dstCall)  <= (size_t)MAX_CALLSIGN_LENGTH);
        TEST_ASSERT_TRUE(strlen(g.dstGroup) <= (size_t)MAX_CALLSIGN_LENGTH);
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_roundtrip_message);
    RUN_TEST(test_roundtrip_announce);
    RUN_TEST(test_oversized_callsign_clamped);
    RUN_TEST(test_hopcount_wire_masking);
    RUN_TEST(test_short_buffer);
    RUN_TEST(test_export_tiny_buffer_no_overflow);
    RUN_TEST(test_export_preserves_messageLength);
    RUN_TEST(test_fuzz_import_invariants);
    return UNITY_END();
}
