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
#include <string>
#include <ArduinoJson.h>

#ifdef NRF52_PLATFORM
#include "util/platform_nrf52.h"
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#else
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include <nvs_flash.h>
#endif

#include "config.h"
#include "hal/hal.h"
#include "mesh/frame.h"
#include "hal/settings.h"
#include "main.h"
#include "network/wifiFunctions.h"
#include "network/webFunctions.h"
#include "util/serial.h"
#include "util/helperFunctions.h"
#include "mesh/peer.h"
#include "mesh/ack.h"
#include "network/udp.h"
#include "network/ethFunctions.h"
#include "mesh/routing.h"
#include "mesh/reporting.h"
#include "hal/dutycycle.h"
#include "util/persistence.h"
#include "time.h"
#include "util/logging.h"
#include "util/heapdbg.h"
#include "util/bgWorker.h"
#include "network/api.h"
#include "bsp/BoardFactory.h"
#include "display/statusDisplay.h"
#include "network/bt_manager.h"
#include "network/ble_transport.h"
#include "util/button_manager.h"
#include "util/led_feedback.h"


// ── Port iteration order ─────────────────────────────────────────────────────
// Network first (primary interface before secondary), LoRa last.
// Returns the number of ports written into out[] (always 3).
#ifdef HAS_WIFI
static uint8_t portOrder[3];
static void buildPortOrder() {
    // Primary network interface first, secondary second, LoRa last
    uint8_t first  = (primaryInterface == 2) ? 2 : 1;  // LAN first if primary==2, else WiFi
    uint8_t second = (first == 2) ? 1 : 2;             // the other network port
    portOrder[0] = first;
    portOrder[1] = second;
    portOrder[2] = 0;  // LoRa always last
}
#endif

// ── Global state ──────────────────────────────────────────────────────────────

/** Board configuration — set once via BoardFactory::create(). */
IBoardConfig* board = nullptr;

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
SemaphoreHandle_t listMutex = NULL;
TaskHandle_t mainLoopTaskHandle = NULL;

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

/** Deadline for the next ROUTING_INFO_MESSAGE broadcast (24 h cycle). */
uint32_t routingInfoTimer = 60 * 1000; // first send 60 s after boot

/** millis() value at which ESP.restart() is called; 0 = disabled. */
uint32_t rebootTimer = 0;
bool rebootRequested = false;

/** Retry counter for the frame currently being transmitted from txBuffer. */
uint8_t currentRetry = 0;

/** Lifetime counter of frames dropped (any reason). Total = sum of all
 *  dropped*-counters below. */
uint32_t droppedFrames = 0;

// ── Drop reason counters (#14) ────────────────────────────────────────────────
/** Frames dropped because the TX buffer was full when trying to enqueue. */
uint32_t droppedBufferFull   = 0;
/** Frames dropped after all retries to a (still alive) peer were exhausted. */
uint32_t droppedRetryExhaust = 0;
/** Frames purged in bulk because their target peer was marked dead. */
uint32_t droppedPeerDead     = 0;

// ── Drop type counters (#14) ──────────────────────────────────────────────────
/** Dropped MESSAGE_FRAME count. */
uint32_t droppedMessage      = 0;
/** Dropped MESSAGE_ACK_FRAME count. */
uint32_t droppedAck          = 0;
/** Dropped ANNOUNCE_FRAME count. */
uint32_t droppedAnnounce     = 0;
/** Dropped ANNOUNCE_ACK_FRAME count. */
uint32_t droppedAnnounceAck  = 0;
/** Dropped frames of any other type. */
uint32_t droppedOther        = 0;

/**
 * @brief Account a single dropped frame in the per-type counter.
 *        The reason counter and the global total must be updated by the caller.
 */
static inline void accountDroppedType(const Frame& f) {
    switch (f.frameType) {
        case Frame::FrameTypes::MESSAGE_FRAME:       droppedMessage++;     break;
        case Frame::FrameTypes::MESSAGE_ACK_FRAME:   droppedAck++;         break;
        case Frame::FrameTypes::ANNOUNCE_FRAME:      droppedAnnounce++;    break;
        case Frame::FrameTypes::ANNOUNCE_ACK_FRAME:  droppedAnnounceAck++; break;
        default:                                     droppedOther++;       break;
    }
}

/** Flux guard: earliest millis() at which the next LoRa TX is allowed.
 *  A short pause after each TX lets remote receivers settle back into RX,
 *  improving effective range. */
uint32_t loraFluxGuard = 0;

/** Deferred flags set by the web UI and consumed in loop(). */
bool pendingManualUpdate = false;
bool pendingShutdown     = false;
bool pendingForceUpdate  = false;
uint8_t pendingForceChannel = 0;
bool pendingLoraReinit   = false;
bool pendingSettingsSave = false;

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

    // Early duplicate skip for MESSAGE_FRAME: if we already have this (srcCall, id)
    // in the ring-buffer AND the frame is not directly addressed to us, skip the
    // expensive JSON debug serialization, addPeerList, and full case logic.
    // Directly-addressed duplicates (viaCall == mycall) are NOT skipped so that
    // re-ACKs still work for retransmitting peers.
    // NOTE: pending relay copies in txBuffer are intentionally kept here – the
    // suppress-on-heard decision must be made directly before TX, otherwise an
    // echo from a neighbour would prematurely cancel our own scheduled repeat.
    if (f.frameType == Frame::FrameTypes::MESSAGE_FRAME &&
        strcmp(f.viaCall, settings.mycall) != 0) {
        for (int i = 0; i < MAX_STORED_MESSAGES_RAM; i++) {
            if (messages[i].id == f.id &&
                strcmp(messages[i].srcCall, f.srcCall) == 0) {
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
        logJson(dbg);
    }

    // Record RX event in API ring buffer (always active, independent of serialDebug)
    #ifdef HAS_WIFI
    apiRecordRxEvent(f);
    #endif

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
                        if (strcmp(f.nodeCall, peerList[pi].nodeCall) == 0 && peerList[pi].port == 1 && peerList[pi].available) {
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
                if (txBuffer.size() < TX_BUFFER_SIZE) txBuffer.push_back(tf);
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

            // Record ACK event in API ring buffer and mark message as acked
            #ifdef HAS_WIFI
            apiRecordAckEvent(f);
            apiMarkMessageAcked(f.srcCall, f.id);
            #endif

            // Debug output for test framework
            if (serialDebug) {
                JsonDocument dbgAck;
                dbgAck["event"] = "ack";
                dbgAck["srcCall"] = f.srcCall;
                dbgAck["nodeCall"] = f.nodeCall;
                dbgAck["id"] = f.id;
                logJson(dbgAck);
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
                memcpy(tf.srcCall, f.srcCall, sizeof(tf.srcCall));
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
                    if (txBuffer.size() < TX_BUFFER_SIZE) txBuffer.push_back(tf);
                } else {
                    tf.port = 0;
                    tf.transmitMillis = millis() + calculateAckTime();
                    if (txBuffer.size() < TX_BUFFER_SIZE) txBuffer.push_back(tf);
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

            // NOTE: We intentionally do NOT erase pending relay copies from
            // txBuffer when a duplicate is seen here. A duplicate arriving
            // from another relayer means "someone else also has it" — not
            // "we already sent it". Wiping our own pending copies kills the
            // very redundancy that the multi-path relay is meant to provide.
            // The repeat block below uses `found` to skip enqueueing NEW
            // copies for already-known IDs; that is the correct dedup point.

            // Check if this message is addressed to someone else (private message not for us)
            bool forOther = (strlen(f.dstCall) > 0) && (strcmp(f.dstCall, settings.mycall) != 0);

            // Store (srcCall, id) in the ring-buffer to suppress future duplicates
            if ((found == false) && (f.messageLength > 0)) {
                strncpy(messages[messagesHead].srcCall, f.srcCall, MAX_CALLSIGN_LENGTH + 1);
                messages[messagesHead].id = f.id;
                messagesHead++;
                if (messagesHead >= MAX_STORED_MESSAGES_RAM) { messagesHead = 0; }
            }

            if ((found == false) && (f.messageLength > 0) && (!forOther) && (f.messageType != Frame::MessageTypes::ROUTING_INFO_MESSAGE)) {
                // ── New, unseen message addressed to us, a group, or broadcast ──
                // (storage / display / TRACE echo / COMMAND – local consumption only)

                // Record in API message ring buffer (always active)
                #ifdef HAS_WIFI
                apiRecordMessage(f, false);
                #endif

                // Serialize to JSON, broadcast via WebSocket, and append to flash
                char jsonBuffer[1024];
                size_t len = f.messageJSON(jsonBuffer, sizeof(jsonBuffer));
                #ifdef HAS_WIFI
                wsBroadcast(jsonBuffer, len);
                bleTransportSend(std::string(jsonBuffer, len));
                bleTransportNotifyNewMessage((uint8_t)f.id, f.srcCall);
                #endif
                addJSONtoFile(jsonBuffer, len, "/messages.json", MAX_STORED_MESSAGES);
                // Archive to SD card (no-op on boards without SD)
                pagerAddMessageToSD(jsonBuffer, len);

                // Notify display drivers of the incoming message
                if (f.messageType == Frame::MessageTypes::TEXT_MESSAGE) {
                    char textBuf[261] = {0};
                    memcpy(textBuf, f.message, f.messageLength);
                    displayOnNewMessage(f.srcCall, textBuf, f.dstGroup, f.dstCall);
                    onStatusDisplayMessage(f.srcCall, textBuf, f.dstGroup, f.dstCall);
                }
                displayMonitorFrame(f);

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

                // Remote COMMAND: execute instructions sent directly to this node (only from known peers)
                // The command is only executed after it has been successfully logged to the web backend.
                if ((strcmp(f.dstCall, settings.mycall) == 0) && (f.messageType == Frame::MessageTypes::COMMAND_MESSAGE) ) {
                    bool cmdFromKnownPeer = false;
                    for (size_t pi = 0; pi < peerList.size(); pi++) {
                        if (strcmp(peerList[pi].nodeCall, f.srcCall) == 0 && peerList[pi].available) {
                            cmdFromKnownPeer = true;
                            break;
                        }
                    }
                    if (!cmdFromKnownPeer) break;

                    const char* cmdName = nullptr;
                    switch (f.message[0]) {
                        case 0xff: cmdName = "version"; break;
                        case 0xfe: cmdName = "reboot";  break;
                    }

                    if (cmdName && logRemoteCommand(f.srcCall, cmdName)) {
                        switch (f.message[0]) {
                            case 0xff: // Version query: reply with firmware info string
                                sendMessage(f.srcCall, NAME " " VERSION " " PIO_ENV_NAME);
                                break;
                            case 0xfe: // Reboot: schedule restart in 2.5 s
                                rebootTimer = millis() + 2500; rebootRequested = true;
                                break;
                        }
                    }
                }

            }

            // ── Repeat / relay block ─────────────────────────────────────────
            // Runs INDEPENDENTLY of the local-consumption block above, so that
            // unicast messages addressed to other nodes AND ROUTING_INFO_MESSAGE
            // frames are forwarded as well. Only condition: not yet seen and
            // has a payload.
            if ((found == false) && (f.messageLength > 0)) {
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

                    // Prefer WiFi: skip LoRa only if the next hop is reachable via WiFi
                    // AND the WiFi heartbeat is recent (<60s). Otherwise fall back to
                    // sending on both ports so a stale WiFi entry cannot block LoRa
                    // delivery (#6 in .dev/message-handling-regression.idea.md).
                    bool routeViaWifi = false;
                    if (routing) {
                        time_t nowTs = time(NULL);
                        for (size_t pi = 0; pi < peerList.size(); pi++) {
                            if (strcmp(peerList[pi].nodeCall, viaCall) == 0 &&
                                peerList[pi].port == 1 && peerList[pi].available &&
                                (nowTs - peerList[pi].timestamp) < 60) {
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
                                    if (txBuffer.size() < TX_BUFFER_SIZE) {
                                        txBuffer.push_back(tf);
                                        // Pre-record ACK so we don't send a redundant ACK
                                        // if the peer echoes the message back
                                        addACK(tf.srcCall, tf.viaCall, tf.id);
                                    } else {
                                        // #15 diagnostics: relay dropped because TX buffer full
                                        droppedFrames++;
                                        droppedBufferFull++;
                                        accountDroppedType(tf);
                                        logPrintf(LOG_WARN, "Repeat",
                                            "{\"event\":\"repeat_dropped\",\"reason\":\"buffer_full\",\"src\":\"%s\",\"id\":%u,\"via\":\"%s\",\"port\":%d}",
                                            tf.srcCall, (unsigned)tf.id, tf.viaCall, tf.port);
                                    }
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
    // Enable setDebugOutput only after settings load (see below),
    // so early UART output doesn't interfere with esptool auto-reset.
    #endif

    #if defined(ARDUINO_USB_CDC_ON_BOOT) && !defined(NRF52_PLATFORM)
    // USB-CDC needs time to enumerate; early output would be lost
    delay(2000);
    Serial.flush();
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
    logPrintf(LOG_INFO, "Boot", "Reset reason: %s", lastResetReason);
    logPrintf(LOG_INFO, "Boot", "Free heap: %u bytes", ESP.getFreeHeap());

    // ESP32 (original): Reinitialize task WDT with reduced timeout,
    // since the WiFi stack blocks CPU 0 for >30 s during scans/reconnects.
    // ESP32-S3 does not have this issue. The interrupt WDT remains
    // active as a safety net.
    #if !CONFIG_IDF_TARGET_ESP32S3 && !CONFIG_IDF_TARGET_ESP32S2 && !CONFIG_IDF_TARGET_ESP32C3
    esp_task_wdt_deinit();
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 45000,
        .idle_core_mask = 0,
        .trigger_panic = true
    };
    esp_task_wdt_init(&twdt_config);
    #endif
    #endif

    #ifndef NRF52_PLATFORM
    // Suppress verbose ESP-IDF log output for noisy subsystems
    esp_log_level_set("NetworkUdp", ESP_LOG_NONE);
    esp_log_level_set("vfs", ESP_LOG_NONE);
    #endif

    // Pre-allocate vector capacity to avoid heap fragmentation at runtime
    peerList.reserve(PEER_LIST_SIZE);
    txBuffer.reserve(TX_BUFFER_SIZE);
    #ifndef NRF52_PLATFORM
    // PSRAM boards can afford full reservation; others start small and grow
    if (psramFound()) {
        routingList.reserve(ROUTING_BUFFER_SIZE);
    } else {
        routingList.reserve(100);
    }
    #else
    routingList.reserve(100);
    #endif

    // Mount filesystem (nRF52 Preferences uses InternalFS, so mount first)
    #ifdef NRF52_PLATFORM
    InternalFS.begin();
    #else
    if (!LittleFS.begin(false)) {
        logPrintf(LOG_WARN, "FS", "Mount failed — formatting...");
        if (!LittleFS.begin(true)) {
            logPrintf(LOG_ERROR, "FS", "Format failed!");
        }
    }
    #endif
    fsMutex = xSemaphoreCreateMutex();
    listMutex = xSemaphoreCreateMutex();
    mainLoopTaskHandle = xTaskGetCurrentTaskHandle();
    initPendingSendQueue();
    initFileWriteWorker();

    // Load user settings from NVS / InternalFS
    loadSettings();
    #ifdef HAS_WIFI
    buildPortOrder();
    #endif

    #ifndef NRF52_PLATFORM
    // Apply CPU frequency from settings (default 240 MHz).
    // No dynamic switching – APB clock changes disrupt active WiFi connections.
    setCpuFrequencyMhz(cpuFrequency);
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
                strncpy(messages[messagesHead].srcCall, tempSrc, MAX_CALLSIGN_LENGTH + 1);
                messages[messagesHead].id = doc["message"]["id"].as<uint32_t>();
                if (++messagesHead >= MAX_STORED_MESSAGES_RAM) { messagesHead = 0; }
            } else if (error != DeserializationError::EmptyInput) {
                file.readStringUntil('\n'); // skip malformed line and continue
            }
        }
        file.close();
    }

    // Start shared background worker BEFORE loading peers/routes, so its
    // stack is carved out of a still-contiguous heap and never needs to
    // be reallocated later (key mitigation against heap fragmentation).
    bgWorkerInit();

    // Restore persisted peers and routes from flash
    loadPeers();
    loadRoutes();
    #ifdef HAS_WIFI
    apiLoadBuffers();
    #endif

    // Instantiate the board configuration from the BSP factory
    board = BoardFactory::create();

    // Initialise LoRa radio and any board-specific peripherals
    initHal();

    // Initialise button manager (before BT so double-click can cycle BT mode)
    buttonManagerInit();

    // Initialise display (if present)
    if (board->hasDisplay()) {
        initStatusDisplay(board);
    }
    initDisplay();

    #ifdef HAS_WIFI
    // Register WiFi scan handler once (before wifiInit, which may be called repeatedly)
    WiFi.onEvent(onWiFiGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(onWiFiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.onEvent(onWiFiScanDone, ARDUINO_EVENT_WIFI_SCAN_DONE);
    // Connect to WiFi (AP or STA mode depending on settings)
    wifiInit();
    // Initialise Ethernet (W5500) if the board has it
    ethInit();
    // Initialise BLE (mode read from NVS — OFF, COEX, or EXCLUSIVE)
    btManagerInit();
    #endif

    // Set system time to epoch 0 and configure NTP + timezone
    struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    #ifdef HAS_WIFI
    configTzTime(TZ_INFO, settings.ntpServer);
    #endif

    // Start the async web server and WebSocket endpoint
    startWebServer();

    logPrintf(LOG_INFO, "System", "");
    logPrintf(LOG_INFO, "System", "%s", PIO_ENV_NAME);
    logPrintf(LOG_INFO, "System", "%s %s", NAME, VERSION);
    logPrintf(LOG_INFO, "System", "READY.");

    // Emit ready event for test framework (only when debug enabled)
    if (serialDebug) {
        JsonDocument dbgReady;
        dbgReady["event"] = "ready";
        dbgReady["call"] = settings.mycall;
        dbgReady["version"] = VERSION;
        dbgReady["board"] = PIO_ENV_NAME;
        logJson(dbgReady);
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
    // ── 0. Deferred sends from background tasks (e.g. WebSocket) ────────────
    processPendingSends();

    // ── 1. Serial input ───────────────────────────────────────────────────────
    checkSerialRX();

    // ── 2. WiFi indicator ─────────────────────────────────────────────────────
    showWiFiStatus();

    // ── 3. Display polling ────────────────────────────────────────────────────
    // Full-UI displays (T-LoraPager, SenseCAP) and E-Paper (T-Echo)
    // run their own update loop; status-display boards (OLED, TFT) are
    // refreshed on a 5 s timer.  Weak no-ops ensure dead-code-free linking.
    displayUpdateLoop();
    if (board->hasDisplay()) {
        static uint32_t oledRefreshTimer = 0;
        if (timerExpired(oledRefreshTimer)) {
            oledRefreshTimer = millis() + 5000;
            updateStatusDisplay();
        }
        displayButtonPoll();
    }

    // ── 4. ANNOUNCE beacon ────────────────────────────────────────────────────
    // Enqueue an ANNOUNCE_FRAME on both WiFi (port 1) and LoRa (port 0)
    if (timerExpired(announceTimer)) {
        announceTimer = millis() + ANNOUNCE_TIME;
        Frame f;
        f.frameType = Frame::FrameTypes::ANNOUNCE_FRAME;
        f.transmitMillis = 0;
        // Enqueue in portOrder: primary network → secondary network → LoRa
        #ifdef HAS_WIFI
        if (loraConfigured(settings.loraFrequency)) {
            for (int pi = 0; pi < 3; pi++) {
                uint8_t p = portOrder[pi];
                if (p == 0) {
                    // LoRa — always enqueue
                    f.port = 0;
                    if (txBuffer.size() < TX_BUFFER_SIZE) txBuffer.push_back(f);
                } else if (p == 1 && wifiNodeComm) {
                    f.port = 1;
                    if (txBuffer.size() < TX_BUFFER_SIZE) txBuffer.push_back(f);
                } else if (p == 2 && ethConnected && ethNodeComm) {
                    f.port = 2;
                    if (txBuffer.size() < TX_BUFFER_SIZE) txBuffer.push_back(f);
                }
            }
        } else {
            // No frequency configured — LoRa only (will be a no-op over air)
            f.port = 0;
            if (txBuffer.size() < TX_BUFFER_SIZE) txBuffer.push_back(f);
        }
        #else
        f.port = 0;
        if (txBuffer.size() < TX_BUFFER_SIZE) txBuffer.push_back(f);
        #endif
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
            // Iterate ports in priority order: primary network → secondary → LoRa
            #ifdef HAS_WIFI
            const uint8_t* po = portOrder;
            #else
            static const uint8_t po[] = {0, 1, 2};
            #endif
            for (int pi = 0; pi < 3; pi++) {
                uint8_t port = po[pi];
                for (int i = 0; i < txBuffer.size(); i++) {
                    if ((txBuffer[i].retry > 1) && (txBuffer[i].port == port)) {
                        txBuffer[i].syncFlag = true;
                        switch (txBuffer[i].port){
                            case 0: txBuffer[i].transmitMillis = millis() + calculateRetryTime(); break;
                            case 1: // WiFi
                            case 2: // LAN
                                txBuffer[i].transmitMillis = millis() + UDP_TX_RETRY_TIME; break;
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

                // Track whether the frame was actually transmitted (not just postponed)
                bool postponed = false;

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
                                postponed = true;
                                break;
                            }
                            transmitFrame(txBuffer[i]);
                            if (isPublicBand(settings.loraFrequency)) {
                                dutyCycleTrackTx(toa);
                            }
                            loraFluxGuard = millis() + getTOA(10 + 2 * MAX_CALLSIGN_LENGTH);
                            break;
                        }
                        case 1: // WiFi UDP
                        case 2: // LAN (Ethernet) UDP
                            sendUDP(txBuffer[i]); break;
                    }

                    // Duty cycle postponed: skip retry logic, try again later
                    if (postponed) break;

                    // Record TX event in API ring buffer (always active)
                    #ifdef HAS_WIFI
                    apiRecordTxEvent(txBuffer[i]);
                    #endif

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
                        logJson(dbgTx);
                    }
                }
                // Decrement retry counter and track progress for the status update
                if (txBuffer[i].retry > 0) {txBuffer[i].retry --;}
                currentRetry = txBuffer[i].initRetry - txBuffer[i].retry;
                // Schedule next retry
                switch (txBuffer[i].port){
                    case 0: txBuffer[i].transmitMillis = millis() + calculateRetryTime(); break;
                    case 1: // WiFi
                    case 2: // LAN
                        txBuffer[i].transmitMillis = millis() + UDP_TX_RETRY_TIME; break;
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
                        auto newEnd = std::remove_if(txBuffer.begin(), txBuffer.end(),
                                [&](const Frame& txB) {
                                    return (strcmp(txB.viaCall, deadVia) == 0) && (txB.port == deadPort);
                                });
                        uint32_t purged = (uint32_t)std::distance(newEnd, txBuffer.end());
                        droppedFrames  += purged;
                        droppedPeerDead += purged;
                        for (auto it = newEnd; it != txBuffer.end(); ++it) {
                            accountDroppedType(*it);
                        }
                        txBuffer.erase(newEnd, txBuffer.end());
                    } else {
                        droppedFrames++;
                        droppedRetryExhaust++;
                        accountDroppedType(txBuffer[i]);
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

    // ── 7. Status broadcast (3 s interval) ────────────────────────────────────
    if (timerExpired(statusTimer)) {
        statusTimer = millis() + 3000;
        #ifdef HAS_WIFI
        // Periodically clean up dead WebSocket clients to prevent heap leaks
        ws.cleanupClients();

        // Only broadcast status if heap has enough room for the WebSocket
        // shared_ptr allocation (~400 bytes per message per client)
        if (ESP.getFreeHeap() > 40000) {
            char jsonBuffer[640];
            int len;
            // Compact dropped-frames sub-object (#14): per-reason and per-type counters
            char droppedJson[160];
            snprintf(droppedJson, sizeof(droppedJson),
                "{\"total\":%lu,\"bufferFull\":%lu,\"retryExhaust\":%lu,\"peerDead\":%lu,"
                "\"msg\":%lu,\"ack\":%lu,\"ann\":%lu,\"annAck\":%lu,\"other\":%lu}",
                (unsigned long)droppedFrames,
                (unsigned long)droppedBufferFull, (unsigned long)droppedRetryExhaust, (unsigned long)droppedPeerDead,
                (unsigned long)droppedMessage, (unsigned long)droppedAck,
                (unsigned long)droppedAnnounce, (unsigned long)droppedAnnounceAck, (unsigned long)droppedOther);

            #ifdef HAS_BATTERY_ADC
            if (batteryEnabled) {
                len = snprintf(jsonBuffer, sizeof(jsonBuffer),
                    "{\"status\":{\"time\":%ld,\"tx\":%s,\"rx\":%s,\"txBufferCount\":%u,"
                    "\"retry\":%u,\"dropped\":%lu,\"droppedBy\":%s,\"heap\":%u,\"minHeap\":%u,\"uptime\":%lu,"
                    "\"cpuFreq\":%u,\"resetReason\":\"%s\",\"battery\":%.2f}}",
                    (long)time(NULL), txFlag ? "true" : "false", rxFlag ? "true" : "false",
                    (unsigned)txBuffer.size(), currentRetry, (unsigned long)droppedFrames, droppedJson,
                    ESP.getFreeHeap(), ESP.getMinFreeHeap(), millis() / 1000, getCpuFrequencyMhz(),
                    lastResetReason, getBatteryVoltage());
            } else
            #endif
            {
                len = snprintf(jsonBuffer, sizeof(jsonBuffer),
                    "{\"status\":{\"time\":%ld,\"tx\":%s,\"rx\":%s,\"txBufferCount\":%u,"
                    "\"retry\":%u,\"dropped\":%lu,\"droppedBy\":%s,\"heap\":%u,\"minHeap\":%u,\"uptime\":%lu,"
                    "\"cpuFreq\":%u,\"resetReason\":\"%s\"}}",
                    (long)time(NULL), txFlag ? "true" : "false", rxFlag ? "true" : "false",
                    (unsigned)txBuffer.size(), currentRetry, (unsigned long)droppedFrames, droppedJson,
                    ESP.getFreeHeap(), ESP.getMinFreeHeap(), millis() / 1000, getCpuFrequencyMhz(),
                    lastResetReason);
            }
            if (len > 0 && (size_t)len < sizeof(jsonBuffer)) {
                wsBroadcast(jsonBuffer, len);
            }
        }
        #endif
        // Expire stale peers once per second
        checkPeerList();
        // Flush deferred RSSI/SNR updates to WebSocket clients
        if (peerListDirty) {
            peerListDirty = false;
            sendPeerList();
        }
    }

    // ── 7a. Heap watchdog — reboot when heap is critically low ──────────────
    #ifndef NRF52_PLATFORM
    {
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t maxAlloc = ESP.getMaxAllocHeap();
        if ((freeHeap < 10000 || maxAlloc < 4096) && !rebootRequested) {
            logPrintf(LOG_ERROR, "HEAP", "Critical: %u bytes free, largest block %u — rebooting",
                      freeHeap, maxAlloc);
            rebootTimer = millis() + 500;
            rebootRequested = true;
        }
    }
    #endif

    // ── 7b. Periodic ROUTING_INFO beacon (every 24 h) ──────────────────────
    if (timerExpired(routingInfoTimer)) {
        routingInfoTimer = millis() + 24UL * 60 * 60 * 1000; // repeat every 24 h
        Frame rf;
        rf.frameType = Frame::FrameTypes::MESSAGE_FRAME;
        rf.messageType = Frame::MessageTypes::ROUTING_INFO_MESSAGE;
        strncpy(rf.srcCall, settings.mycall, sizeof(rf.srcCall));
        rf.message[0] = 0x00;
        rf.messageLength = 1;
        sendFrame(rf);
        logPrintf(LOG_INFO, "Route", "Sent ROUTING_INFO beacon");
    }

    // ── 7c. Periodic flash persistence of routes/peers ─────────────────────
    if (timerExpired(persistTimer)) {
        persistTimer = millis() + PERSIST_INTERVAL;
        if (routesDirty) saveRoutes();
        if (peersDirty)  savePeers();
        #ifdef HAS_WIFI
        if (apiBuffersDirty) apiSaveBuffers();
        #endif
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

    // Deferred settings save from WebSocket handler
    if (pendingSettingsSave) {
        pendingSettingsSave = false;
        saveSettings();
        #ifdef HAS_WIFI
        buildPortOrder();  // primaryInterface may have changed
        #endif
    }

    // Deferred LoRa reinit after settings change
    if (pendingLoraReinit) {
        pendingLoraReinit = false;
        initHal();
    }

    // Periodic LoRa recovery: if radio failed to init, retry every 30 s
    static uint32_t loraRecoveryTimer = 0;
    if (!loraReady && loraEnabled && loraConfigured(settings.loraFrequency)
        && timerExpired(loraRecoveryTimer)) {
        loraRecoveryTimer = millis() + 30000;
        logPrintf(LOG_INFO, "LoRa", "Attempting radio recovery...");
        initHal();
    }

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

    btManagerTick();
    ledFeedbackTick();

    // ── Button gesture dispatch ──────────────────────────────────────────────
    switch (buttonManagerTick()) {
        case ButtonEvent::SINGLE_CLICK:
            // Toggle status display on/off
            ledFeedback(1, 100, 0);
            if (hasStatusDisplay()) {
                if (oledEnabled) disableStatusDisplay();
                else enableStatusDisplay();
            }
            break;
        case ButtonEvent::DOUBLE_CLICK:
            // Cycle BT mode: OFF → COEX → EXCLUSIVE → OFF
            btManagerCycleMode();
            ledFeedback(3, 80, 80);
            logPrintf(LOG_INFO, "Button", "BT mode → %d", (int)btManagerGetMode());
            break;
        case ButtonEvent::LONG_PRESS:
            // Toggle WiFi AP ↔ Client mode + reboot
            ledFeedback(1, 500, 0);
            #ifdef HAS_WIFI
            settings.apMode = !settings.apMode;
            saveSettings();
            rebootTimer = millis() + 1000;
            rebootRequested = true;
            #endif
            break;
        case ButtonEvent::VERY_LONG_PRESS:
            // Factory reset
            ledFeedback(255, 50, 50);
            logPrintf(LOG_WARN, "Button", "FACTORY RESET in 2 seconds!");
            delay(2000);
            #ifdef NRF52_PLATFORM
            InternalFS.format();
            NVIC_SystemReset();
            #else
            nvs_flash_erase();
            LittleFS.format();
            ESP.restart();
            #endif
            break;
        default:
            break;
    }

    heapTick();
}
