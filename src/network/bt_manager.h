#pragma once

#include <stdint.h>

/**
 * @file bt_manager.h
 * @brief BLE mode management with WiFi coexistence.
 *
 * Three modes (NVS-persistent):
 *   OFF       — BLE completely disabled
 *   COEX      — WiFi + BLE run simultaneously
 *   EXCLUSIVE — WiFi paused while a BLE client is connected
 */

enum class BtMode : uint8_t {
    OFF       = 0,
    COEX      = 1,
    EXCLUSIVE = 2
};

/// Initialise the BT manager.  Reads mode from NVS, starts BLE if needed.
/// Must be called AFTER loadSettings() and wifiInit().
void btManagerInit();

/// Switch to a new BT mode.  Persists to NVS immediately.
void btManagerSetMode(BtMode mode);

/// Returns the current BT mode.
BtMode btManagerGetMode();

/// Cycle to the next mode: OFF → COEX → EXCLUSIVE → OFF.
void btManagerCycleMode();

/// Returns true if a BLE client is currently connected.
bool btManagerIsConnected();

/// Call from loop() — manages WiFi recovery in EXCLUSIVE mode.
void btManagerTick();
