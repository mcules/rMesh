#pragma once

#ifdef LILYGO_T_BEAM

/**
 * @brief SSD1306 OLED status display for the LILYGO T-Beam (128x32).
 *
 * Shows callsign, battery, WiFi mode, IP, SSID and last received message.
 * Controlled via boot button (short press) and oledEnabled setting (persisted).
 */

/// Probe I2C for OLED and initialise U8g2 if found.
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

#endif // LILYGO_T_BEAM
