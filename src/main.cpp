/**
 * @file main.cpp
 * @brief Arduino entry point and main application logic for rMesh.
 *
 * Responsibilities:
 *  - setup(): hardware initialisation, filesystem mount, settings load,
 *    WiFi/web-server start, NTP configuration.
 *  - loop(): periodic announce beacons, TX-buffer draining (LoRa + UDP),
 *    RX frame processing, WebSocket status broadcasts, OTA update checks,
 *    topology reporting, and scheduled housekeeping.
 *  - processRxFrame(): central dispatch for all received frames
 *    (ANNOUNCE, ANNOUNCE_ACK, MESSAGE_ACK, MESSAGE).
 */

#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h>

#ifdef NRF52_PLATFORM
#include "platform_nrf52.h"
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#else
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include <nvs_flash.h>
#endif

#include "config.h"
#include "hal.h"
#include "frame.h"
#include "settings.h"
#include "main.h"
#include "wifiFunctions.h"
#include "webFunctions.h"
#include "serial.h"
#include "helperFunctions.h"
#include "peer.h"
#include "ack.h"
#include "udp.h"
#include "routing.h"
#include "reporting.h"
#include "dutycycle.h"
#include "persistence.h"
#include "time.h"

#ifdef LILYGO_T_LORA_PAGER
#include "display_LILYGO_T-LoraPager.h"
#include "hal_LILYGO_T-LoraPager.h"
#endif

#ifdef SEEED_SENSECAP_INDICATOR
#include "display_SEEED_SenseCAP_Indicator.h"
#include "hal_SEEED_SenseCAP_Indicator.h"
#endif

#ifdef HELTEC_WIFI_LORA_32_V3
#include "display_HELTEC_WiFi_LoRa_32_V3.h"
#endif
#ifdef HELTEC_HT_TRACKER_V1_2
#include "display_HELTEC_HT-Tracker_V1_2.h"
#endif
#ifdef LILYGO_T3_LORA32_V1_6_1
#include "display_LILYGO_T3_LoRa32_V1_6_1.h"
#endif
#ifdef LILYGO_T_BEAM
#include "display_LILYGO_T-Beam.h"
#endif
#ifdef LILYGO_T_ECHO
#include "display_LILYGO_T-Echo.h"
#endif


// ── Global state ──────────────────────────────────────────────────────────────

/** POSIX timezone rule string for CET/CEST (Central Europe). */
const char* TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3";

#ifndef NRF52_PLATFORM
/** Human-readable string for the last ESP32 reset reason. */
static const char* lastResetReason = "unknown";

static const char* getResetReasonStr() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:  return "power-on";
        case ESP_RST_EXT:      return "external";
        case ESP_RST_SW:       return "software";
        case ESP_RST_PANIC:    return "panic/crash";
        case ESP_RST_INT_WDT:  return "interrupt-watchdog";
        case ESP_RST_TASK_WDT: return "task-watchdog";
        case ESP_RST_WDT:      return "other-watchdog";
        case ESP_RST_DEEPSLEEP:return "deep-sleep";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_SDIO:     return "SDIO";
        default:               return "unknown";
    }
}
#endif

/** Outgoing frame queue (see main.h for details). */
std::vector<Frame> txBuffer;

/** In-RAM message deduplication ring-buffer (see main.h for details). */
MSG messages[MAX_STORED_MESSAGES_RAM];
uint16_t messagesHead = 0;

/** Mutex protecting all LittleFS accesses. */
SemaphoreHandle_t fsMutex = NULL;

// ── Timing ────────────────────────────────────────────────────────────────────

/**
 * Overflow-safe timer check: returns true when the deadline has been reached.
 * Works correctly across the millis() 32-bit wrap (~49 days) for intervals
 * up to ~24.8 days.
 */
static inline bool timerExpired(uint32_t deadline) {
    return (int32_t)(millis() - deadline) >= 0;
}

/** First ANNOUNCE is sent 5 s after boot to give WiFi time to connect. */
uint32_t announceTimer = 5000;

/** Deadline for the next 1-second WebSocket status push. */
uint32_t statusTimer = 0;

/** Deadline for the next periodic flash save of dirty routes/peers. */
static uint32_t persistTimer = PERSIST_INTERVAL;

/** millis() value at which ESP.restart() is called; 0 = disabled. */
uint32_t rebootTimer = 0;
bool rebootRequested = false;

/** Retry counter for the frame currently being transmitted from txBuffer. */
uint8_t currentRetry = 0;

/** Flux guard: earliest millis() at which the next LoRa TX is allowed.
 *  A short pause after each TX lets remote receivers settle back into RX,
 *  improving effective range. */
uint32_t loraFluxGuard = 0;

/** Deferred flags set by the web UI and consumed in loop(). */
bool pendingManualUpdate = false;
bool pendingShutdown     = false;
bool pendingForceUpdate  = false;
uint8_t pendingForceChannel = 0;

/** First OTA update check fires 1 hour after boot. */
uint32_t updateCheckTimer = 60 * 60 * 1000;

/** First messages.json trim fires 30 minutes after boot. */
uint32_t messagesDeleteTimer = 30 * 60 * 1000;

/** Filesystem low-space flag — set by addJSONtoFileTask, consumed by main loop. */
volatile bool trimNeeded = false;

/** First topology report fires 5 minutes after boot. */
uint32_t reportingTimer = 5 * 60 * 1000;


// ── Frame processing ──────────────────────────────────────────────────────────

/**
 * @brief Dispatch a received frame to the appropriate handler.
 *
 * Called for both LoRa and UDP frames after successful reception.  The function:
 *  1. Rejects frames without a nodeCall.
 *  2. Feeds the frame to the monitor (JSON log) and the peer list.
 *  3. Dispatches on frameType:
 *
 *  | Frame type        | Action                                                  |
 *  |-------------------|---------------------------------------------------------|
 *  | ANNOUNCE_FRAME    | Enqueue an ANNOUNCE_ACK_FRAME towards the sender.       |
 *  |                   | WiFi peers suppress a duplicate LoRa ACK.               |
 *  | ANNOUNCE_ACK_FRAME| Mark the peer as available; update the routing table.   |
 *  | MESSAGE_ACK_FRAME | Remove the matching frame from txBuffer; record the ACK.|
 *  | MESSAGE_FRAME     | Deduplicate; send ACK; store + forward new messages;    |
 *  |                   | handle TRACE echo and remote COMMAND frames.            |
 *
 * @param f  Reference to the received frame (may be modified for ACK replies).
 */
void processRxFrame(Frame &f) {
    // Ignore frames without a sender callsign
    if (strlen(f.nodeCall) == 0) {return;}

    // Log to monitor
    f.monitorJSON();

    // Early duplicate drop for MESSAGE_FRAME: if we already have this (srcCall, id)
    // in the ring-buffer AND the frame is not directly addressed to us, skip the
    // expensive JSON debug serialization, addPeerList, and full case logic.
    // Directly-addressed duplicates (viaCall == mycall) are NOT dropped so that
    // re-ACKs still work for retransmitting peers.
    if (f.frameType == Frame::FrameTypes::MESSAGE_FRAME &&
        strcmp(f.viaCall, settings.mycall) != 0) {
        for (int i = 0; i < MAX_STORED_MESSAGES_RAM; i++) {
            if (messages[i].id == f.id &&
                strcmp(messages[i].srcCall, f.srcCall) == 0) {
                // Duplicate: remove any pending relay copies from TX buffer
                txBuffer.erase(
                    std::remove_if(txBuffer.begin(), txBuffer.end(),
                        [&](const Frame& txB) {
                            return (strcmp(txB.srcCall, f.srcCall) == 0) &&
                                   (txB.id == f.id) &&
                                   (txB.frameType == Frame::FrameTypes::MESSAGE_FRAME);
                        }),
                    txBuffer.end()
                );
                return;
            }
        }
    }

    // Debug output for test framework
    if (serialDebug) {
        JsonDocument dbg;
        dbg["event"] = "rx";
        dbg["frameType"] = f.frameType;
        if (strlen(f.srcCall) > 0) dbg["srcCall"] = f.srcCall;
        if (strlen(f.nodeCall) > 0) dbg["nodeCall"] = f.nodeCall;
        if (strlen(f.dstCall) > 0) dbg["dstCall"] = f.dstCall;
        if (strlen(f.dstGroup) > 0) dbg["dstGroup"] = f.dstGroup;
        if (strlen(f.viaCall) > 0) dbg["viaCall"] = f.viaCall;
        dbg["id"] = f.id;
        dbg["hopCount"] = f.hopCount;
        dbg["messageType"] = f.messageType;
        if (f.messageLength > 0 && (f.messageType == Frame::MessageTypes::TEXT_MESSAGE || f.messageType == Frame::MessageTypes::TRACE_MESSAGE)) {
            char text[261] = {0};
            memcpy(text, f.message, f.messageLength);
            dbg["text"] = text;
        }
        dbg["rssi"] = f.rssi;
        dbg["snr"] = f.snr;
        dbg["port"] = f.port;
        Serial.print("DBG:");
        serializeJson(dbg, Serial);
        Serial.println();
    }

    // Update peer list with signal quality data from this frame
    addPeerList(f);

    Frame tf;       // Reply frame built during processing
    bool found = false;
    switch (f.frameType) {

        // ── ANNOUNCE_FRAME ────────────────────────────────────────────────────
        // A remote node is announcing itself; reply with ANNOUNCE_ACK.
        case Frame::FrameTypes::ANNOUNCE_FRAME:
            if (strlen(f.nodeCall) > 0 ){
                // Prefer WiFi: suppress LoRa ACK if peer is already known via WiFi
                if (f.port == 0) {
                    bool peerOnWifi = false;
                    for (size_t pi = 0; pi < peerList.size(); pi++) {
                        if (strcmp(f.nodeCall, peerList[pi].nodeCall) == 0 && peerList[pi].port == 1) {
                            peerOnWifi = true; break;
                        }
                    }
                    if (peerOnWifi) break;
                }
                tf.frameType = Frame::FrameTypes::ANNOUNCE_ACK_FRAME;
                tf.port = f.port;
                // Schedule reply: add Time-on-Air jitter for LoRa, send immediately over UDP
                switch (tf.port){
                    case 0: tf.transmitMillis = millis() + calculateAckTime(); break;
                    case 1: tf.transmitMillis = millis(); break;
                }
                memcpy(tf.viaCall, f.nodeCall, sizeof(tf.viaCall));
                txBuffer.push_back(tf);
            }
            break;

        // ── ANNOUNCE_ACK_FRAME ────────────────────────────────────────────────
        // Remote node confirmed our announce; mark it available and update routing.
        case Frame::FrameTypes::ANNOUNCE_ACK_FRAME:
            if (strcmp(f.viaCall, settings.mycall) == 0) {
                // Direct ACK to us: mark peer available, add 0-hop route
                availablePeerList(f.nodeCall, true, f.port);
                addRoutingList(f.nodeCall, f.nodeCall, f.hopCount);
            } else if (strlen(f.viaCall) > 0) {
                // Overheard ACK: nodeCall confirmed viaCall as its peer.
                // Learn that viaCall is reachable through nodeCall (1 extra hop).
                addRoutingList(f.viaCall, f.nodeCall, f.hopCount + 1);
            }
            break;

        // ── MESSAGE_ACK_FRAME ─────────────────────────────────────────────────
        // The destination confirmed receipt of a message we forwarded.
        case Frame::FrameTypes::MESSAGE_ACK_FRAME:
            // Mark the peer as available and update routing
            if (strcmp(f.viaCall, settings.mycall) == 0) {
                availablePeerList(f.nodeCall, true, f.port);
                addRoutingList(f.nodeCall, f.nodeCall, f.hopCount);
                // Record direct ACK for this node
                addACK(f.srcCall, settings.mycall, f.id);
            }

            // Remove all pending retries for this (viaCall, id) pair from txBuffer
            txBuffer.erase(
                std::remove_if(txBuffer.begin(), txBuffer.end(),
                    [&](const Frame& txB) {
                        return (strcmp(txB.viaCall, f.nodeCall) == 0) && (txB.id == f.id);
                    }),
                txBuffer.end()
            );

            // Persist the ACK so repeat logic and foreign-ACK forwarding work correctly
            addACK(f.srcCall, f.nodeCall, f.id);

            // Debug output for test framework
            if (serialDebug) {
                JsonDocument dbgAck;
                dbgAck["event"] = "ack";
                dbgAck["srcCall"] = f.srcCall;
                dbgAck["nodeCall"] = f.nodeCall;
                dbgAck["id"] = f.id;
                Serial.print("DBG:");
                serializeJson(dbgAck, Serial);
                Serial.println();
            }
            break;

        // ── MESSAGE_FRAME ─────────────────────────────────────────────────────
        // A data message arrived; ACK it, deduplicate, store, and optionally repeat.
        case Frame::FrameTypes::MESSAGE_FRAME:
            // Mark direct sender as available
            if (strcmp(f.viaCall, settings.mycall) == 0) {
                availablePeerList(f.nodeCall, true, f.port);
            }

            // Remove our own pending copy of this message if another node already
            // relayed it (same srcCall + viaCall + id, still in first-try state)
            txBuffer.erase(
                std::remove_if(txBuffer.begin(), txBuffer.end(),
                    [&](const Frame& txB) {
                        return (strcmp(txB.srcCall, f.srcCall) == 0) && (strcmp(txB.viaCall, f.viaCall) == 0) && (txB.id == f.id) && (txB.initRetry == TX_RETRY);
                    }),
                txBuffer.end()
            );

            // Remove our pending copy if a different node is already repeating it
            txBuffer.erase(
                std::remove_if(txBuffer.begin(), txBuffer.end(),
                    [&](const Frame& txB) {
                        return (strcmp(txB.srcCall, f.srcCall) == 0) && (strcmp(txB.viaCall, f.nodeCall) == 0) && (txB.id == f.id) && (txB.initRetry == TX_RETRY);
                    }),
                txBuffer.end()
            );

            // Remove any stale ACK frames for this message from txBuffer
            txBuffer.erase(
                std::remove_if(txBuffer.begin(), txBuffer.end(),
                    [&](const Frame& txB) {
                        return (strcmp(txB.srcCall, f.srcCall) == 0) && (txB.id == f.id) && (txB.frameType == Frame::FrameTypes::MESSAGE_ACK_FRAME);
                    }),
                txBuffer.end()
            );

            // Decide whether to send an ACK:
            //  - Always ACK when addressed directly (viaCall == mycall)
            //  - ACK once for overheard messages not yet acknowledged by us
            bool sendACK = false;
            if (strcmp(f.viaCall, settings.mycall) == 0) {sendACK = true;}
            if ((strlen(f.viaCall) > 0) && (checkACK(f.srcCall, f.nodeCall, f.id) == false) && (checkACK(f.srcCall, settings.mycall, f.id) == false)) {sendACK = true;}

            if (sendACK) {
                addACK(f.srcCall, f.nodeCall, f.id);
                tf.frameType = Frame::FrameTypes::MESSAGE_ACK_FRAME;
                memcpy(tf.viaCall, f.nodeCall, sizeof(tf.viaCall));
                memcpy(tf.srcCall, f.nodeCall, sizeof(tf.srcCall));
                tf.id = f.id;
                // Send the ACK via the same transport the peer is reachable on
                bool nodeOnWifi = false;
                for (size_t pi = 0; pi < peerList.size(); pi++) {
                    if (strcmp(f.nodeCall, peerList[pi].nodeCall) == 0 && peerList[pi].port == 1) {
                        nodeOnWifi = true;
                        break;
                    }
                }
                if (nodeOnWifi) {
                    tf.port = 1;
                    tf.transmitMillis = 0;
                    txBuffer.push_back(tf);
                } else {
                    tf.port = 0;
                    tf.transmitMillis = millis() + calculateAckTime();
                    txBuffer.push_back(tf);
                }
            }

            // Duplicate check: scan the in-RAM ring-buffer for (srcCall, id)
            for (int i = 0; i < MAX_STORED_MESSAGES_RAM; i++) {
                if (messages[i].id == f.id) {
                    if (strcmp(messages[i].srcCall, f.srcCall) == 0) {
                        found = true;
                        break;
                    }
                }
            }

            // Update routing: the sender is reachable via nodeCall
            addRoutingList(f.srcCall, f.nodeCall, f.hopCount);

            // Duplicate detected: remove any remaining relay copies from TX buffer
            if (found) {
                txBuffer.erase(
                    std::remove_if(txBuffer.begin(), txBuffer.end(),
                        [&](const Frame& txB) {
                            return (strcmp(txB.srcCall, f.srcCall) == 0) && (txB.id == f.id)
                                && (txB.frameType == Frame::FrameTypes::MESSAGE_FRAME);
                        }),
                    txBuffer.end()
                );
            }

            // Check if this message is addressed to someone else (private message not for us)
            bool forOther = (strlen(f.dstCall) > 0) && (strcmp(f.dstCall, settings.mycall) != 0);

            // Store (srcCall, id) in the ring-buffer to suppress future duplicates
            if ((found == false) && (f.messageLength > 0)) {
                strncpy(messages[messagesHead].srcCall, f.srcCall, MAX_CALLSIGN_LENGTH + 1);
                messages[messagesHead].id = f.id;
                messagesHead++;
                if (messagesHead >= MAX_STORED_MESSAGES_RAM) { messagesHead = 0; }
            }

            if ((found == false) && (f.messageLength > 0) && (!forOther)) {
                // ── New, unseen message addressed to us, a group, or broadcast ──

                // Serialize to JSON, broadcast via WebSocket, and append to flash
                char* jsonBuffer = (char*)malloc(4096);
                if (jsonBuffer == nullptr) {
                    Serial.println("[OOM] processRxFrame: malloc failed");
                    break;
                }
                size_t len = f.messageJSON(jsonBuffer, 4096);
                #ifdef HAS_WIFI
                wsBroadcast(jsonBuffer, len);
                #endif
                addJSONtoFile(jsonBuffer, len, "/messages.json", MAX_STORED_MESSAGES);
                #ifdef LILYGO_T_LORA_PAGER
                // Archive to SD card without size limit when a card is inserted
                pagerAddMessageToSD(jsonBuffer, len);
                #endif
                free(jsonBuffer);
                jsonBuffer = nullptr;

                // Display incoming message on T-LoraPager screen
                #ifdef LILYGO_T_LORA_PAGER
                if (f.messageType == Frame::MessageTypes::TEXT_MESSAGE) {
                    char textBuf[261] = {0};
                    memcpy(textBuf, f.message, f.messageLength);
                    displayOnNewMessage(f.srcCall, textBuf, f.dstGroup, f.dstCall);
                }
                displayMonitorFrame(f);
                #endif

                // Display incoming message on SenseCAP Indicator screen
                #ifdef SEEED_SENSECAP_INDICATOR
                if (f.messageType == Frame::MessageTypes::TEXT_MESSAGE) {
                    char textBuf[261] = {0};
                    memcpy(textBuf, f.message, f.messageLength);
                    displayOnNewMessage(f.srcCall, textBuf, f.dstGroup, f.dstCall);
                }
                displayMonitorFrame(f);
                #endif

                // Show last message on status display (SSD1306 or E-Paper)
                #if defined(HELTEC_WIFI_LORA_32_V3) || defined(LILYGO_T3_LORA32_V1_6_1) || defined(LILYGO_T_BEAM) || defined(HELTEC_HT_TRACKER_V1_2) || defined(LILYGO_T_ECHO)
                if (f.messageType == Frame::MessageTypes::TEXT_MESSAGE) {
                    char textBuf[261] = {0};
                    memcpy(textBuf, f.message, f.messageLength);
                    onStatusDisplayMessage(f.srcCall, textBuf, f.dstGroup, f.dstCall);
                }
                #endif

                // TRACE echo: if we are the destination, append our callsign + time and reply
                if ((strcmp(f.dstCall, settings.mycall) == 0) && (f.messageType == Frame::MessageTypes::TRACE_MESSAGE) && (strstr((char*)f.message, "ECHO") == NULL)) {
                        char traceMsg[261] = {0};
                        size_t traceLen = f.messageLength;
                        if (traceLen > 255) traceLen = 255;
                        memcpy(traceMsg, f.message, traceLen);
                        const char* echoTag = " -> ECHO ";
                        size_t callLen = strlen(settings.mycall);
                        char timeStr[16];
                        getFormattedTime("%H:%M:%S", timeStr, sizeof(timeStr));
                        size_t timeLen = strlen(timeStr);
                        size_t needed = 9 + callLen + 1 + timeLen;
                        if (traceLen + needed <= 260) {
                            memcpy(&traceMsg[traceLen], echoTag, 9);
                            traceLen += 9;
                            memcpy(&traceMsg[traceLen], settings.mycall, callLen);
                            traceLen += callLen;
                            traceMsg[traceLen++] = ' ';
                            memcpy(&traceMsg[traceLen], timeStr, timeLen);
                            traceLen += timeLen;
                        }
                        if (traceLen > 255) traceLen = 255;
                        traceMsg[traceLen] = '\0';
                        sendMessage(f.srcCall, traceMsg, Frame::MessageTypes::TRACE_MESSAGE);
                }

                // Remote COMMAND: execute instructions sent directly to this node
                if ((strcmp(f.dstCall, settings.mycall) == 0) && (f.messageType == Frame::MessageTypes::COMMAND_MESSAGE) ) {
                    switch (f.message[0]) {
                        case 0xff: // Version query: reply with firmware info string
                            sendMessage(f.srcCall, NAME " " VERSION " " PIO_ENV_NAME);
                            break;
                        case 0xfe: // Reboot: schedule restart in 2.5 s
                            rebootTimer = millis() + 2500; rebootRequested = true;
                            break;
                    }
                }

                // Repeat / relay the message to reachable peers
                // Conditions: repeat enabled, hop limit not exceeded, not addressed to us
                if ((settings.loraRepeat == true) && (f.hopCount < extSettings.maxHopMessage) && (strcmp(f.dstCall, settings.mycall) != 0)) {
                    // Build the relay frame
                    tf.frameType = f.frameType;
                    memcpy(tf.srcCall, f.srcCall, sizeof(tf.srcCall));
                    memcpy(tf.dstGroup, f.dstGroup, sizeof(tf.dstGroup));
                    memcpy(tf.dstCall, f.dstCall, sizeof(tf.dstCall));
                    tf.hopCount = f.hopCount;
                    if (tf.hopCount < 15) {tf.hopCount ++;}
                    tf.messageType = f.messageType;
                    memcpy(tf.message, f.message, sizeof(tf.message));
                    tf.messageLength = f.messageLength;
                    tf.id = f.id;
                    tf.timestamp = time(NULL);
                    tf.syncFlag = false;

                    // Look up a direct route to the destination
                    bool routing = false;
                    char viaCall[MAX_CALLSIGN_LENGTH + 1];
                    getRoute(f.dstCall, viaCall, MAX_CALLSIGN_LENGTH + 1);
                    if (strlen(viaCall) > 0) { routing = true; }

                    // Prefer WiFi: skip LoRa if the next hop is reachable via WiFi
                    bool routeViaWifi = false;
                    if (routing) {
                        for (size_t pi = 0; pi < peerList.size(); pi++) {
                            if (strcmp(peerList[pi].nodeCall, viaCall) == 0 &&
                                peerList[pi].port == 1 && peerList[pi].available) {
                                routeViaWifi = true; break;
                            }
                        }
                    }

                    // TRACE: append our callsign + timestamp to the path (once, before port loop)
                    if (f.messageType == Frame::MessageTypes::TRACE_MESSAGE) {
                        size_t callLen = strlen(settings.mycall);
                        char timeStr[16];
                        getFormattedTime("%H:%M:%S", timeStr, sizeof(timeStr));
                        size_t timeLen = strlen(timeStr);
                        size_t needed = 4 + callLen + 1 + timeLen;
                        if (tf.messageLength + needed <= sizeof(tf.message)) {
                            memcpy(&tf.message[tf.messageLength], " -> ", 4);
                            tf.messageLength += 4;
                            memcpy(&tf.message[tf.messageLength], settings.mycall, callLen);
                            tf.messageLength += callLen;
                            tf.message[tf.messageLength++] = ' ';
                            memcpy(&tf.message[tf.messageLength], timeStr, timeLen);
                            tf.messageLength += timeLen;
                        }
                        if (tf.messageLength > 255) tf.messageLength = 255;
                    }

                    // Check if the routed next-hop is actually reachable.
                    // If not, fall back to flooding so the message isn't silently lost.
                    if (routing) {
                        bool routeReachable = false;
                        for (size_t pi = 0; pi < peerList.size(); pi++) {
                            if (strcmp(peerList[pi].nodeCall, viaCall) == 0 &&
                                peerList[pi].available &&
                                strcmp(peerList[pi].nodeCall, f.nodeCall) != 0 &&
                                strcmp(peerList[pi].nodeCall, f.srcCall) != 0) {
                                routeReachable = true;
                                break;
                            }
                        }
                        if (!routeReachable) {
                            routing = false;  // fall back to flood
                            routeViaWifi = false;
                        }
                    }

                    // Iterate ports: WiFi first (port 1), then LoRa (port 0)
                    for (int _port = 1; _port >= 0; _port--) {
                        tf.port = (uint8_t)_port;
                        if (tf.port == 0 && routeViaWifi) continue; // skip LoRa when WiFi route exists

                        switch (tf.port){
                            case 0: tf.transmitMillis = millis() + calculateRetryTime(); break;
                            case 1: tf.transmitMillis = millis() + UDP_TX_RETRY_TIME; break;
                        }

                        // Enqueue a relay copy for every eligible peer on this port
                        for (int i = 0; i < peerList.size(); i++) {
                            // Skip peers that have already relayed this message
                            found = checkACK(f.srcCall, peerList[i].nodeCall, f.id);

                            // Enqueue: not to original sender, not to message source,
                            // only on matching port, only if not already ACK'd
                            if ((found == false) && (peerList[i].available == true) && (peerList[i].port == tf.port) && (strcmp(peerList[i].nodeCall, f.nodeCall) != 0) && (strcmp(peerList[i].nodeCall, f.srcCall) != 0)) {
                                if ((routing == false) || ( strcmp(peerList[i].nodeCall, viaCall) == 0)) {
                                    memcpy(tf.viaCall, peerList[i].nodeCall, sizeof(tf.viaCall));
                                    tf.retry = TX_RETRY;
                                    tf.initRetry = TX_RETRY;
                                    txBuffer.push_back(tf);
                                    // Pre-record ACK so we don't send a redundant ACK
                                    // if the peer echoes the message back
                                    addACK(tf.srcCall, tf.viaCall, tf.id);
                                }
                            }
                        }
                    }
                }

            }

            break;
    }
 }


// ── Arduino lifecycle ─────────────────────────────────────────────────────────

/**
 * @brief One-time hardware and software initialisation.
 *
 * Execution order:
 *  1. UART at 115200 baud (with a short delay on USB-CDC platforms).
 *  2. CPU locked to 240 MHz; noisy ESP-IDF log categories silenced.
 *  3. Pre-allocated capacity for peerList, txBuffer, and routingList.
 *  4. Persistent settings loaded from NVS via loadSettings().
 *  5. LittleFS mounted; fsMutex created; messages.json pre-loaded into RAM.
 *  6. Hardware abstraction layer (HAL) initialised.
 *  7. WiFi started; NTP / timezone configured; web server started.
 */
void setup() {
    // Initialise UART debug output
    Serial.begin(115200);
    #ifndef NRF52_PLATFORM
    // setDebugOutput erst nach Settings-Load aktivieren (siehe unten),
    // damit frühe UART-Ausgaben nicht den esptool auto-reset stören.
    #endif

    #if defined(LILYGO_T_LORA_PAGER)
    // USB-CDC needs ~1 s to enumerate; early output would be lost
    delay(2000);
    Serial.println("=== rMesh T-LoraPager boot ===");
    Serial.printf("PSRAM: %s (%u bytes)\n", psramFound() ? "OK" : "NOT FOUND", ESP.getPsramSize());
    Serial.printf("Free heap: %u\n", ESP.getFreeHeap());
    Serial.flush();
    #elif defined(SEEED_SENSECAP_INDICATOR)
    // UART0 via CH340 bridge — ready immediately, no wait needed
    delay(100);
    #elif defined(NRF52_PLATFORM)
    // USB-CDC on nRF52840 — wait up to 3s for host to connect
    {
        // Early LED feedback: blink to show firmware is alive
        pinMode(PIN_LED_GREEN, OUTPUT);
        digitalWrite(PIN_LED_GREEN, HIGH);
        uint32_t usbWait = millis();
        while (!Serial && (millis() - usbWait < 3000)) {
            delay(100);
        }
        digitalWrite(PIN_LED_GREEN, LOW);
    }
    #else
    {
        uint32_t serialWait = millis();
        while (!Serial && (millis() - serialWait < 3000)) { delay(10); }
    }
    #endif

    #ifndef NRF52_PLATFORM
    lastResetReason = getResetReasonStr();
    Serial.printf("\n[Boot] Reset reason: %s\n", lastResetReason);
    Serial.printf("[Boot] Free heap: %u bytes\n", ESP.getFreeHeap());

    // ESP32 (original): Task-WDT deaktivieren, da der WiFi-Stack bei
    // Scans/Reconnects CPU 0 für >30s blockiert. ESP32-S3 hat dieses
    // Problem nicht. Der Interrupt-WDT bleibt als Sicherheitsnetz aktiv.
    // WDT wird mit langem Timeout neu initialisiert statt deaktiviert,
    // damit esp_task_wdt_reset()-Aufrufe aus Arduino/async_tcp nicht spammen.
    #if !CONFIG_IDF_TARGET_ESP32S3 && !CONFIG_IDF_TARGET_ESP32S2 && !CONFIG_IDF_TARGET_ESP32C3
    esp_task_wdt_deinit();
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 120000,
        .idle_core_mask = 0,
        .trigger_panic = true
    };
    esp_task_wdt_init(&twdt_config);
    #endif
    #endif

    #ifndef NRF52_PLATFORM
    // 160 MHz: guter Kompromiss zwischen Stromverbrauch und WiFi-Stabilität.
    // Kein dynamisches Umschalten – APB-Clock-Wechsel stört laufende WiFi-Verbindungen.
    setCpuFrequencyMhz(160);
    // Suppress verbose ESP-IDF log output for noisy subsystems
    esp_log_level_set("NetworkUdp", ESP_LOG_NONE);
    esp_log_level_set("vfs", ESP_LOG_NONE);
    #endif

    // Pre-allocate vector capacity to avoid heap fragmentation at runtime
    peerList.reserve(PEER_LIST_SIZE);
    txBuffer.reserve(TX_BUFFER_SIZE);
    routingList.reserve(ROUTING_BUFFER_SIZE);

    // Mount filesystem (nRF52 Preferences uses InternalFS, so mount first)
    #ifdef NRF52_PLATFORM
    InternalFS.begin();
    #else
    if (!LittleFS.begin(false)) {
        Serial.println("[FS] Mount failed — formatting...");
        if (!LittleFS.begin(true)) {
            Serial.println("[FS] Format failed!");
        }
    }
    #endif
    fsMutex = xSemaphoreCreateMutex();

    // Load user settings from NVS / InternalFS
    loadSettings();

    #ifndef NRF52_PLATFORM
    Serial.setDebugOutput(serialDebug);
    #endif

    // Pre-populate the in-RAM deduplication ring-buffer from messages.json
    File file = LittleFS.open("/messages.json", "r");
    if (file) {
        JsonDocument doc;
        while (file.available()) {
            DeserializationError error = deserializeJson(doc, file);
            if (error == DeserializationError::Ok) {
                const char* tempSrc = doc["message"]["srcCall"] | "";
                uint32_t tempId = doc["message"]["id"] | 0;
                strncpy(messages[messagesHead].srcCall, tempSrc, MAX_CALLSIGN_LENGTH);
                messages[messagesHead].id = doc["message"]["id"].as<uint32_t>();
                messagesHead++;
                if (messagesHead >= MAX_STORED_MESSAGES_RAM) { messagesHead = 0; }
            } else if (error != DeserializationError::EmptyInput) {
                file.readStringUntil('\n'); // skip malformed line and continue
            }
        }
        file.close();
    }

    // Restore persisted peers and routes from flash
    loadPeers();
    loadRoutes();

    // Initialise LoRa radio and any board-specific peripherals
    initHal();

    // Initialise status display (if present)
    #if defined(HELTEC_WIFI_LORA_32_V3) || defined(LILYGO_T3_LORA32_V1_6_1) || defined(LILYGO_T_BEAM) || defined(HELTEC_HT_TRACKER_V1_2) || defined(LILYGO_T_ECHO)
    initStatusDisplay();
    #endif

    #ifdef HAS_WIFI
    // Register WiFi scan handler once (before wifiInit, which may be called repeatedly)
    WiFi.onEvent(onWiFiGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(onWiFiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.onEvent(onWiFiScanDone, ARDUINO_EVENT_WIFI_SCAN_DONE);
    // Connect to WiFi (AP or STA mode depending on settings)
    wifiInit();
    #endif

    // Set system time to epoch 0 and configure NTP + timezone
    struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    #ifdef HAS_WIFI
    configTzTime(TZ_INFO, settings.ntpServer);
    #endif

    // Start the async web server and WebSocket endpoint
    startWebServer();

    Serial.printf("\n\n\n%s\n%s %s\nREADY.\n", PIO_ENV_NAME, NAME, VERSION);

    // Emit ready event for test framework (only when debug enabled)
    if (serialDebug) {
        JsonDocument dbgReady;
        dbgReady["event"] = "ready";
        dbgReady["call"] = settings.mycall;
        dbgReady["version"] = VERSION;
        dbgReady["board"] = PIO_ENV_NAME;
        Serial.print("DBG:");
        serializeJson(dbgReady, Serial);
        Serial.println();
    }
}


/**
 * @brief Main application loop — runs continuously after setup().
 *
 * Each iteration handles (in order):
 *  1. Serial command processing.
 *  2. WiFi status indicator update.
 *  3. Display polling (T-LoraPager / SenseCAP Indicator only).
 *  4. Periodic ANNOUNCE beacon on both LoRa and UDP.
 *  5. TX-buffer processing: synchronous frame scheduling and transmission
 *     with per-frame retry logic and peer availability tracking.
 *  6. LoRa and UDP receive dispatch via processRxFrame().
 *  7. 1-second WebSocket status broadcast + peer list maintenance.
 *  8. Reboot / deep-sleep if flagged by the web UI or a remote command.
 *  9. OTA update checks (periodic and manual).
 * 10. messages.json housekeeping (daily trim).
 * 11. Topology reporting (hourly + change-driven 30 s debounce).
 */
void loop() {
    // ── 1. Serial input ───────────────────────────────────────────────────────
    checkSerialRX();

    // ── 2. WiFi indicator ─────────────────────────────────────────────────────
    showWiFiStatus();

    // ── 3. Display polling ────────────────────────────────────────────────────
    #ifdef LILYGO_T_LORA_PAGER
    displayUpdateLoop();
    #endif
    #ifdef SEEED_SENSECAP_INDICATOR
    displayUpdateLoop();
    #endif
    #if defined(HELTEC_WIFI_LORA_32_V3) || defined(LILYGO_T3_LORA32_V1_6_1) || defined(LILYGO_T_BEAM) || defined(HELTEC_HT_TRACKER_V1_2)
    {
        static uint32_t oledRefreshTimer = 0;
        if (oledEnabled && timerExpired(oledRefreshTimer)) {
            oledRefreshTimer = millis() + 5000;
            updateStatusDisplay();
        }
    }
    #endif
    #ifdef LILYGO_T_ECHO
    displayUpdateLoop();
    #endif

    // ── 4. ANNOUNCE beacon ────────────────────────────────────────────────────
    // Enqueue an ANNOUNCE_FRAME on both WiFi (port 1) and LoRa (port 0)
    if (timerExpired(announceTimer)) {
        announceTimer = millis() + ANNOUNCE_TIME;
        Frame f;
        f.frameType = Frame::FrameTypes::ANNOUNCE_FRAME;
        f.transmitMillis = 0;
        #ifdef HAS_WIFI
        f.port = 1;
        txBuffer.push_back(f);
        #endif
        f.port = 0;
        txBuffer.push_back(f);
        sendPeerList();
    }

    // ── 5. TX-buffer draining ─────────────────────────────────────────────────
    // Only transmit when no LoRa TX/RX is already in progress
    if ((txFlag == false) && (rxFlag == false)) {

        // Synchronous frames (retry > 1) must be sent one at a time per port.
        // Check whether all previously marked sync frames have been sent.
        bool sendNewSyncFrame = true;
        for (int i = 0; i < txBuffer.size(); i++) {
            if (txBuffer[i].syncFlag == true) {sendNewSyncFrame = false;}
        }

        // Mark the first unsent sync frame per port as ready to send
        if (sendNewSyncFrame == true) {
            for (int port = 0; port <= 1; port++) {
                for (int i = 0; i < txBuffer.size(); i++) {
                    if ((txBuffer[i].retry > 1) && (txBuffer[i].port == port)) {
                        txBuffer[i].syncFlag = true;
                        switch (txBuffer[i].port){
                            case 0: txBuffer[i].transmitMillis = millis() + calculateRetryTime(); break;
                            case 1: txBuffer[i].transmitMillis = millis() + UDP_TX_RETRY_TIME; break;
                        }
                        break;
                    }
                }
            }
        }

        // Iterate the buffer and send any frame whose deadline has passed
        if (txBuffer.size() == 0) {currentRetry = 0;}
        for (int i = 0; i < txBuffer.size(); i++) {
            if (timerExpired(txBuffer[i].transmitMillis) && ((txBuffer[i].retry <= 1) || (txBuffer[i].syncFlag == true))) {
                // Flux guard: enforce minimum pause between LoRa transmissions
                // so remote receivers can settle back into RX mode (improves range)
                if (txBuffer[i].port == 0 && !timerExpired(loraFluxGuard)) break;

                // Transmit — discard silently if LoRa is disabled on this device
                if (txBuffer[i].port == 0 && !loraEnabled) {
                    txBuffer[i].retry = 0;
                } else {
                    switch (txBuffer[i].port){
                        case 0: {
                            // Duty cycle enforcement for public SRD band (10% in 60s)
                            uint32_t toa = getTOA(txBuffer[i].messageLength + 10 + 2 * MAX_CALLSIGN_LENGTH);
                            if (isPublicBand(settings.loraFrequency) && !dutyCycleAllowed(toa)) {
                                // Postpone frame instead of dropping it
                                txBuffer[i].transmitMillis = millis() + 5000;
                                break;
                            }
                            transmitFrame(txBuffer[i]);
                            if (isPublicBand(settings.loraFrequency)) {
                                dutyCycleTrackTx(toa);
                            }
                            loraFluxGuard = millis() + getTOA(10 + 2 * MAX_CALLSIGN_LENGTH);
                            break;
                        }
                        case 1: sendUDP(txBuffer[i]); break;
                    }

                    // Debug output for test framework
                    if (serialDebug) {
                        JsonDocument dbgTx;
                        dbgTx["event"] = "tx";
                        dbgTx["frameType"] = txBuffer[i].frameType;
                        if (strlen(txBuffer[i].srcCall) > 0) dbgTx["srcCall"] = txBuffer[i].srcCall;
                        if (strlen(txBuffer[i].nodeCall) > 0) dbgTx["nodeCall"] = txBuffer[i].nodeCall;
                        if (strlen(txBuffer[i].dstCall) > 0) dbgTx["dstCall"] = txBuffer[i].dstCall;
                        if (strlen(txBuffer[i].viaCall) > 0) dbgTx["viaCall"] = txBuffer[i].viaCall;
                        dbgTx["id"] = txBuffer[i].id;
                        dbgTx["hopCount"] = txBuffer[i].hopCount;
                        dbgTx["port"] = txBuffer[i].port;
                        dbgTx["retry"] = txBuffer[i].retry;
                        Serial.print("DBG:");
                        serializeJson(dbgTx, Serial);
                        Serial.println();
                    }
                }
                // Decrement retry counter and track progress for the status update
                if (txBuffer[i].retry > 0) {txBuffer[i].retry --;}
                currentRetry = txBuffer[i].initRetry - txBuffer[i].retry;
                // Schedule next retry
                switch (txBuffer[i].port){
                    case 0: txBuffer[i].transmitMillis = millis() + calculateRetryTime(); break;
                    case 1: txBuffer[i].transmitMillis = millis() + UDP_TX_RETRY_TIME; break;
                }
                // All retries exhausted: remove the frame.
                // For multi-retry frames (initRetry > 1): the peer is unreachable,
                // so mark it unavailable and purge ALL pending frames to that peer
                // to prevent txBuffer congestion.
                // For one-shot frames (initRetry <= 1, e.g. ACKs): only remove
                // the single frame, do NOT purge other frames to the same peer.
                if (txBuffer[i].retry == 0) {
                    if (txBuffer[i].initRetry > 1) {
                        char deadVia[MAX_CALLSIGN_LENGTH + 1];
                        strncpy(deadVia, txBuffer[i].viaCall, sizeof(deadVia));
                        deadVia[sizeof(deadVia) - 1] = '\0';
                        uint8_t deadPort = txBuffer[i].port;
                        availablePeerList(deadVia, false, deadPort);
                        // Set cooldown so ANNOUNCE_ACK cannot immediately re-enable
                        for (auto& p : peerList) {
                            if (strcmp(p.nodeCall, deadVia) == 0 && p.port == deadPort) {
                                p.cooldownUntil = millis() + PEER_RETRY_COOLDOWN;
                                break;
                            }
                        }
                        txBuffer.erase(
                            std::remove_if(txBuffer.begin(), txBuffer.end(),
                                [&](const Frame& txB) {
                                    return (strcmp(txB.viaCall, deadVia) == 0) && (txB.port == deadPort);
                                }),
                            txBuffer.end()
                        );
                    } else {
                        txBuffer.erase(txBuffer.begin() + i);
                    }
                    i = -1; // restart iteration since indices shifted
                }
                break;
            }
        }
    }


    // ── 6. Receive dispatch ───────────────────────────────────────────────────
    Frame f;
    if (checkReceive(f)) { processRxFrame(f); }   // LoRa
    #ifdef HAS_WIFI
    if (checkUDP(f))     { processRxFrame(f); }   // UDP
    #endif

    // ── 7. Status broadcast (1 s interval) ────────────────────────────────────
    if (timerExpired(statusTimer)) {
        statusTimer = millis() + 1000;
        #ifdef HAS_WIFI
        JsonDocument doc;
        doc["status"]["time"]         = time(NULL);
        doc["status"]["tx"]           = txFlag;
        doc["status"]["rx"]           = rxFlag;
        doc["status"]["txBufferCount"]= txBuffer.size();
        doc["status"]["retry"]        = currentRetry;
        doc["status"]["heap"]         = ESP.getFreeHeap();
        doc["status"]["minHeap"]      = ESP.getMinFreeHeap();
        doc["status"]["uptime"]       = millis() / 1000;
        doc["status"]["cpuFreq"]      = getCpuFrequencyMhz();
        doc["status"]["resetReason"]  = lastResetReason;
        #ifdef HAS_BATTERY_ADC
        if (batteryEnabled) doc["status"]["battery"] = getBatteryVoltage();
        #endif
        char jsonBuffer[512];
        size_t len = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
        wsBroadcast(jsonBuffer, len);
        #endif
        // Expire stale peers once per second
        checkPeerList();
        // Flush deferred RSSI/SNR updates to WebSocket clients
        if (peerListDirty) {
            peerListDirty = false;
            sendPeerList();
        }
    }

    // ── 7b. Periodic flash persistence of routes/peers ─────────────────────
    if (timerExpired(persistTimer)) {
        persistTimer = millis() + PERSIST_INTERVAL;
        if (routesDirty) saveRoutes();
        if (peersDirty)  savePeers();
    }

    // ── 8. Reboot / shutdown ──────────────────────────────────────────────────
    #ifdef NRF52_PLATFORM
    if (rebootRequested && timerExpired(rebootTimer))  { platformRestart(); }
    if (pendingShutdown)         { platformDeepSleep(); }
    #else
    if (rebootRequested && timerExpired(rebootTimer))  { ESP.restart(); }
    if (pendingShutdown)         { esp_deep_sleep_start(); }
    #endif

    #ifdef HAS_WIFI
    // ── 9. OTA update checks ──────────────────────────────────────────────────
    if (timerExpired(updateCheckTimer)) {
        updateCheckTimer = millis() + 24 * 60 * 60 * 1000; // repeat every 24 h
        checkForUpdates();
    }
    // Manual trigger from the web UI (deferred to avoid async_tcp watchdog issues)
    if (pendingManualUpdate) {
        pendingManualUpdate = false;
        checkForUpdates();
    }
    if (pendingForceUpdate) {
        pendingForceUpdate = false;
        checkForUpdates(true, pendingForceChannel);
    }
    #endif

    // ── 10. messages.json housekeeping ────────────────────────────────────────
    if (trimNeeded) {
        trimNeeded = false;
        messagesDeleteTimer = millis() + 24 * 60 * 60 * 1000; // reset 24 h timer
        trimFile("/messages.json", MAX_STORED_MESSAGES);
    }
    if (timerExpired(messagesDeleteTimer)) {
        messagesDeleteTimer = millis() + 24 * 60 * 60 * 1000; // repeat every 24 h
        trimFile("/messages.json", MAX_STORED_MESSAGES);
    }

    #ifdef HAS_WIFI
    // ── 11. Topology reporting ────────────────────────────────────────────────
    if (timerExpired(reportingTimer)) {
        reportingTimer = millis() + 60 * 60 * 1000; // repeat every 1 h
        reportTopology();
    }
    reportTopologyIfChanged(); // change-driven report with 30 s debounce
    #endif
}
