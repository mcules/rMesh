#pragma once

#ifdef HAS_WIFI

#include <ESPAsyncWebServer.h>
#include "mesh/frame.h"
#include "hal/settings.h"

// Ring buffer sizes — buffers are persisted to LittleFS so multiple REST
// clients can each fetch the full live tail without destructive ACK-purging.
#define API_MSG_BUFFER_SIZE  32
#define API_EVT_BUFFER_SIZE  64

// Message ring buffer entry (~210 bytes each, 5 * 210 = ~1050 bytes)
struct ApiMessage {
    uint32_t id;
    uint32_t time;
    char src[MAX_CALLSIGN_LENGTH + 1];
    char group[MAX_GROUP_NAME_LEN];
    char text[160];
    char via[MAX_CALLSIGN_LENGTH + 1];
    uint8_t hops;
    int16_t rssi;
    int8_t snr;
    uint8_t dir;      // 0=rx, 1=tx
    bool acked;
};

// Event type discriminator (replaces former char event[8] field).
enum ApiEventType : uint8_t {
    API_EVT_RX  = 0,
    API_EVT_TX  = 1,
    API_EVT_ACK = 2,
};

// Event ring buffer entry. ~40 bytes each.
struct ApiEvent {
    uint32_t time;
    uint8_t  eventType;   // ApiEventType
    uint8_t  frameType;
    char nodeCall[MAX_CALLSIGN_LENGTH + 1];
    char viaCall[MAX_CALLSIGN_LENGTH + 1];
    char srcCall[MAX_CALLSIGN_LENGTH + 1];
    uint32_t id;
    int16_t rssi;
    int8_t  snr;
    uint8_t port;
};

// LoRa frame counters (always active, independent of serialDebug)
extern uint32_t apiTxTotal;
extern uint32_t apiRxTotal;

/**
 * @brief Register all /api/* endpoints on the given web server.
 */
void setupApiEndpoints(AsyncWebServer &server);

/**
 * @brief Record a received or sent message into the API ring buffer.
 * Call from processRxFrame() for RX and from sendGroup()/POST handler for TX.
 */
void apiRecordMessage(const Frame &f, bool isTx);

/**
 * @brief Record an event (rx, tx, ack) into the event ring buffer.
 * Call from processRxFrame() and the TX path.
 */
void apiRecordRxEvent(const Frame &f);
void apiRecordTxEvent(const Frame &f);
void apiRecordAckEvent(const Frame &f);

/**
 * @brief Mark a message as ACKed in the ring buffer.
 */
void apiMarkMessageAcked(const char* srcCall, uint32_t id);

/**
 * @brief Load persisted message/event ring buffers from LittleFS.
 * Call once in setup() after LittleFS is mounted.
 */
void apiLoadBuffers();

/**
 * @brief Persist the message/event ring buffers to LittleFS via bgWorker.
 * Triggered from the main loop's periodic persistence tick when dirty.
 */
void apiSaveBuffers();

/** Dirty flag — set when buffers have changed since last save. */
extern volatile bool apiBuffersDirty;

#endif
