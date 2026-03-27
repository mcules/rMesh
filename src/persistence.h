#pragma once

/**
 * @file persistence.h
 * @brief Flash persistence for the routing table and peer list.
 *
 * Stores routes and peers as compact binary files on LittleFS so that the
 * mesh network is immediately operational after a reboot without waiting
 * for full route/peer discovery.
 *
 * Files:
 *  - /routes.bin — routing table (up to ROUTING_BUFFER_SIZE entries)
 *  - /peers.bin  — peer list (up to PEER_LIST_SIZE entries)
 */

#include <cstdint>

/** Dirty flag — set by routing.cpp when routes are added/changed/removed. */
extern volatile bool routesDirty;

/** Dirty flag — set by peer.cpp when peers are added or removed. */
extern volatile bool peersDirty;

/**
 * @brief Load the routing table from /routes.bin into routingList.
 *
 * Called once during setup(), after LittleFS is mounted.
 * Silently returns an empty list if the file is missing or corrupt.
 */
void loadRoutes();

/**
 * @brief Save the current routingList to /routes.bin.
 *
 * Runs as a FreeRTOS task with fsMutex protection.
 * Clears routesDirty on success.
 */
void saveRoutes();

/**
 * @brief Load the peer list from /peers.bin into peerList.
 *
 * Called once during setup(), after LittleFS is mounted.
 * Loaded peers are marked available with a shortened inactivity grace
 * period (PEER_INITIAL_TIMEOUT) so they are quickly pruned if unreachable.
 */
void loadPeers();

/**
 * @brief Save the current peerList to /peers.bin.
 *
 * Only persists callsign, port, timestamp, and available flag.
 * RSSI/SNR/frqError are omitted (stale after reboot).
 * Runs as a FreeRTOS task with fsMutex protection.
 * Clears peersDirty on success.
 */
void savePeers();
