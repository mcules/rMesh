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
 * Peer entry (12 bytes):
 *   [nodeCall : 10 bytes][port : 1 byte][available : 1 byte]
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
#include "routing.h"
#include "peer.h"
#include "main.h"
#include "config.h"
#include "settings.h"

static const char* ROUTES_FILE = "/routes.bin";
static const char* PEERS_FILE  = "/peers.bin";
static const uint8_t FILE_VERSION = 1;

volatile bool routesDirty = false;
volatile bool peersDirty  = false;

// ── Route persistence ────────────────────────────────────────────────────────

/** Packed on-disk route entry (no timestamp — regenerated on load). */
struct __attribute__((packed)) RouteEntry {
    char srcCall[MAX_CALLSIGN_LENGTH + 1];
    char viaCall[MAX_CALLSIGN_LENGTH + 1];
    uint8_t hopCount;
    uint8_t _pad;
};

void loadRoutes() {
    if (!xSemaphoreTake(fsMutex, pdMS_TO_TICKS(5000))) {
        Serial.println(F("[Persistence] fsMutex timeout in loadRoutes"));
        return;
    }

    File f = LittleFS.open(ROUTES_FILE, "r");
    if (!f) {
        xSemaphoreGive(fsMutex);
        Serial.println(F("[Persistence] No routes file found — starting fresh"));
        return;
    }

    uint8_t version = 0;
    f.read(&version, 1);
    if (version != FILE_VERSION) {
        Serial.printf("[Persistence] Unknown routes file version %d — skipping\n", version);
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

    Serial.printf("[Persistence] Loaded %d routes from %s\n", loaded, ROUTES_FILE);
}

static void saveRoutesTask(void* pvParameters) {
    if (!xSemaphoreTake(fsMutex, pdMS_TO_TICKS(30000))) {
        Serial.println(F("[Persistence] fsMutex timeout in saveRoutesTask"));
        vTaskDelete(NULL);
        return;
    }

    File f = LittleFS.open(ROUTES_FILE, "w");
    if (!f) {
        Serial.println(F("[Persistence] Failed to open routes file for writing"));
        xSemaphoreGive(fsMutex);
        vTaskDelete(NULL);
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

    routesDirty = false;
    Serial.printf("[Persistence] Saved %d routes to %s\n", count, ROUTES_FILE);
    vTaskDelete(NULL);
}

void saveRoutes() {
    xTaskCreate(saveRoutesTask, "SaveRoutes", 4096, NULL, 1, NULL);
}

// ── Peer persistence ─────────────────────────────────────────────────────────

/** Packed on-disk peer entry (no RSSI/SNR — stale after reboot). */
struct __attribute__((packed)) PeerEntry {
    char nodeCall[MAX_CALLSIGN_LENGTH + 1];
    uint8_t port;
    uint8_t available;
};

void loadPeers() {
    if (!xSemaphoreTake(fsMutex, pdMS_TO_TICKS(5000))) {
        Serial.println(F("[Persistence] fsMutex timeout in loadPeers"));
        return;
    }

    File f = LittleFS.open(PEERS_FILE, "r");
    if (!f) {
        xSemaphoreGive(fsMutex);
        Serial.println(F("[Persistence] No peers file found — starting fresh"));
        return;
    }

    uint8_t version = 0;
    f.read(&version, 1);
    if (version != FILE_VERSION) {
        Serial.printf("[Persistence] Unknown peers file version %d — skipping\n", version);
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

    PeerEntry entry;
    uint16_t loaded = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (f.read((uint8_t*)&entry, sizeof(entry)) != sizeof(entry)) break;

        // Skip own callsign
        if (strcmp(settings.mycall, entry.nodeCall) == 0) continue;

        Peer p;
        memcpy(p.nodeCall, entry.nodeCall, sizeof(p.nodeCall));
        p.nodeCall[MAX_CALLSIGN_LENGTH] = '\0';
        p.port = entry.port;
        p.available = entry.available;
        p.timestamp = graceTimestamp;
        p.rssi = 0;
        p.snr = 0;
        p.frqError = 0;
        peerList.push_back(p);
        loaded++;
    }

    f.close();
    xSemaphoreGive(fsMutex);

    Serial.printf("[Persistence] Loaded %d peers from %s (grace %ds)\n",
                  loaded, PEERS_FILE, PEER_INITIAL_TIMEOUT);
}

static void savePeersTask(void* pvParameters) {
    if (!xSemaphoreTake(fsMutex, pdMS_TO_TICKS(30000))) {
        Serial.println(F("[Persistence] fsMutex timeout in savePeersTask"));
        vTaskDelete(NULL);
        return;
    }

    File f = LittleFS.open(PEERS_FILE, "w");
    if (!f) {
        Serial.println(F("[Persistence] Failed to open peers file for writing"));
        xSemaphoreGive(fsMutex);
        vTaskDelete(NULL);
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
        f.write((uint8_t*)&entry, sizeof(entry));
    }

    f.close();
    xSemaphoreGive(fsMutex);

    peersDirty = false;
    Serial.printf("[Persistence] Saved %d peers to %s\n", count, PEERS_FILE);
    vTaskDelete(NULL);
}

void savePeers() {
    xTaskCreate(savePeersTask, "SavePeers", 4096, NULL, 1, NULL);
}