/**
 * @file peer.cpp
 * @brief Peer list management implementation.
 *
 * Maintains `peerList`, the runtime table of known remote rMesh nodes.
 * Each entry is keyed by (callsign, port) so that a node reachable via both
 * LoRa and WiFi can be tracked independently and the preferred transport
 * selected automatically.
 */

#include <Arduino.h>
#include <ArduinoJson.h>

#include "frame.h"
#include "main.h"
#include "webFunctions.h"
#include "peer.h"
#include "reporting.h"
#include "helperFunctions.h"
#include "config.h"
#include "settings.h"

/** Runtime peer table; extern-declared in peer.h. */
std::vector<Peer> peerList;

/**
 * @brief Periodic peer-list maintenance — called once per second from loop().
 *
 * Three passes over the peer list:
 *  1. **Inactivity mark** — peers not heard for PEER_INACTIVE_TIMEOUT seconds
 *     are marked unavailable.
 *  2. **Expiry removal** — peers silent for PEER_TIMEOUT seconds are deleted.
 *  3. **Deduplication / transport preference** — for each callsign with
 *     multiple active entries, at most one remains available:
 *     WiFi (port 1) wins over LoRa (port 0); on the same port, higher SNR wins.
 *
 * Broadcasts the updated peer list via WebSocket and flags a topology change
 * if anything was modified.
 */
void checkPeerList() {
    bool update = false;
    time_t now = time(NULL);

    // Mark peers as unavailable after inactivity timeout
    for (auto& peer : peerList) {
        if (peer.available && (now - peer.timestamp) > PEER_INACTIVE_TIMEOUT) {
            peer.available = false;
            update = true;
            Serial.printf("[Peer] %s (Port %d) marked unavailable due to inactivity\n", peer.nodeCall, peer.port);
        }
    }

    // Remove peers after full timeout
    for (auto it = peerList.begin(); it != peerList.end();) {
        if ((now - it->timestamp) > PEER_TIMEOUT) {
            Serial.printf("[Peer] %s (Port %d) removed due to timeout\n", it->nodeCall, it->port);
            it = peerList.erase(it);
            update = true;
        } else {
            ++it;
        }
    }

    // Filter LoRa peers below minimum SNR threshold
    if (extSettings.minSnr > -20) {
        for (auto& peer : peerList) {
            if (peer.available && peer.port == 0 && peer.snr < extSettings.minSnr) {
                peer.available = false;
                update = true;
                Serial.printf("[Peer] %s (Port %d) below min SNR (%.1f < %d dB)\n",
                    peer.nodeCall, peer.port, peer.snr, extSettings.minSnr);
            }
        }
    }

    // Prefer WiFi (port 1) over LoRa (port 0); if same port, keep better SNR
    for (size_t i = 0; i < peerList.size(); i++) {
        if (!peerList[i].available) continue;

        for (size_t j = i + 1; j < peerList.size(); j++) {
            if (!peerList[j].available) continue;

            if (strcmp(peerList[i].nodeCall, peerList[j].nodeCall) != 0) continue;

            bool iWifi = (peerList[i].port == 1);
            bool jWifi = (peerList[j].port == 1);

            if (iWifi && !jWifi) {
                peerList[j].available = false;
                update = true;
            } else if (!iWifi && jWifi) {
                peerList[i].available = false;
                update = true;
                break;
            } else {
                if (peerList[i].snr < peerList[j].snr) {
                    peerList[i].available = false;
                    update = true;
                    break;
                } else {
                    peerList[j].available = false;
                    update = true;
                }
            }
        }
    }

    if (update) {
        sendPeerList();
        markTopologyChanged();
    }
}

/**
 * @brief Broadcast the current peer list as a JSON WebSocket message.
 *
 * Allocates a heap buffer sized to the exact serialised length, calls
 * wsBroadcast(), then frees the buffer.  Logs to Serial on allocation failure.
 *
 * JSON format:
 * @code
 * { "peerlist": { "peers": [
 *     { "port", "call", "timestamp", "rssi", "snr", "frqError", "available" },
 *     …
 * ] } }
 * @endcode
 */
void sendPeerList() {
    JsonDocument doc;
    doc["peerlist"]["peers"] = JsonArray();
    for (int i = 0; i < peerList.size(); i++) {
        // Check if same callsign exists on a different port (dual-path node)
        bool dualPath = false;
        for (int j = 0; j < peerList.size(); j++) {
            if (i != j && strcmp(peerList[i].nodeCall, peerList[j].nodeCall) == 0
                       && peerList[i].port != peerList[j].port) {
                dualPath = true;
                break;
            }
        }

        JsonObject peer = doc["peerlist"]["peers"].add<JsonObject>();
        peer["port"] = peerList[i].port;
        peer["call"] = peerList[i].nodeCall;
        peer["timestamp"] = peerList[i].timestamp;
        peer["rssi"] = peerList[i].rssi;
        peer["snr"] = peerList[i].snr;
        peer["frqError"] = peerList[i].frqError;
        peer["available"] = peerList[i].available;
        if (dualPath) {
            peer["preferred"] = peerList[i].available;
        }
    }
    
    size_t jsonLen = measureJson(doc) + 1;
    char* jsonBuffer = (char*)malloc(jsonLen);
    if (jsonBuffer != nullptr) {
        size_t len = serializeJson(doc, jsonBuffer, jsonLen);
        wsBroadcast(jsonBuffer, len);
        free(jsonBuffer);
    } else {
        Serial.println(F("[OOM] sendPeerList: malloc failed"));
    }
}

/**
 * @brief Update the availability flag of a specific (callsign, port) entry.
 *
 * If the flag changed, broadcasts the updated peer list and flags a topology
 * change.  When marking a peer available the inactivity timestamp is refreshed.
 *
 * @param call       Null-terminated callsign to look up.
 * @param available  Desired availability state.
 * @param port       Transport to match (0 = LoRa, 1 = WiFi/UDP).
 */
void availablePeerList(const char* call, bool available, uint8_t port) {
    bool update = false;

    auto it = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) {
        return (strcmp(peer.nodeCall, call) == 0) && (peer.port == port);
    });

    if (it != peerList.end()) {
        // Reject availability for LoRa peers below minimum SNR threshold
        bool effectiveAvailable = available;
        if (available && it->port == 0 && extSettings.minSnr > -20 && it->snr < extSettings.minSnr) {
            effectiveAvailable = false;
        }

        if (it->available != effectiveAvailable) {
            it->available = effectiveAvailable;
            update = true;
        }

        if (effectiveAvailable) {
            it->timestamp = time(NULL);
        }
    }

    if (update) {
        sendPeerList();
        markTopologyChanged();
    }
}

/**
 * @brief Insert or refresh a peer entry from a received frame.
 *
 * Look-up key: (nodeCall, port).
 *  - **Existing entry**: rssi, snr, frqError and timestamp are updated;
 *    availability state is preserved (managed separately via availablePeerList()).
 *  - **New entry**: appended with available = false.
 *
 * After every call the list is sorted by SNR descending (best link first)
 * and a WebSocket broadcast is sent.  A topology change is flagged only for
 * new entries.  Frames from our own callsign are silently ignored.
 *
 * @param f  Received frame; nodeCall, port, rssi, snr, frqError are consumed.
 */
void addPeerList(Frame &f) {
    if (strlen(f.nodeCall) == 0) {
        return;
    }

    if (strcmp(f.nodeCall, settings.mycall) == 0) {
        return;
    }

    time_t now = time(NULL);

    // Search for an existing peer with same callsign and port
    auto it = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) {
        return (strcmp(peer.nodeCall, f.nodeCall) == 0) && (peer.port == f.port);
    });

    bool isNew = (it == peerList.end());

    if (!isNew) {
        // Update existing peer, but keep current availability state
        it->timestamp = now;
        it->rssi = f.rssi;
        it->snr = f.snr;
        it->frqError = f.frqError;
        it->port = f.port;
    } else {
        // Add new peer (enforce capacity limit)
        if (peerList.size() >= PEER_LIST_SIZE) {
            Serial.printf("[Peer] List full (%d), ignoring new peer %s\n", PEER_LIST_SIZE, f.nodeCall);
            return;
        }
        Peer p;
        memcpy(p.nodeCall, f.nodeCall, sizeof(p.nodeCall));
        p.nodeCall[sizeof(p.nodeCall) - 1] = '\0';
        p.timestamp = now;
        p.rssi = f.rssi;
        p.snr = f.snr;
        p.frqError = f.frqError;
        p.port = f.port;
        p.available = false;
        peerList.push_back(p);

        Serial.printf("[Reporting] New peer: %s (Port %d)\n", f.nodeCall, f.port);
    }

    // Sort by SNR descending
    std::sort(peerList.begin(), peerList.end(), [](const Peer& a, const Peer& b) {
        return a.snr > b.snr;
    });

    sendPeerList();

    if (isNew) {
        markTopologyChanged();
    }
}