#pragma once

#include "mesh/frame.h"

/*
 * display_SEEED_SenseCAP_Indicator.h
 *
 * UI for the Seeed SenseCAP Indicator: ST7701S 480x480 RGB display.
 * Same UI structure as the T-LoraPager, adapted for square display.
 *
 * Layout (480 × 480):
 *   ┌─────────────────────────────────────────┐  y=0
 *   │  Header (Callsign | Status | Time)       │  18 px
 *   ├─────────────────────────────────────────┤  y=18
 *   │  Gruppen-Tabs                           │  18 px
 *   ├─────────────────────────────────────────┤  y=36
 *   │                                         │
 *   │  Message list       (480 x 412 px)       │
 *   │                                         │
 *   ├─────────────────────────────────────────┤  y=448
 *   │  Status-Bar        (480 ×  32 px)       │
 *   └─────────────────────────────────────────┘  y=480
 */

void initDisplay();
void displayUpdateLoop();
void displayOnNewMessage(const char* srcCall, const char* text, const char* dstGroup, const char* dstCall);
void displayTxFrame(const char* dstCall, const char* text);
void displayMonitorFrame(const Frame& f);

// PCA9535 access for LoRa HAL (bits 0-3 = LORA CS/RST/BUSY/DIO1)
void     pca9535_write_bit(int bit, bool value);
bool     pca9535_read_bit(int bit);
