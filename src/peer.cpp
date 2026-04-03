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
#include "serial.h"
#include "persistence.h"
#include "routing.h"
#include "logging.h"

/** Runtime peer table; extern-declared in peer.h. */
std::vector<Peer> peerList;

/** Deferred-broadcast flag for RSSI/SNR updates (see peer.h). */
bool peerListDirty = false;

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

    // Time jump after NTP sync: correct peer timestamps from the pre-NTP phase
    // (near 0) to now, instead of letting them expire immediately.
    // Without NTP, time(NULL) on ESP32 returns values near 0 (seconds since boot).
    // After NTP sync, the value jumps to real Unix time (>1e9).
    const time_t NTP_PLAUSIBLE = 1000000000;  // Sep 2001 – any NTP sync yields more
    if (now > NTP_PLAUSIBLE) {
        for (auto& peer : peerList) {
            if (peer.timestamp < NTP_PLAUSIBLE) {
                peer.timestamp = now - (PEER_INACTIVE_TIMEOUT - PEER_INITIAL_TIMEOUT);
                update = true;
            }
        }
        // Also correct route timestamps so entries from the
        // pre-NTP phase (timestamp ≈ 0) are not immediately evicted as "oldest".
        for (auto& route : routingList) {
            if (route.timestamp < NTP_PLAUSIBLE) {
                route.timestamp = now;
            }
        }
    }

    // Don't evaluate timeouts before NTP sync (time base unreliable)
    if (now < NTP_PLAUSIBLE) {
        if (update) { sendPeerList(); markTopologyChanged(); }
        return;
    }

    // Mark peers as unavailable after inactivity timeout
    for (auto& peer : peerList) {
        if (peer.available && (now - peer.timestamp) > PEER_INACTIVE_TIMEOUT) {
            peer.available = false;
            update = true;
            logPrintf(LOG_DEBUG, "Peer", "{\"event\":\"peer\",\"action\":\"unavailable\",\"call\":\"%s\",\"port\":%d,\"reason\":\"inactivity\"}", peer.nodeCall, peer.port);
        }
    }

    // Remove peers after full timeout
    for (auto it = peerList.begin(); it != peerList.end();) {
        if ((now - it->timestamp) > PEER_TIMEOUT) {
            logPrintf(LOG_DEBUG, "Peer", "{\"event\":\"peer\",\"action\":\"removed\",\"call\":\"%s\",\"port\":%d,\"reason\":\"timeout\"}", it->nodeCall, it->port);
            it = peerList.erase(it);
            update = true;
            peersDirty = true;
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
                logPrintf(LOG_DEBUG, "Peer", "{\"event\":\"peer\",\"action\":\"unavailable\",\"call\":\"%s\",\"port\":%d,\"reason\":\"snr\",\"snr\":%.1f,\"min_snr\":%d}",
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
    // Build JSON with snprintf instead of JsonDocument to avoid heap fragmentation.
    // Each peer entry is ~130 bytes; use tight malloc based on actual peer count.
    size_t bufSize = 40 + peerList.size() * 150;
    if (bufSize < 256) bufSize = 256;
    char* jsonBuffer = (char*)malloc(bufSize);
    if (jsonBuffer == nullptr) {
        logPrintf(LOG_WARN, "Peer", "sendPeerList: malloc failed");
        return;
    }
    size_t pos = 0;

    pos += snprintf(jsonBuffer + pos, bufSize - pos, "{\"peerlist\":{\"peers\":[");

    for (size_t i = 0; i < peerList.size() && pos < bufSize - 200; i++) {
        // Check if same callsign exists on a different port (dual-path node)
        bool dualPath = false;
        for (size_t j = 0; j < peerList.size(); j++) {
            if (i != j && strcmp(peerList[i].nodeCall, peerList[j].nodeCall) == 0
                       && peerList[i].port != peerList[j].port) {
                dualPath = true;
                break;
            }
        }

        if (i > 0) jsonBuffer[pos++] = ',';
        pos += snprintf(jsonBuffer + pos, bufSize - pos,
            "{\"port\":%u,\"call\":\"%s\",\"timestamp\":%ld,\"rssi\":%.1f,"
            "\"snr\":%.1f,\"frqError\":%.1f,\"available\":%s",
            peerList[i].port, peerList[i].nodeCall,
            (long)peerList[i].timestamp, peerList[i].rssi,
            peerList[i].snr, peerList[i].frqError,
            peerList[i].available ? "true" : "false");
        if (dualPath) {
            pos += snprintf(jsonBuffer + pos, bufSize - pos,
                ",\"preferred\":%s", peerList[i].available ? "true" : "false");
        }
        jsonBuffer[pos++] = '}';
    }

    pos += snprintf(jsonBuffer + pos, bufSize - pos, "]}}");
    if (pos < bufSize) {
        wsBroadcast(jsonBuffer, pos);
    }
    free(jsonBuffer);
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

        // Reject availability while cooldown is active (retry exhaustion)
        // Use overflow-safe comparison: (int32_t)(now - deadline) >= 0 means expired
        if (effectiveAvailable && it->cooldownUntil != 0 && (int32_t)(millis() - it->cooldownUntil) < 0) {
            effectiveAvailable = false;
        }
        // Clear expired cooldown
        if (it->cooldownUntil != 0 && (int32_t)(millis() - it->cooldownUntil) >= 0) {
            it->cooldownUntil = 0;
        }

        if (!available) {
            // When explicitly marking unavailable, clear cooldown
            // (the caller may set a new one afterwards)
            it->cooldownUntil = 0;
        }

        if (it->available != effectiveAvailable) {
            it->available = effectiveAvailable;
            update = true;
        }

        if (effectiveAvailable) {
            it->timestamp = time(NULL);
        }
    }

    if (update && serialDebug) {
        JsonDocument dbgPeer;
        dbgPeer["event"] = "peer";
        dbgPeer["action"] = "available";
        dbgPeer["call"] = call;
        dbgPeer["available"] = (it != peerList.end()) ? it->available : false;
        dbgPeer["port"] = port;
        logJson(dbgPeer);
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
        logPrintf(LOG_DEBUG, "Peer", "{\"event\":\"peer\",\"action\":\"ignore\",\"call\":\"%s\",\"reason\":\"own_call\"}", f.nodeCall);
        return;
    }

    time_t now = time(NULL);
    // Search for an existing peer with same callsign and port
    auto it = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) {
        return (strcmp(peer.nodeCall, f.nodeCall) == 0) && (peer.port == f.port);
    });

    bool isNew = (it == peerList.end());
    logPrintf(LOG_DEBUG, "Peer", "{\"event\":\"peer\",\"action\":\"lookup\",\"call\":\"%s\",\"port\":%d,\"isNew\":%s,\"listSize\":%d}",
        f.nodeCall, f.port, isNew ? "true" : "false", (int)peerList.size());

    if (!isNew) {
        // Update existing peer, but keep current availability state
        it->timestamp = now;
        // Only overwrite if new values are present (RSSI is always negative for LoRa)
        if (f.rssi != 0 || f.snr != 0 || f.frqError != 0) {
            it->rssi = f.rssi;
            it->snr = f.snr;
            it->frqError = f.frqError;
        }
        it->port = f.port;
    } else {
        // Add new peer (enforce capacity limit)
        if (peerList.size() >= PEER_LIST_SIZE) {
            logPrintf(LOG_WARN, "Peer", "List full (%d), ignoring new peer %s", PEER_LIST_SIZE, f.nodeCall);
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
        peersDirty = true;

        if (serialDebug) {
            JsonDocument dbgPeer;
            dbgPeer["event"] = "peer";
            dbgPeer["action"] = "add";
            dbgPeer["call"] = f.nodeCall;
            dbgPeer["port"] = f.port;
            dbgPeer["rssi"] = f.rssi;
            dbgPeer["snr"] = f.snr;
            logJson(dbgPeer);
        }
    }

    // Sort by SNR descending
    std::sort(peerList.begin(), peerList.end(), [](const Peer& a, const Peer& b) {
        return a.snr > b.snr;
    });

    if (isNew) {
        // New peer: broadcast immediately and flag topology change
        sendPeerList();
        markTopologyChanged();
    } else {
        // Existing peer with updated RSSI/SNR: defer broadcast to main-loop timer
        peerListDirty = true;
    }
}