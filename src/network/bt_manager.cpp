#ifdef HAS_BLE

#include "bt_manager.h"
#include <Arduino.h>
#include <WiFi.h>
#include "network/ble_transport.h"
#include "network/wifiFunctions.h"
#include "network/webFunctions.h"
#include "hal/settings.h"
#include "util/logging.h"
#include "main.h"

static BtMode s_mode = BtMode::OFF;
static bool   s_bleRunning = false;
static bool   s_wifiStopped = false;
static bool   s_pendingWifiStop = false;   // deferred WiFi shutdown
static uint32_t s_wifiStopAt = 0;

static void stopBle();

// ── BLE connect/disconnect callback ─────────────────────────────────────────

static void onBleConnect(bool connected) {
    if (s_mode != BtMode::EXCLUSIVE) return;

    if (connected && !s_wifiStopped && !s_pendingWifiStop) {
        // BLE client connected in EXCLUSIVE mode → schedule WiFi shutdown
        logPrintf(LOG_INFO, "BT", "EXCLUSIVE: scheduling WiFi stop for BLE");
        stopWebServer();
        s_pendingWifiStop = true;
        s_wifiStopAt = millis() + 500;  // let AsyncTCP drain
    }
    if (!connected && s_wifiStopped) {
        // BLE client disconnected → stop BLE and restore WiFi
        logPrintf(LOG_INFO, "BT", "EXCLUSIVE: stopping BLE, restoring WiFi");
        stopBle();
    }
}

// ── Internal helpers ────────────────────────────────────────────────────────

static void startBle() {
    if (s_bleRunning) return;
    bleTransportInit(settings.mycall, [](const std::string& json) {
        extern void processBleJson(const char* json, size_t len);
        processBleJson(json.c_str(), json.size());
    });
    bleTransportSetConnectCallback(onBleConnect);
    s_bleRunning = true;
}

static void stopBle() {
    if (!s_bleRunning) return;
    bleTransportDeinit();
    s_bleRunning = false;
    // If WiFi was stopped for EXCLUSIVE, restore it
    if (s_wifiStopped) {
        s_wifiStopped = false;
        wifiInit();
        startWebServer();
    }
}

// ── Public API ──────────────────────────────────────────────────────────────

void btManagerInit() {
    s_mode = (BtMode)btMode;  // loaded from NVS in loadSettings()

    logPrintf(LOG_INFO, "BT", "Free heap before BLE init: %u", ESP.getFreeHeap());

    switch (s_mode) {
        case BtMode::OFF:
            logPrintf(LOG_INFO, "BT", "Mode OFF");
            break;
        case BtMode::COEX:
            if (!psramFound()) {
                logPrintf(LOG_WARN, "BT", "COEX requires PSRAM — BLE deferred until WiFi stops");
                s_mode = BtMode::EXCLUSIVE;
                btMode = (uint8_t)BtMode::EXCLUSIVE;
                // Do NOT start BLE now — will start on demand
                break;
            }
            logPrintf(LOG_INFO, "BT", "Mode COEX (WiFi + BLE, PSRAM available)");
            startBle();
            break;
        case BtMode::EXCLUSIVE:
            if (!psramFound()) {
                // On non-PSRAM boards: don't start BLE at boot — it eats 148 KB.
                // BLE will start lazily when user triggers it (double-click or WebUI).
                logPrintf(LOG_INFO, "BT", "Mode EXCLUSIVE (BLE deferred, starts on demand)");
                break;
            }
            logPrintf(LOG_INFO, "BT", "Mode EXCLUSIVE (WiFi paused on BLE connect)");
            startBle();
            break;
    }
    logPrintf(LOG_INFO, "BT", "Free heap after BLE init: %u", ESP.getFreeHeap());
}

void btManagerSetMode(BtMode mode) {
    if (mode == s_mode) return;

    BtMode old = s_mode;
    s_mode = mode;
    btMode = (uint8_t)mode;

    // Save to NVS
    saveOledSettings();  // reuses the general settings save path

    switch (mode) {
        case BtMode::OFF:
            logPrintf(LOG_INFO, "BT", "Switching to OFF");
            stopBle();
            break;
        case BtMode::COEX:
            if (!psramFound()) {
                logPrintf(LOG_WARN, "BT", "COEX requires PSRAM — using EXCLUSIVE instead");
                s_mode = BtMode::EXCLUSIVE;
                btMode = (uint8_t)BtMode::EXCLUSIVE;
                // Fall through to EXCLUSIVE logic below
            } else {
                logPrintf(LOG_INFO, "BT", "Switching to COEX");
                if (old == BtMode::OFF) startBle();
                if (s_wifiStopped) {
                    s_wifiStopped = false;
                    wifiInit();
                    startWebServer();
                }
                break;
            }
            // fall through for non-PSRAM EXCLUSIVE
        case BtMode::EXCLUSIVE:
            logPrintf(LOG_INFO, "BT", "Switching to EXCLUSIVE — scheduling WiFi stop, starting BLE");
            if (!s_wifiStopped && !s_pendingWifiStop) {
                stopWebServer();
                s_pendingWifiStop = true;
                s_wifiStopAt = millis() + 500;
            }
            // BLE starts in btManagerTick() after WiFi is fully down
            break;
    }
}

BtMode btManagerGetMode() {
    return s_mode;
}

void btManagerCycleMode() {
    switch (s_mode) {
        case BtMode::OFF:       btManagerSetMode(BtMode::COEX);      break;
        case BtMode::COEX:      btManagerSetMode(BtMode::EXCLUSIVE); break;
        case BtMode::EXCLUSIVE: btManagerSetMode(BtMode::OFF);       break;
    }
}

bool btManagerIsConnected() {
    return bleTransportIsConnected();
}

void btManagerTick() {
    // Deferred WiFi shutdown — gives AsyncTCP time to drain its event queue
    if (s_pendingWifiStop && (int32_t)(millis() - s_wifiStopAt) >= 0) {
        s_pendingWifiStop = false;
        logPrintf(LOG_INFO, "BT", "WiFi OFF now");
        WiFi.mode(WIFI_OFF);
        s_wifiStopped = true;
        // Now safe to start BLE (WiFi freed its heap)
        if (!s_bleRunning && s_mode == BtMode::EXCLUSIVE) {
            startBle();
            logPrintf(LOG_INFO, "BT", "Free heap after BLE start: %u", ESP.getFreeHeap());
        }
    }
    bleTransportTick();
}

#else // !HAS_WIFI

#include "bt_manager.h"

// No-op stubs for nRF52 and other non-WiFi builds
void btManagerInit() {}
void btManagerSetMode(BtMode) {}
BtMode btManagerGetMode() { return BtMode::OFF; }
void btManagerCycleMode() {}
bool btManagerIsConnected() { return false; }
void btManagerTick() {}

#endif
