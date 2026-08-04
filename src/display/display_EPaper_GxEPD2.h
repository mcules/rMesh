#pragma once

/**
 * @file display_LILYGO_T-Echo.h
 * @brief E-Paper display driver for LilyGo T-Echo (GDEH0154D67 200x200).
 *
 * Uses GxEPD2 library for the 1.54" black/white e-paper display.
 * Provides status display with callsign, battery, peers, and last message.
 * Button cycles through display pages.
 *
 * Public API is declared in statusDisplay.h; this header exists for
 * driver-internal declarations only.
 */

#include <Arduino.h>

// Display page enum
enum DisplayPage {
    PAGE_STATUS = 0,    // Callsign, LoRa config, battery, uptime
    PAGE_PEERS,         // Peer list with RSSI/SNR
    PAGE_ROUTES,        // Routing table
    PAGE_MESSAGES,      // Last received messages
    PAGE_COUNT
};
