#ifdef HAS_WIFI

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

// ── BLE connect/disconnect callback ─────────────────────────────────────────

static void onBleConnect(bool connected) {
    if (s_mode != BtMode::EXCLUSIVE) return;

    if (connected && !s_wifiStopped) {
        // BLE client connected in EXCLUSIVE mode → pause WiFi
        logPrintf(LOG_INFO, "BT", "EXCLUSIVE: pausing WiFi for BLE");
        WiFi.mode(WIFI_OFF);
        s_wifiStopped = true;
    }
    if (!connected && s_wifiStopped) {
        // BLE client disconnected → restore WiFi
        logPrintf(LOG_INFO, "BT", "EXCLUSIVE: restoring WiFi");
        s_wifiStopped = false;
        wifiInit();
        startWebServer();
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
                logPrintf(LOG_WARN, "BT", "COEX requires PSRAM — falling back to EXCLUSIVE on this board");
                s_mode = BtMode::EXCLUSIVE;
                btMode = (uint8_t)BtMode::EXCLUSIVE;
            } else {
                logPrintf(LOG_INFO, "BT", "Mode COEX (WiFi + BLE, PSRAM available)");
            }
            startBle();
            break;
        case BtMode::EXCLUSIVE:
            if (ESP.getFreeHeap() < 60000) {
                logPrintf(LOG_WARN, "BT", "Heap too low for EXCLUSIVE (%u bytes) — falling back to OFF", ESP.getFreeHeap());
                s_mode = BtMode::OFF;
                btMode = 0;
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
                if (old == BtMode::OFF) startBle();
                break;
            }
            logPrintf(LOG_INFO, "BT", "Switching to COEX");
            if (old == BtMode::OFF) startBle();
            // If WiFi was stopped (from EXCLUSIVE), restore
            if (s_wifiStopped) {
                s_wifiStopped = false;
                wifiInit();
                startWebServer();
            }
            break;
        case BtMode::EXCLUSIVE:
            logPrintf(LOG_INFO, "BT", "Switching to EXCLUSIVE");
            if (old == BtMode::OFF) startBle();
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
