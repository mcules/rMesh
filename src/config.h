#pragma once

#include <vector>
#include <algorithm>

/**
 * @file config.h
 * @brief Central compile-time configuration for rMesh.
 *
 * Contains frequency-band detection helpers, timing constants, and
 * buffer-size limits.  Include version.h for the firmware version string.
 */

// ── Frequency Band Detection ──────────────────────────────────────────────────

/** LoRa SyncWord for the amateur 70-cm band (430–440 MHz). */
#define AMATEUR_SYNCWORD       0x2B

/**
 * LoRa SyncWord for the public SRD 869 sub-band P (869.4–869.65 MHz).
 * Permitted TX power: up to 500 mW, 10 % duty cycle.
 */
#define PUBLIC_SYNCWORD        0x12

/** Maximum TX power (dBm) allowed in SRD 869 sub-band P (500 mW). */
#define PUBLIC_MAX_TX_POWER    27

/**
 * @brief Returns true when a LoRa frequency has been configured
 *        (i.e. the stored value is greater than 1 MHz).
 */
inline bool loraConfigured(float f) { return f > 1.0f; }

/**
 * @brief Returns true when @p f falls inside the amateur 70-cm band
 *        (430–440 MHz).
 */
inline bool isAmateurBand(float f)  { return f >= 430.0f && f <= 440.0f; }

/**
 * @brief Returns true when @p f falls inside the public SRD 869 sub-band P
 *        (869.4–869.65 MHz).
 */
inline bool isPublicBand(float f)   { return f >= 869.4f && f <= 869.65f; }

/**
 * @brief Returns the correct LoRa SyncWord for the configured frequency.
 *
 * PUBLIC_SYNCWORD is returned for frequencies in SRD 869 sub-band P;
 * AMATEUR_SYNCWORD is returned for the 70-cm band and as a fallback for
 * any unrecognised frequency.
 *
 * @param f  Configured LoRa frequency in MHz.
 * @return   SyncWord byte to pass to the LoRa driver.
 */
inline uint8_t syncWordForFrequency(float f) {
    if (isPublicBand(f))  return PUBLIC_SYNCWORD;
    if (isAmateurBand(f)) return AMATEUR_SYNCWORD;
    return AMATEUR_SYNCWORD;  // fallback
}

// ── Timing ────────────────────────────────────────────────────────────────────

/**
 * Interval between ANNOUNCE beacon transmissions (ms, for millis()).
 * Base: 10 minutes + a random jitter of 0–2 minutes to avoid collisions.
 */
#define ANNOUNCE_TIME (10 * 60 * 1000 + random(0, 2 * 60 * 1000))

/**
 * Inactivity timeout before a peer is marked as unavailable (s, for time()).
 * Default: 25 minutes without a response.
 */
#define PEER_INACTIVE_TIMEOUT (25 * 60)

/**
 * Inactivity timeout before a peer is removed from the peer list (s, for time()).
 * Default: 60 minutes without a response.
 */
#define PEER_TIMEOUT (60 * 60)

/** Number of TX retries before a frame is considered undeliverable. */
#define TX_RETRY 10

/** Maximum number of messages persisted in messages.json (flash). */
#define MAX_STORED_MESSAGES 1000

/** Maximum number of messages held in the RAM message cache. */
#define MAX_STORED_MESSAGES_RAM 100

/** Maximum number of ACK frames stored in ack.json. */
#define MAX_STORED_ACK 100

// ── UDP Timing ────────────────────────────────────────────────────────────────

/** Retry interval for UDP transmissions (ms). */
#define UDP_TX_RETRY_TIME 2000

// ── Internal Constants ────────────────────────────────────────────────────────

/** Human-readable product name, used in version strings and the web UI. */
#define NAME "rMesh"

#include "version.h"

/** Maximum length of a callsign string (1–15 characters). */
#define MAX_CALLSIGN_LENGTH 9

/** Size of the outgoing TX frame buffer (number of frames). */
#define TX_BUFFER_SIZE 50

/** Maximum number of peers tracked simultaneously. */
#define PEER_LIST_SIZE 20

/** Capacity of the routing table (number of callsign entries). */
#define ROUTING_BUFFER_SIZE 1000

/**
 * Grace period (s) for flash-restored peers before they are marked unavailable.
 * After a reboot, loaded peers start as available but are given only this much
 * time (instead of the full PEER_INACTIVE_TIMEOUT) to prove they are still alive.
 */
#define PEER_INITIAL_TIMEOUT 120

/** Interval (ms) between periodic flash saves of dirty routes/peers. */
#define PERSIST_INTERVAL (5 * 60 * 1000)

/** UDP port used for all rMesh network communication. */
#define UDP_PORT 3333
