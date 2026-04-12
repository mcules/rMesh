#ifdef HAS_BLE

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "ble_transport.h"
#include "util/logging.h"

// Nordic UART Service UUIDs
static const char* NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char* NUS_RX_UUID      = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"; // phone → node (WRITE)
static const char* NUS_TX_UUID      = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"; // node → phone (NOTIFY)

static NimBLEServer*         s_server  = nullptr;
static NimBLECharacteristic* s_txChar  = nullptr;
static NimBLEAdvertising*    s_adv     = nullptr;
static BleRxCallback         s_onRx;
static std::string           s_rxBuf;
static bool                  s_clientConnected = false;
static BleConnectCallback    s_connectCb;

// Manufacturer-specific advertisement data (10 bytes)
// Byte 0-1: company id 0x0808 (rMesh)
// Byte 2:   event type (0x00=idle, 0x01=new message)
// Byte 3:   message ID
// Byte 4-9: srcCall (null-padded)
static uint8_t s_mfgData[10] = { 0x08, 0x08, 0x00, 0x00, 0,0,0,0,0,0 };
static uint32_t s_notifyRevertMs = 0;

// ── Server callbacks ────────────────────────────────────────────────────────

class ServerCB : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer*) override {
        s_clientConnected = true;
        logPrintf(LOG_INFO, "BLE", "Client connected");
        if (s_connectCb) s_connectCb(true);
    }
    void onDisconnect(NimBLEServer*) override {
        s_clientConnected = false;
        logPrintf(LOG_INFO, "BLE", "Client disconnected");
        if (s_connectCb) s_connectCb(false);
        if (s_adv) s_adv->start();
    }
};

// ── RX characteristic callbacks (phone → node) ─────────────────────────────

class RxCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar) override {
        std::string chunk = pChar->getValue();
        s_rxBuf += chunk;

        // Process complete lines (newline-delimited JSON)
        size_t pos;
        while ((pos = s_rxBuf.find('\n')) != std::string::npos) {
            std::string line = s_rxBuf.substr(0, pos);
            s_rxBuf.erase(0, pos + 1);
            if (!line.empty() && s_onRx) {
                s_onRx(line);
            }
        }
    }
};

// ── Public API ──────────────────────────────────────────────────────────────

void bleTransportInit(const char* mycall, BleRxCallback onRx) {
    s_onRx = onRx;

    // Device name: "rMesh-<mycall>"
    std::string devName = "rMesh-";
    devName += mycall;

    NimBLEDevice::init(devName);
    NimBLEDevice::setMTU(247);

    s_server = NimBLEDevice::createServer();
    s_server->setCallbacks(new ServerCB());

    NimBLEService* svc = s_server->createService(NUS_SERVICE_UUID);

    // TX characteristic (node → phone): NOTIFY
    s_txChar = svc->createCharacteristic(
        NUS_TX_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    // RX characteristic (phone → node): WRITE + WRITE_NR
    NimBLECharacteristic* rxChar = svc->createCharacteristic(
        NUS_RX_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    rxChar->setCallbacks(new RxCB());

    svc->start();

    // Configure advertising
    s_adv = NimBLEDevice::getAdvertising();
    s_adv->addServiceUUID(NUS_SERVICE_UUID);
    s_adv->setScanResponse(true);

    // Set manufacturer data (idle state)
    NimBLEAdvertisementData scanResp;
    scanResp.setManufacturerData(std::string((char*)s_mfgData, sizeof(s_mfgData)));
    s_adv->setScanResponseData(scanResp);

    s_adv->start();
    logPrintf(LOG_INFO, "BLE", "NUS ready, advertising as '%s'", devName.c_str());
}

void bleTransportSend(const std::string& json) {
    if (!s_clientConnected || !s_txChar) return;

    std::string frame = json + "\n";
    uint16_t mtu = NimBLEDevice::getMTU() - 3;
    if (mtu < 20) mtu = 20;

    // Send in MTU-sized chunks
    for (size_t off = 0; off < frame.size(); off += mtu) {
        size_t len = std::min((size_t)mtu, frame.size() - off);
        s_txChar->setValue((const uint8_t*)frame.c_str() + off, len);
        s_txChar->notify();
    }
}

void bleTransportNotifyNewMessage(uint8_t msgId, const char* srcCall) {
    // Update manufacturer data: event=0x01, msgId, srcCall
    s_mfgData[2] = 0x01;
    s_mfgData[3] = msgId;
    memset(&s_mfgData[4], 0, 6);
    if (srcCall) {
        size_t len = strlen(srcCall);
        if (len > 6) len = 6;
        memcpy(&s_mfgData[4], srcCall, len);
    }

    // Restart advertising with updated data
    if (s_adv) {
        NimBLEAdvertisementData scanResp;
        scanResp.setManufacturerData(std::string((char*)s_mfgData, sizeof(s_mfgData)));
        s_adv->setScanResponseData(scanResp);
        if (!s_clientConnected) {
            s_adv->start();
        }
    }

    // Schedule revert to idle after 10 seconds
    s_notifyRevertMs = millis() + 10000;
}

// Call from main loop to revert advertisement to idle after timeout
void bleTransportTick() {
    if (s_notifyRevertMs != 0 && (int32_t)(millis() - s_notifyRevertMs) >= 0) {
        s_notifyRevertMs = 0;
        s_mfgData[2] = 0x00;
        s_mfgData[3] = 0x00;
        memset(&s_mfgData[4], 0, 6);
        if (s_adv) {
            NimBLEAdvertisementData scanResp;
            scanResp.setManufacturerData(std::string((char*)s_mfgData, sizeof(s_mfgData)));
            s_adv->setScanResponseData(scanResp);
        }
    }
}

void bleTransportDeinit() {
    if (s_adv) s_adv->stop();
    NimBLEDevice::deinit(true);
    s_server = nullptr;
    s_txChar = nullptr;
    s_adv = nullptr;
    s_clientConnected = false;
    logPrintf(LOG_INFO, "BLE", "Deinitialized");
}

bool bleTransportIsConnected() {
    return s_clientConnected;
}

void bleTransportSetConnectCallback(BleConnectCallback cb) {
    s_connectCb = cb;
}

#endif // HAS_BLE
