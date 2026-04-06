#pragma once

#ifdef HAS_WIFI

#include <ESPAsyncWebServer.h>
#include "frame.h"
#include "settings.h"

// Ring buffer sizes (emergency values: ACK-based cleanup keeps buffers near-empty)
#define API_MSG_BUFFER_SIZE  5
#define API_EVT_BUFFER_SIZE  10

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

// Event ring buffer entry (~80 bytes each, 100 * 80 = ~8 KB)
struct ApiEvent {
    uint32_t time;
    char event[8];        // "rx", "tx", "ack", "routing", "error"
    uint8_t frameType;
    char nodeCall[MAX_CALLSIGN_LENGTH + 1];
    char viaCall[MAX_CALLSIGN_LENGTH + 1];
    char srcCall[MAX_CALLSIGN_LENGTH + 1];
    uint32_t id;
    int16_t rssi;
    int8_t snr;
    uint8_t port;
    // routing-specific
    char action[8];       // "new", "update"
    char dest[MAX_CALLSIGN_LENGTH + 1];
    uint8_t hops;
    // error-specific
    char source[12];
    char text[64];
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
 * @brief Record an event (rx, tx, ack, routing, error) into the event ring buffer.
 * Call from processRxFrame(), TX path, routing updates, and error handlers.
 */
void apiRecordRxEvent(const Frame &f);
void apiRecordTxEvent(const Frame &f);
void apiRecordAckEvent(const Frame &f);
void apiRecordRoutingEvent(const char* action, const char* dest, const char* via, uint8_t hops);
void apiRecordErrorEvent(const char* source, const char* text);

/**
 * @brief Mark a message as ACKed in the ring buffer.
 */
void apiMarkMessageAcked(const char* srcCall, uint32_t id);

#endif
