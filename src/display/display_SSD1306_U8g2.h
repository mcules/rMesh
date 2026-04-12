#pragma once

#include "displayPagesU8g2.h"
#include "statusDisplay.h"

/**
 * @brief Generic SSD1306 128x64 OLED status display driver (U8g2).
 *
 * Shared by all boards with a U8g2-driven SSD1306 OLED display.
 * Board-specific pin configuration comes from IBoardConfig at runtime.
 * Optional page-cycle button support via the oledButtonPin setting.
 */
