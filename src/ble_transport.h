#pragma once

#ifdef HAS_WIFI  // ESP32 boards only (NimBLE); nRF52 uses its own BLE stack

#include <functional>
#include <string>

using BleRxCallback = std::function<void(const std::string&)>;

void bleTransportInit(const char* mycall, BleRxCallback onRx);
void bleTransportSend(const std::string& json);
void bleTransportNotifyNewMessage(uint8_t msgId, const char* srcCall);
void bleTransportTick();

#else

// No-op stubs for non-WiFi builds
inline void bleTransportInit(const char*, ...) {}
inline void bleTransportSend(const std::string&) {}
inline void bleTransportNotifyNewMessage(uint8_t, const char*) {}
inline void bleTransportTick() {}

#endif
