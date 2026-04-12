#pragma once

#ifdef HAS_BLE  // ESP32 boards only (NimBLE); nRF52 uses its own BLE stack

#include <functional>
#include <string>

using BleRxCallback = std::function<void(const std::string&)>;

using BleConnectCallback = std::function<void(bool connected)>;

void bleTransportInit(const char* mycall, BleRxCallback onRx);
void bleTransportDeinit();
void bleTransportSend(const std::string& json);
void bleTransportNotifyNewMessage(uint8_t msgId, const char* srcCall);
void bleTransportTick();
bool bleTransportIsConnected();
void bleTransportSetConnectCallback(BleConnectCallback cb);

#else

// No-op stubs for non-WiFi builds
inline void bleTransportInit(const char*, ...) {}
inline void bleTransportDeinit() {}
inline void bleTransportSend(const std::string&) {}
inline void bleTransportNotifyNewMessage(uint8_t, const char*) {}
inline void bleTransportTick() {}
inline bool bleTransportIsConnected() { return false; }
inline void bleTransportSetConnectCallback(...) {}

#endif
