#pragma once

/**
 * @file peer.h
 * @brief Peer management: data structure and public API.
 *
 * A *peer* is any remote rMesh node that has been heard at least once.
 * Each peer is tracked per transport (LoRa = port 0, UDP/WiFi = port 1),
 * so the same callsign can appear twice in peerList if it is reachable via
 * both transports simultaneously.
 */

#include "mesh/frame.h"

/**
 * @brief Describes one remote rMesh node on a specific transport.
 */
struct Peer {
    /** Null-terminated callsign of the remote node. */
    char nodeCall[MAX_CALLSIGN_LENGTH + 1] = {0};

    /** Unix timestamp of the last received frame from this peer. */
    time_t timestamp = 0;

    /** Last measured RSSI in dBm (LoRa only; 0 for UDP peers). */
    float rssi = 0;

    /** Last measured SNR in dB (LoRa only; 0 for UDP peers). */
    float snr = 0;

    /** Last measured frequency error in Hz (LoRa only; 0 for UDP peers). */
    float frqError = 0;

    /**
     * @brief Whether the peer is currently considered reachable.
     *
     * Set to true by availablePeerList() when an ACK is received;
     * set to false after PEER_INACTIVE_TIMEOUT seconds of silence
     * or after all TX retries are exhausted.
     */
    bool available = 0;

    /**
     * @brief Transport this entry refers to.
     * - 0 = LoRa
     * - 1 = UDP / WiFi
     */
    uint8_t port = 0;

    /**
     * @brief millis() timestamp until this peer is blocked from becoming available.
     *
     * Set after all TX retries exhaust (peer unreachable).  While
     * millis() < cooldownUntil, availablePeerList(…, true, …) is rejected
     * so that the next ANNOUNCE_ACK cycle cannot immediately re-enable
     * a peer that just failed 10 retries.
     */
    uint32_t cooldownUntil = 0;
};

/**
 * @brief Check all peers for inactivity and enforce WiFi-over-LoRa preference.
 *
 * Called once per second from the main loop.  Actions taken:
 *  - Marks peers as unavailable after PEER_INACTIVE_TIMEOUT seconds of silence.
 *  - Removes peers that have exceeded PEER_TIMEOUT seconds of silence.
 *  - When the same callsign is active on both transports, the LoRa entry is
 *    suppressed in favour of the WiFi entry.  If both are on the same transport,
 *    the entry with the lower SNR is suppressed.
 * Calls sendPeerList() and markTopologyChanged() if any state changed.
 */
void checkPeerList();

/**
 * @brief Update the availability flag of a specific peer entry.
 *
 * Looks up the peer by callsign + port.  If found and the flag changed,
 * refreshes the timestamp (when marking available) and broadcasts the
 * updated peer list via WebSocket.
 *
 * @param call       Null-terminated callsign to look up.
 * @param available  New availability state.
 * @param port       Transport to match (0 = LoRa, 1 = UDP/WiFi).
 */
void availablePeerList(const char* call, bool available, uint8_t port);

/**
 * @brief Insert or update a peer entry from a received frame.
 *
 * If a peer with the same callsign + port already exists, its signal metrics
 * and timestamp are refreshed.  Otherwise a new entry is appended.
 * After any change the list is re-sorted by SNR (descending) and broadcast.
 * Own callsign frames are silently ignored.
 *
 * @param f  The received frame carrying nodeCall, port, rssi, snr, frqError.
 */
void addPeerList(Frame &f);

/**
 * @brief Serialize the current peer list to JSON and broadcast it via WebSocket.
 *
 * Output format:
 * @code
 * { "peerlist": { "peers": [ { "port", "call", "timestamp", "rssi", "snr",
 *                               "frqError", "available" }, … ] } }
 * @endcode
 */
void sendPeerList();

/** Global peer list; populated and maintained by the functions above. */
extern std::vector<Peer> peerList;

/**
 * @brief Flag set by addPeerList() when RSSI/SNR changed but a full
 *        WebSocket broadcast was deferred.  The main loop clears this
 *        once per second by calling sendPeerList().
 */
extern bool peerListDirty;
