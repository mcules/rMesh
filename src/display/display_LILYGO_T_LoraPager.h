#pragma once

/*
 * display_LILYGO_T_LoraPager.h
 *
 * Chat-Display + QWERTY-Keyboard für den LILYGO T-LoraPager.
 *
 * Architektur:
 *   - LilyGoLib  : Hardware-Init (SPI-Bus, IO-Expander, Keyboard-Power,
 *                  SPI-Lock, getKeyChar)
 *   - LovyanGFX  : Text-Rendering auf dem ST7796-Display
 *                  (nutzt denselben SPI-Bus via instance.lockSPI / unlockSPI)
 *
 * UI-Layout (Landscape, 480 × 222):
 *   ┌─────────────────────────────────────┐  y=0
 *   │  Nachrichtenliste  (480 × 196 px)   │
 *   ├─────────────────────────────────────┤  y=196
 *   │  Eingabezeile      (480 ×  26 px)   │
 *   └─────────────────────────────────────┘  y=222
 */

#include "mesh/frame.h"

void initDisplay();
void displayUpdateLoop();
void displayOnNewMessage(const char* srcCall, const char* text, const char* dstGroup, const char* dstCall);
void displayTxFrame(const char* dstCall, const char* text);
void displayMonitorFrame(const Frame& f);
