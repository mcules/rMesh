/**
 * @file persistence.cpp
 * @brief Flash persistence for routing table and peer list.
 *
 * Binary file format (both files):
 *   [version : 1 byte][count : 2 bytes (LE)][entries…]
 *
 * Route entry (22 bytes):
 *   [srcCall : 10 bytes][viaCall : 10 bytes][hopCount : 1 byte][padding : 1 byte]
 *
 * Peer entry V1 (12 bytes):
 *   [nodeCall : 10 bytes][port : 1 byte][available : 1 byte]
 * Peer entry V2 (24 bytes):
 *   [nodeCall : 10 bytes][port : 1 byte][available : 1 byte]
 *   [rssi : 4 bytes][snr : 4 bytes][frqError : 4 bytes]
 *
 * Timestamps are NOT persisted for routes (set to now on load).
 * For peers, the timestamp is set so that the PEER_INITIAL_TIMEOUT grace
 * period applies instead of the full PEER_INACTIVE_TIMEOUT.
 */

#include <Arduino.h>
#ifdef NRF52_PLATFORM
#include "platform_nrf52.h"
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#else
#include <LittleFS.h>
#endif

#include "persistence.h"
#include "heapdbg.h"
#include "bgWorker.h"
#include "routing.h"
#include "peer.h"
#include "main.h"
#include "config.h"
#include "settings.h"
#include "logging.h"

static const char* ROUTES_FILE = "/routes.bin";
static const char* PEERS_FILE  = "/peers.bin";
static const uint8_t FILE_VERSION = 2;
static const uint8_t PEER_FILE_VERSION_V1 = 1;

volatile bool routesDirty = false;
volatile bool peersDirty  = false;

// Re-entrance guards: prevent piling up multiple save tasks (each 4KB stack)
// if a previous save has not finished before the next PERSIST_INTERVAL tick.
static volatile bool saveRoutesInProgress = false;
static volatile bool savePeersInProgress  = false;

// ── Route persistence ────────────────────────────────────────────────────────

/** Packed on-disk route entry (no timestamp — regenerated on load). */
struct __attribute__((packed)) RouteEntry {
    char srcCall[MAX_CALLSIGN_LENGTH + 1];
    char viaCall[MAX_CALLSIGN_LENGTH + 1];
    uint8_t hopCount;
    uint8_t _pad;
};

void loadRoutes() {
    HEAP_SCOPE("loadRoutes");
    if (!xSemaphoreTake(fsMutex, pdMS_TO_TICKS(5000))) {
        logPrintf(LOG_ERROR, "FS", "fsMutex timeout in loadRoutes");
        return;
    }

    File f = LittleFS.open(ROUTES_FILE, "r");
    if (!f) {
        xSemaphoreGive(fsMutex);
        logPrintf(LOG_INFO, "FS", "No routes file found — starting fresh");
        return;
    }

    uint8_t version = 0;
    f.read(&version, 1);
    if (version != FILE_VERSION && version != PEER_FILE_VERSION_V1) {
        logPrintf(LOG_WARN, "FS", "Unknown routes file version %d — skipping", version);
        f.close();
        xSemaphoreGive(fsMutex);
        return;
    }

    uint16_t count = 0;
    f.read((uint8_t*)&count, 2);

    if (count > ROUTING_BUFFER_SIZE) count = ROUTING_BUFFER_SIZE;

    time_t now = time(NULL);
    routingList.clear();
    routingList.reserve(count);

    RouteEntry entry;
    uint16_t loaded = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (f.read((uint8_t*)&entry, sizeof(entry)) != sizeof(entry)) break;

        // Skip routes to own callsign
        if (strcmp(settings.mycall, entry.srcCall) == 0) continue;

        Route r;
        memcpy(r.srcCall, entry.srcCall, sizeof(r.srcCall));
        r.srcCall[MAX_CALLSIGN_LENGTH] = '\0';
        memcpy(r.viaCall, entry.viaCall, sizeof(r.viaCall));
        r.viaCall[MAX_CALLSIGN_LENGTH] = '\0';
        r.hopCount = entry.hopCount;
        r.timestamp = now;
        routingList.push_back(r);
        loaded++;
    }

    f.close();
    xSemaphoreGive(fsMutex);

    // Sort like addRoutingList does
    std::sort(routingList.begin(), routingList.end(), [](const Route& a, const Route& b) {
        if (a.hopCount != b.hopCount) return a.hopCount < b.hopCount;
        return a.timestamp > b.timestamp;
    });

    logPrintf(LOG_INFO, "FS", "Loaded %d routes from %s", loaded, ROUTES_FILE);
}

// Worker function — runs on the shared bgWorker task, no own stack alloc.
static void saveRoutesWork() {
    uint32_t _hf0 = ESP.getFreeHeap();
    uint32_t _hm0 = ESP.getMaxAllocHeap();

    if (!xSemaphoreTake(listMutex, pdMS_TO_TICKS(1000))) {
        logPrintf(LOG_ERROR, "FS", "listMutex timeout in saveRoutes");
        heapRecord("saveRoutes/listTO", _hf0, _hm0);
        saveRoutesInProgress = false;
        return;
    }
    if (!xSemaphoreTake(fsMutex, pdMS_TO_TICKS(30000))) {
        logPrintf(LOG_ERROR, "FS", "fsMutex timeout in saveRoutes");
        xSemaphoreGive(listMutex);
        heapRecord("saveRoutes/fsTO", _hf0, _hm0);
        saveRoutesInProgress = false;
        return;
    }

    File f = LittleFS.open(ROUTES_FILE, "w");
    if (!f) {
        logPrintf(LOG_ERROR, "FS", "Failed to open routes file for writing");
        xSemaphoreGive(fsMutex);
        xSemaphoreGive(listMutex);
        heapRecord("saveRoutes/openFail", _hf0, _hm0);
        saveRoutesInProgress = false;
        return;
    }

    f.write(&FILE_VERSION, 1);
    uint16_t count = routingList.size();
    f.write((uint8_t*)&count, 2);

    RouteEntry entry;
    memset(&entry, 0, sizeof(entry));
    for (const auto& r : routingList) {
        memcpy(entry.srcCall, r.srcCall, sizeof(entry.srcCall));
        memcpy(entry.viaCall, r.viaCall, sizeof(entry.viaCall));
        entry.hopCount = r.hopCount;
        entry._pad = 0;
        f.write((uint8_t*)&entry, sizeof(entry));
    }

    f.close();
    xSemaphoreGive(fsMutex);
    xSemaphoreGive(listMutex);

    routesDirty = false;
    logPrintf(LOG_INFO, "FS", "Saved %d routes to %s", count, ROUTES_FILE);
    heapRecord("saveRoutes/done", _hf0, _hm0);
    saveRoutesInProgress = false;
}

void saveRoutes() {
    if (saveRoutesInProgress) return;
    saveRoutesInProgress = true;
    HEAP_MARK("saveRoutes/enq");
    if (!bgWorkerEnqueue(saveRoutesWork)) {
        logPrintf(LOG_WARN, "FS", "saveRoutes enqueue failed (queue full?)");
        saveRoutesInProgress = false;
    }
}

// ── Peer persistence ─────────────────────────────────────────────────────────

/** Packed on-disk peer entry V1 (no RSSI/SNR). */
struct __attribute__((packed)) PeerEntryV1 {
    char nodeCall[MAX_CALLSIGN_LENGTH + 1];
    uint8_t port;
    uint8_t available;
};

/** Packed on-disk peer entry V2 (with RSSI/SNR/frqError). */
struct __attribute__((packed)) PeerEntry {
    char nodeCall[MAX_CALLSIGN_LENGTH + 1];
    uint8_t port;
    uint8_t available;
    float rssi;
    float snr;
    float frqError;
};

void loadPeers() {
    HEAP_SCOPE("loadPeers");
    if (!xSemaphoreTake(fsMutex, pdMS_TO_TICKS(5000))) {
        logPrintf(LOG_ERROR, "FS", "fsMutex timeout in loadPeers");
        return;
    }

    File f = LittleFS.open(PEERS_FILE, "r");
    if (!f) {
        xSemaphoreGive(fsMutex);
        logPrintf(LOG_INFO, "FS", "No peers file found — starting fresh");
        return;
    }

    uint8_t version = 0;
    f.read(&version, 1);
    if (version != FILE_VERSION && version != PEER_FILE_VERSION_V1) {
        logPrintf(LOG_WARN, "FS", "Unknown peers file version %d — skipping", version);
        f.close();
        xSemaphoreGive(fsMutex);
        return;
    }

    uint16_t count = 0;
    f.read((uint8_t*)&count, 2);

    if (count > PEER_LIST_SIZE) count = PEER_LIST_SIZE;

    time_t now = time(NULL);
    // Set timestamp so peer becomes inactive after PEER_INITIAL_TIMEOUT seconds
    // instead of the full PEER_INACTIVE_TIMEOUT.
    time_t graceTimestamp = now - (PEER_INACTIVE_TIMEOUT - PEER_INITIAL_TIMEOUT);

    peerList.clear();
    peerList.reserve(count);

    uint16_t loaded = 0;
    for (uint16_t i = 0; i < count; i++) {
        Peer p;
        if (version == PEER_FILE_VERSION_V1) {
            PeerEntryV1 entry;
            if (f.read((uint8_t*)&entry, sizeof(entry)) != sizeof(entry)) break;
            if (strcmp(settings.mycall, entry.nodeCall) == 0) continue;
            memcpy(p.nodeCall, entry.nodeCall, sizeof(p.nodeCall));
            p.port = entry.port;
            p.available = entry.available;
            p.rssi = 0;
            p.snr = 0;
            p.frqError = 0;
        } else {
            PeerEntry entry;
            if (f.read((uint8_t*)&entry, sizeof(entry)) != sizeof(entry)) break;
            if (strcmp(settings.mycall, entry.nodeCall) == 0) continue;
            memcpy(p.nodeCall, entry.nodeCall, sizeof(p.nodeCall));
            p.port = entry.port;
            p.available = entry.available;
            p.rssi = entry.rssi;
            p.snr = entry.snr;
            p.frqError = entry.frqError;
        }
        p.nodeCall[MAX_CALLSIGN_LENGTH] = '\0';
        p.timestamp = graceTimestamp;
        peerList.push_back(p);
        loaded++;
    }

    f.close();
    xSemaphoreGive(fsMutex);

    logPrintf(LOG_INFO, "FS", "Loaded %d peers from %s (grace %ds)",
                  loaded, PEERS_FILE, PEER_INITIAL_TIMEOUT);
}

static void savePeersWork() {
    uint32_t _hf0 = ESP.getFreeHeap();
    uint32_t _hm0 = ESP.getMaxAllocHeap();

    if (!xSemaphoreTake(listMutex, pdMS_TO_TICKS(1000))) {
        logPrintf(LOG_ERROR, "FS", "listMutex timeout in savePeers");
        heapRecord("savePeers/listTO", _hf0, _hm0);
        savePeersInProgress = false;
        return;
    }
    if (!xSemaphoreTake(fsMutex, pdMS_TO_TICKS(30000))) {
        logPrintf(LOG_ERROR, "FS", "fsMutex timeout in savePeers");
        xSemaphoreGive(listMutex);
        heapRecord("savePeers/fsTO", _hf0, _hm0);
        savePeersInProgress = false;
        return;
    }

    File f = LittleFS.open(PEERS_FILE, "w");
    if (!f) {
        logPrintf(LOG_ERROR, "FS", "Failed to open peers file for writing");
        xSemaphoreGive(fsMutex);
        xSemaphoreGive(listMutex);
        heapRecord("savePeers/openFail", _hf0, _hm0);
        savePeersInProgress = false;
        return;
    }

    f.write(&FILE_VERSION, 1);
    uint16_t count = peerList.size();
    f.write((uint8_t*)&count, 2);

    PeerEntry entry;
    memset(&entry, 0, sizeof(entry));
    for (const auto& p : peerList) {
        memcpy(entry.nodeCall, p.nodeCall, sizeof(entry.nodeCall));
        entry.port = p.port;
        entry.available = p.available ? 1 : 0;
        entry.rssi = p.rssi;
        entry.snr = p.snr;
        entry.frqError = p.frqError;
        f.write((uint8_t*)&entry, sizeof(entry));
    }

    f.close();
    xSemaphoreGive(fsMutex);
    xSemaphoreGive(listMutex);

    peersDirty = false;
    logPrintf(LOG_INFO, "FS", "Saved %d peers to %s", count, PEERS_FILE);
    heapRecord("savePeers/done", _hf0, _hm0);
    savePeersInProgress = false;
}

void savePeers() {
    if (savePeersInProgress) return;
    savePeersInProgress = true;
    HEAP_MARK("savePeers/enq");
    if (!bgWorkerEnqueue(savePeersWork)) {
        logPrintf(LOG_WARN, "FS", "savePeers enqueue failed (queue full?)");
        savePeersInProgress = false;
    }
}