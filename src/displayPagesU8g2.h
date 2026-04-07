#pragma once

#if defined(ESP32_E22_V1) || defined(HELTEC_WIFI_LORA_32_V3) || defined(LILYGO_T3_LORA32_V1_6_1)

#include <U8g2lib.h>
#include <stdint.h>

/**
 * @file displayPagesU8g2.h
 * @brief Page renderer shared by all U8g2-based 128x64 SSD1306 status displays.
 *
 * Holds the common page enum, availability filter and draw functions used by
 * the rotating multi-screen UI on the E22 Multimodul, Heltec WiFi LoRa 32 V3
 * and LilyGo T3 LoRa32 V1.6.1. The driver owns its own U8G2 instance and
 * passes a reference into these helpers — the renderer reads global state
 * (settings, peer/route lists, WiFi, etc.) directly.
 */

enum DisplayPage : uint8_t {
    PAGE_IDENTITY = 0,
    PAGE_NETWORK,
    PAGE_LORA_MESH,
    PAGE_MESSAGES,
    PAGE_SYSTEM,
    PAGE_COUNT
};

/// Short label drawn into the header bar (e.g. "ID", "NET", "LoRa").
const char* u8g2PageTitle(uint8_t page);

/// Decides whether the given page should currently take part in the
/// rotation. Honours `oledPageMask`, the channel filter and the message
/// buffer state.
bool u8g2PageAvailable(uint8_t page, const char* lastMsgSrc);

/// Renders @p page into u8g2's buffer. The caller is responsible for
/// `clearBuffer()` before and `sendBuffer()` after.
void u8g2DrawPage(U8G2& u8g2, uint8_t page,
                  const char* lastMsgSrc, const char* lastMsgText);

/// Draws the centred rMesh boot splash (logo + version + callsign).
void u8g2DrawSplash(U8G2& u8g2);

/// Draws the "Flashing <what>" full-screen update notification.
void u8g2DrawFlashing(U8G2& u8g2, const char* what);

#endif