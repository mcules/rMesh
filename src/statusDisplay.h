#pragma once

#include <stdint.h>

/**
 * @file statusDisplay.h
 * @brief Common entry points that any status-display driver may implement.
 *
 * Both functions are declared unconditionally so that platform-agnostic call
 * sites (e.g. firmware update flow) can invoke them without sprinkling
 * `#ifdef` blocks per board. Drivers that own a screen capable of rendering
 * the splash/flashing notification provide a strong implementation in their
 * own translation unit; everyone else falls back to the weak no-op default
 * defined in `displayCommon.cpp`.
 *
 * Implementations MUST display unconditionally — even when the OLED has been
 * disabled by the user — because the boot splash and the firmware-update
 * notification are intentional overrides of that setting.
 */

/// Show a brief boot splash screen for @p holdMs milliseconds. Drivers that
/// implement this should put the panel back into power-save mode after the
/// hold elapses if the user has disabled the display in settings.
void showStatusDisplaySplash(uint32_t holdMs);

/// Show a "Flashing <what>" full-screen notification while a firmware or
/// filesystem update is in progress. The screen stays locked until the device
/// reboots — drivers must ignore subsequent `updateStatusDisplay()` calls.
void showStatusDisplayFlashing(const char* what);