#pragma once

/**
 * @file main.h
 * @brief Global state declarations shared across the rMesh firmware.
 *
 * Exposes the TX frame buffer, the in-RAM message ring-buffer, the filesystem
 * mutex, and various timer/flag variables that coordinate the main loop with
 * other modules (web UI, OTA update, reporting, …).
 */

#include "config.h"
#include "mesh/frame.h"
#include "bsp/IBoardConfig.h"

#ifdef NRF52_PLATFORM
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#else
#include <freertos/semphr.h>
#endif

/**
 * @brief Lightweight record used in the in-RAM message deduplication ring-buffer.
 *
 * Stores only the source callsign and message ID so that duplicate MESSAGE_FRAMEs
 * can be detected and discarded without keeping the full payload in RAM.
 */
struct MSG {
    char srcCall[MAX_CALLSIGN_LENGTH + 1] = {0}; ///< Null-terminated source callsign.
    uint32_t id = 0;                              ///< Unique message ID from the frame.
};

/** Millisecond timestamp (millis()) at which the device will call ESP.restart(). */
extern uint32_t rebootTimer;
/** Set to true to arm the reboot timer. */
extern bool rebootRequested;

/** Set to true by the web UI to trigger a manual OTA update check in the main loop. */
extern bool pendingManualUpdate;

/** Set to true by saveSettings() to defer initHal() to the loop context. */
extern bool pendingLoraReinit;

/** Set to true by the WebSocket handler to defer saveSettings() to the loop context. */
extern bool pendingSettingsSave;

/** Set to true by the web UI to trigger an immediate deep-sleep shutdown. */
extern bool pendingShutdown;

/** Set to true by the web UI to force an OTA update on a specific channel. */
extern bool pendingForceUpdate;

/** OTA channel index to use when pendingForceUpdate is true. */
extern uint8_t pendingForceChannel;

/** Millisecond deadline for the next 1-second WebSocket status broadcast. */
extern uint32_t statusTimer;

/** Millisecond deadline for the next ANNOUNCE beacon transmission. */
extern uint32_t announceTimer;

/** POSIX timezone string for Central European Time / Summer Time. */
extern const char* TZ_INFO;

/**
 * @brief Outgoing frame queue.
 *
 * Frames are pushed here by processRxFrame(), the announce timer, and the
 * web API.  The main loop drains this buffer, respecting per-frame
 * transmitMillis deadlines and retry counters.
 */
extern std::vector<Frame> txBuffer;

/**
 * @brief FreeRTOS mutex protecting all LittleFS file operations.
 *
 * Must be held (xSemaphoreTake) before any LittleFS open/read/write/close
 * and released (xSemaphoreGive) immediately afterwards.
 */
extern SemaphoreHandle_t fsMutex;

/**
 * @brief FreeRTOS mutex protecting peerList and routingList access.
 *
 * Must be held when iterating or modifying peerList/routingList from
 * FreeRTOS tasks that run concurrently with the Arduino loop.
 */
extern SemaphoreHandle_t listMutex;

/**
 * @brief FreeRTOS task handle identifying the Arduino loop() task.
 *
 * Used by sendFrame() to detect when it is called from a background task
 * (e.g. the async WebSocket handler) and defer the send to the main loop,
 * avoiding unsynchronised access to txBuffer and the messages ring-buffer.
 */
extern TaskHandle_t mainLoopTaskHandle;

/**
 * @brief In-RAM ring-buffer of recently seen (srcCall, id) pairs.
 *
 * Used for duplicate detection: before writing a received message to flash or
 * forwarding it, the node checks whether the (srcCall, id) tuple is already
 * present in this array.  Size is MAX_STORED_MESSAGES_RAM.
 */
extern MSG messages[MAX_STORED_MESSAGES_RAM];

/** Write head for the messages ring-buffer; wraps around at MAX_STORED_MESSAGES_RAM. */
extern uint16_t messagesHead;

/** Set to true when LittleFS free space is critically low; triggers an immediate trim in the main loop. */
extern volatile bool trimNeeded;

/// Global board configuration — set once in setup() via BoardFactory::create().
extern IBoardConfig* board;
