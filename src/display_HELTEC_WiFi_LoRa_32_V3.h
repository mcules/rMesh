#pragma once

#ifdef HELTEC_WIFI_LORA_32_V3

#include "displayPagesU8g2.h"
#include "statusDisplay.h"

/**
 * @brief SSD1306 OLED status display for the Heltec WiFi LoRa 32 V3 (128x64).
 *
 * Rotating multi-screen UI shared with the ESP32 E22 Multimodul. Page set,
 * timing and splash/flashing rendering live in displayPagesU8g2 /
 * displayRotation.
 */

/// Probe I2C for SSD1306 and initialise U8g2 if found.
bool initStatusDisplay();

/// Redraw the status screen.
void updateStatusDisplay();

/// Turn display on, draw content, set oledEnabled = true and persist.
void enableStatusDisplay();

/// Turn display off (power save), set oledEnabled = false and persist.
void disableStatusDisplay();

/// Returns true if a display was detected during init.
bool hasStatusDisplay();

/// Called from main.cpp when a new TEXT_MESSAGE arrives.
void onStatusDisplayMessage(const char* srcCall, const char* text, const char* dstGroup, const char* dstCall);

#endif // HELTEC_WIFI_LORA_32_V3
