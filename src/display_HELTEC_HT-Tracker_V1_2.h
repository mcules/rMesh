#pragma once

#ifdef HELTEC_HT_TRACKER_V1_2

/**
 * @brief ST7735 TFT status display for the Heltec HT-Tracker V1.2 (80x160).
 *
 * Shows callsign, battery, WiFi mode, IP, SSID and last received message.
 * Controlled via boot button (short press) and oledEnabled setting (persisted).
 */

/// Initialise TFT display via SPI.
bool initStatusDisplay();

/// Redraw the status screen.
void updateStatusDisplay();

/// Turn display on, draw content, set oledEnabled = true and persist.
void enableStatusDisplay();

/// Turn display off (backlight), set oledEnabled = false and persist.
void disableStatusDisplay();

/// Returns true if the display was initialised successfully.
bool hasStatusDisplay();

/// Called from main.cpp when a new TEXT_MESSAGE arrives.
void onStatusDisplayMessage(const char* srcCall, const char* text, const char* dstGroup, const char* dstCall);

#endif // HELTEC_HT_TRACKER_V1_2
