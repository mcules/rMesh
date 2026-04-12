#pragma once

#include <stdint.h>
#include <stddef.h>

/**
 * @file statusDisplay.h
 * @brief Common entry points that any display driver may implement.
 *
 * All functions are declared unconditionally so that platform-agnostic call
 * sites (e.g. main.cpp, firmware update flow) can invoke them without
 * sprinkling `#ifdef` blocks per board.  Drivers that own a display provide
 * strong implementations in their own translation unit; everyone else falls
 * back to the weak no-op defaults defined in `displayCommon.cpp`.
 */

class IBoardConfig;
class Frame;

// ── Status display (OLED / TFT / E-Paper) ──────────────────────────────────

/// Initialise the status display using pin information from the board config.
/// Returns true if a display was detected and initialised.
bool initStatusDisplay(const IBoardConfig* board);

/// Show a brief boot splash screen for @p holdMs milliseconds.
void showStatusDisplaySplash(uint32_t holdMs);

/// Show a "Flashing <what>" full-screen notification while a firmware or
/// filesystem update is in progress.
void showStatusDisplayFlashing(const char* what);

/// Periodic display refresh — called from the main loop.
void updateStatusDisplay();

/// Enable the display (user setting).
void enableStatusDisplay();

/// Disable the display (user setting).
void disableStatusDisplay();

/// Returns true if a display was detected during init.
bool hasStatusDisplay();

/// Notify the display driver of a new incoming message.
void onStatusDisplayMessage(const char* srcCall, const char* text,
                            const char* dstGroup, const char* dstCall);

// ── Full-UI display (T-LoraPager, SenseCAP Indicator) ───────────────────────

/// Initialise the full-UI display (called instead of initStatusDisplay).
void initDisplay();

/// Periodic display tick for full-UI boards.
void displayUpdateLoop();

/// Notify full-UI display of an incoming message.
void displayOnNewMessage(const char* srcCall, const char* text,
                         const char* dstGroup = "",
                         const char* dstCall = "");

/// Notify full-UI display of an outgoing frame.
void displayTxFrame(const char* dstCall, const char* text);

/// Notify full-UI display of a monitored frame.
void displayMonitorFrame(const Frame& f);

// ── E22-specific extras ─────────────────────────────────────────────────────

/// Poll the hardware page-cycle button (E22 board).
void displayButtonPoll();

// ── Shared utilities ────────────────────────────────────────────────────────

/// Check whether a message matches the user-configured display group filter.
bool matchesDisplayGroup(const char* dstGroup, const char* dstCall);

/// Convert a UTF-8 string to CP437 encoding (German umlauts + common accents).
/// Unmapped codepoints are replaced with '?'.
void utf8ToCP437(const char* src, char* dst, size_t dstLen);

// ── T-LoraPager SD card ─────────────────────────────────────────────────────

/// Returns true if an SD card is mounted (T-LoraPager only).
bool pagerSdAvailable();

/// Archive a message JSON record to the SD card (T-LoraPager only).
void pagerAddMessageToSD(const char* json, size_t len);
