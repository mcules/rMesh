#include <Arduino.h>
#ifdef NRF52_PLATFORM
#include "util/platform_nrf52.h"
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
// nRF52 FreeRTOS: portENTER/EXIT_CRITICAL take no args (single-core)
#define FW_ENTER_CRITICAL()  portENTER_CRITICAL()
#define FW_EXIT_CRITICAL()   portEXIT_CRITICAL()
#else
#include <LittleFS.h>
#endif

#include "hal/settings.h"
#include "util/helperFunctions.h"
#include "mesh/frame.h"
#include "main.h"
#include "network/webFunctions.h"
#include "mesh/peer.h"
#include "config.h"
#include "mesh/routing.h"
#include "util/logging.h"


// Monotonic message-ID counter, seeded with a random value on boot so that
// IDs don't collide across reboots.  Incremented for every outgoing message.
static uint32_t msgIdCounter = 0;
static bool     msgIdSeeded  = false;

static uint32_t nextMsgId() {
    if (!msgIdSeeded) {
        #ifdef NRF52_PLATFORM
        msgIdCounter = (uint32_t)random(0, INT32_MAX);
        #else
        msgIdCounter = esp_random();
        #endif
        msgIdSeeded = true;
    }
    return ++msgIdCounter;
}

void printHexArray(uint8_t* data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    if (data[i] < 0x10) {
      Serial.print("0"); // Leading zero for values less than 16
    }
    Serial.print(data[i], HEX); 
    Serial.print(" "); // Space for better readability
  }
  Serial.println(); // Newline at the end
}



// ── Deferred send queue ──────────────────────────────────────────────────────
// sendFrame() is not thread-safe (it mutates txBuffer and the messages[]
// ring-buffer without locking).  The async WebSocket handler runs on a
// different FreeRTOS task, so calling sendFrame() from there is a race.
// Instead, background tasks push the prepared Frame into a small queue
// protected by pendingSendMutex; the main loop drains it once per iteration.

static SemaphoreHandle_t pendingSendMutex = NULL;
static std::vector<Frame> pendingSendQueue;

void initPendingSendQueue() {
    pendingSendMutex = xSemaphoreCreateMutex();
    pendingSendQueue.reserve(5);
}

void processPendingSends() {
    if (pendingSendQueue.empty()) return;
    std::vector<Frame> local;
    if (xSemaphoreTake(pendingSendMutex, pdMS_TO_TICKS(50))) {
        local.swap(pendingSendQueue);
        xSemaphoreGive(pendingSendMutex);
    }
    for (auto& frame : local) {
        sendFrame(frame);
    }
}

void sendFrame(Frame &f) {
    // Defer to main loop if called from a background task (e.g. WebSocket)
    if (mainLoopTaskHandle != NULL && xTaskGetCurrentTaskHandle() != mainLoopTaskHandle) {
        if (pendingSendMutex != NULL && xSemaphoreTake(pendingSendMutex, pdMS_TO_TICKS(500))) {
            if (pendingSendQueue.size() < TX_BUFFER_SIZE) {
                pendingSendQueue.push_back(f);
            } else {
                logPrintf(LOG_WARN, "TX", "Pending send queue full, dropping frame");
            }
            xSemaphoreGive(pendingSendMutex);
        }
        return;
    }

    //Send frame
    f.id = nextMsgId();
    f.timestamp = time(NULL);
    f.tx = true;

    //Search for route
    bool routing = false;
    char viaCall[MAX_CALLSIGN_LENGTH + 1] = {0};
    getRoute(f.dstCall, viaCall, MAX_CALLSIGN_LENGTH + 1);    
    if (strlen(viaCall) > 0) { routing = true; }

    // Check if the routed destination is reachable via WiFi
    bool routeViaWifi = false;
    if (routing) {
        for (size_t pi = 0; pi < peerList.size(); pi++) {
            if (strcmp(peerList[pi].nodeCall, viaCall) == 0 &&
                peerList[pi].port == 1 && peerList[pi].available) {
                routeViaWifi = true;
                break;
            }
        }
    }

    for (int port = 1; port >= 0; port--) {
        // Skip LoRa entirely if routed destination is reachable via WiFi (WiFi = primary, RF = fallback)
        if (port == 0 && routeViaWifi) continue;

        uint8_t availableNodeCount = 0;
        f.viaCall[0] = 0;
        f.retry = TX_RETRY;
        f.initRetry = TX_RETRY;
        f.syncFlag = false;
        f.port = port;
        //Send to all peers
        for (int i = 0; i < peerList.size(); i++) {
            if ((peerList[i].available) && (peerList[i].port == port)) {
                if ((routing == false) || (strcmp(peerList[i].nodeCall, viaCall) == 0)) {
                    availableNodeCount++;
                    f.port = peerList[i].port;
                    memcpy(f.viaCall, peerList[i].nodeCall, sizeof(f.viaCall));
                    if (txBuffer.size() >= TX_BUFFER_SIZE) {
                        logPrintf(LOG_WARN, "TX", "Buffer full, dropping frame");
                        continue;
                    }
                    if (txBuffer.size() == 0) {f.syncFlag = true;} else {f.syncFlag = false;}
                    txBuffer.push_back(f);
                }
            }
        }

        //If no peers, send frame without destination and retry (WiFi only if peers are configured)
        #ifdef HAS_WIFI
        bool skipUdpBroadcast = (port == 1 && udpPeers.empty());
        #else
        bool skipUdpBroadcast = (port == 1);
        #endif
        if (availableNodeCount == 0 && !skipUdpBroadcast && txBuffer.size() < TX_BUFFER_SIZE) {
            f.viaCall[0] = 0;
            f.retry = 1;
            f.initRetry = 1;
            f.syncFlag = false;
            f.port = port;
            txBuffer.push_back(f);
        }
    }

    //Store message in ring buffer
    strncpy(messages[messagesHead].srcCall, f.srcCall, MAX_CALLSIGN_LENGTH + 1);
    messages[messagesHead].id = f.id;
    if (++messagesHead >= MAX_STORED_MESSAGES_RAM) { messagesHead = 0; }                        

    //Send message to WebSocket & store
    char jsonBuffer[1024];
    size_t len = f.messageJSON(jsonBuffer, sizeof(jsonBuffer));
    #ifdef HAS_WIFI
    wsBroadcast(jsonBuffer, len);
    #endif
    addJSONtoFile(jsonBuffer, len, "/messages.json", MAX_STORED_MESSAGES);
}

void sendMessage(const char* dst, const char* text, uint8_t messageType) {
    if (strlen(text) == 0) {return;}
    //Build new frame for all peers
    Frame f;
    f.frameType = Frame::FrameTypes::MESSAGE_FRAME;
    f.messageType = messageType;
    strncpy(f.srcCall, settings.mycall, sizeof(f.srcCall));
    safeUtf8Copy((char*)f.dstCall, (uint8_t*)dst, MAX_CALLSIGN_LENGTH, sizeof(f.dstCall));
    safeUtf8Copy((char*)f.message, (uint8_t*)text, sizeof(f.message), sizeof(f.message));
    f.messageLength = strlen((char*)f.message);
    sendFrame(f);
}

void sendGroup(const char* dst, const char* text, uint8_t messageType) {
    if (strlen(text) == 0) {return;}
    //Build new frame for all peers
    Frame f;
    f.frameType = Frame::FrameTypes::MESSAGE_FRAME;
    f.messageType = messageType;
    strncpy(f.srcCall, settings.mycall, sizeof(f.srcCall));
    safeUtf8Copy((char*)f.dstGroup, (uint8_t*)dst, MAX_CALLSIGN_LENGTH, sizeof(f.dstGroup));
    safeUtf8Copy((char*)f.message, (uint8_t*)text, sizeof(f.message), sizeof(f.message));
    f.messageLength = strlen((char*)f.message);
    sendFrame(f);
}

// ── Single file-write worker task with static ring buffer ────────────────────
// Zero heap allocation: fixed-size slot pool in BSS instead of malloc/free.
// Producers (addJSONtoFile) pick a free slot, fill it, and queue its index.
// The worker drains the entire pending queue per fsMutex hold and groups
// writes by filename so each LittleFS file is opened/closed only once per
// batch — flash metadata flush is the dominant cost, so this dramatically
// speeds up bursts and shortens the time the slot pool is occupied.

#define FW_SLOTS     8
#define FW_CONTENT   1024

struct FileWriteSlot {
    char content[FW_CONTENT];
    char fileName[32];
    size_t length;
    volatile bool inUse;   // true between addJSONtoFile() and worker completion
};

static FileWriteSlot  fwSlots[FW_SLOTS];
static QueueHandle_t  fwQueue = NULL;  // queue of slot indices (uint8_t)

// ── Diagnostics counters (exposed via /api/status) ──────────────────────────
// Live count of pending slots and lifetime high-water mark. Updated under a
// short critical section so the watermark is consistent across cores.
#ifndef NRF52_PLATFORM
static portMUX_TYPE fwMux = portMUX_INITIALIZER_UNLOCKED;
// ESP32 FreeRTOS: portENTER/EXIT_CRITICAL require a spinlock arg (multi-core)
#define FW_ENTER_CRITICAL()  portENTER_CRITICAL(&fwMux)
#define FW_EXIT_CRITICAL()   portEXIT_CRITICAL(&fwMux)
#endif
static volatile uint8_t  fwPending     = 0;
static volatile uint8_t  fwMaxPending  = 0;
static volatile uint32_t fwDroppedFull = 0;  // queue full / no slot dropped
static volatile uint32_t fwTotalWrites = 0;  // successful enqueues

uint8_t  fileWriterPending()    { return fwPending; }
uint8_t  fileWriterMaxPending() { return fwMaxPending; }
uint8_t  fileWriterSlotCount()  { return FW_SLOTS; }
uint32_t fileWriterDropped()    { return fwDroppedFull; }
uint32_t fileWriterTotal()      { return fwTotalWrites; }

static inline void fwAccountEnqueue() {
    FW_ENTER_CRITICAL();
    fwPending = fwPending + 1;
    if (fwPending > fwMaxPending) fwMaxPending = fwPending;
    fwTotalWrites = fwTotalWrites + 1;
    FW_EXIT_CRITICAL();
}

static inline void fwAccountDequeue() {
    FW_ENTER_CRITICAL();
    if (fwPending > 0) fwPending = fwPending - 1;
    FW_EXIT_CRITICAL();
}

// Drain queued slot indices into a local array, blocking only on the first
// receive. Returns the number drained (>=1 on success, 0 on shutdown).
static uint8_t drainQueue(uint8_t* out, uint8_t maxN) {
    uint8_t n = 0;
    if (xQueueReceive(fwQueue, &out[n], portMAX_DELAY) != pdTRUE) return 0;
    n++;
    while (n < maxN && xQueueReceive(fwQueue, &out[n], 0) == pdTRUE) n++;
    return n;
}

static void fileWriteWorkerTask(void *) {
    uint8_t indices[FW_SLOTS];
    bool    handled[FW_SLOTS];
    for (;;) {
        uint8_t n = drainQueue(indices, FW_SLOTS);
        if (n == 0) continue;

        if (!xSemaphoreTake(fsMutex, pdMS_TO_TICKS(30000))) {
            logPrintf(LOG_ERROR, "FS", "fsMutex timeout in fileWriteWorker");
            // Release slots so producers don't deadlock; data is lost.
            for (uint8_t i = 0; i < n; i++) {
                fwSlots[indices[i]].inUse = false;
                fwAccountDequeue();
            }
            continue;
        }

        #ifndef NRF52_PLATFORM
        size_t freeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
        if (freeSpace < FS_MIN_FREE_BYTES) {
            logPrintf(LOG_WARN, "FS", "Low space (%u bytes free) — skipping batch, requesting trim", freeSpace);
            trimNeeded = true;
            xSemaphoreGive(fsMutex);
            for (uint8_t i = 0; i < n; i++) {
                fwSlots[indices[i]].inUse = false;
                fwAccountDequeue();
            }
            continue;
        }
        #endif

        // Group by filename: for each unique filename open the file once,
        // append all matching slots in queue order, close. Preserves order
        // within each file. handled[] tracks which indices we already wrote.
        for (uint8_t i = 0; i < n; i++) handled[i] = false;

        for (uint8_t i = 0; i < n; i++) {
            if (handled[i]) continue;
            const char* fname = fwSlots[indices[i]].fileName;
            File file = LittleFS.open(fname, "a");
            if (!file) {
                logPrintf(LOG_ERROR, "FS", "Could not open %s for appending", fname);
                // Skip all slots for this file
                for (uint8_t j = i; j < n; j++) {
                    if (!handled[j] && strcmp(fwSlots[indices[j]].fileName, fname) == 0) {
                        handled[j] = true;
                    }
                }
                continue;
            }
            for (uint8_t j = i; j < n; j++) {
                if (handled[j]) continue;
                FileWriteSlot &sj = fwSlots[indices[j]];
                if (strcmp(sj.fileName, fname) != 0) continue;
                if (sj.length > 0) {
                    file.write((const uint8_t*)sj.content, sj.length);
                    file.print("\n");
                }
                handled[j] = true;
            }
            file.close();
        }

        xSemaphoreGive(fsMutex);

        // Release slots and update accounting
        for (uint8_t i = 0; i < n; i++) {
            fwSlots[indices[i]].inUse = false;
            fwAccountDequeue();
        }
    }
}

void initFileWriteWorker() {
    for (uint8_t i = 0; i < FW_SLOTS; i++) fwSlots[i].inUse = false;
    fwQueue = xQueueCreate(FW_SLOTS, sizeof(uint8_t));
    xTaskCreate(fileWriteWorkerTask, "FileWriter", 4096, NULL, 1, NULL);
}

void addJSONtoFile(char* buffer, size_t length, const char* file, const uint16_t lines) {
    if (!fwQueue) return;
    if (length > FW_CONTENT) {
        logPrintf(LOG_WARN, "FS", "addJSONtoFile: content too large (%u > %u)", length, FW_CONTENT);
        length = FW_CONTENT;
    }

    // Find a free slot. Safe allocator: if all slots are in use the queue
    // is by definition full, so we drop with a logged error instead of
    // overwriting an unprocessed slot (which would silently lose data).
    uint8_t idx = 0xFF;
    FW_ENTER_CRITICAL();
    for (uint8_t i = 0; i < FW_SLOTS; i++) {
        if (!fwSlots[i].inUse) {
            fwSlots[i].inUse = true;
            idx = i;
            break;
        }
    }
    FW_EXIT_CRITICAL();

    if (idx == 0xFF) {
        FW_ENTER_CRITICAL();
        fwDroppedFull = fwDroppedFull + 1;
        FW_EXIT_CRITICAL();
        logPrintf(LOG_ERROR, "FS", "File write slots exhausted (%u/%u), dropping write to %s",
                  (unsigned)fwPending, (unsigned)FW_SLOTS, file);
        return;
    }

    FileWriteSlot &s = fwSlots[idx];
    memcpy(s.content, buffer, length);
    s.length = length;
    strncpy(s.fileName, file, sizeof(s.fileName) - 1);
    s.fileName[sizeof(s.fileName) - 1] = '\0';

    fwAccountEnqueue();

    if (xQueueSend(fwQueue, &idx, pdMS_TO_TICKS(1000)) != pdTRUE) {
        // Should not happen — queue and slot pool are the same size — but
        // be defensive: release the slot and account the drop.
        s.inUse = false;
        fwAccountDequeue();
        FW_ENTER_CRITICAL();
        fwDroppedFull = fwDroppedFull + 1;
        FW_EXIT_CRITICAL();
        logPrintf(LOG_ERROR, "FS", "File write queue send failed for %s", file);
    }
}


void trimFileTask(void * pvParameters) {
    FileWriteParams* p = (FileWriteParams*) pvParameters;

    // Phase 1: Count lines (short mutex hold)
    size_t lineCount = 0;
    if (xSemaphoreTake(fsMutex, pdMS_TO_TICKS(5000))) {
        File countFile = LittleFS.open(p->fileName, "r");
        if (countFile) {
            while (countFile.available()) {
                if (countFile.read() == '\n') lineCount++;
            }
            countFile.close();
        }
        xSemaphoreGive(fsMutex);
    } else {
        free(p);
        vTaskDelete(NULL);
        return;
    }

    if (lineCount <= p->maxLines) {
        free(p);
        vTaskDelete(NULL);
        return;
    }

    // Phase 2: Trim file (separate mutex hold)
    size_t linesToSkip = lineCount - p->maxLines;
    if (xSemaphoreTake(fsMutex, pdMS_TO_TICKS(10000))) {
        File srcFile = LittleFS.open(p->fileName, "r");
        File dstFile = LittleFS.open("/temp_trim.json", "w");

        if (srcFile && dstFile) {
            // Stack buffer (task has 8192 stack) avoids heap malloc/free
            char lineBuffer[1024];
            size_t currentLine = 0;

            while (srcFile.available()) {
                int len = srcFile.readBytesUntil('\n', lineBuffer, sizeof(lineBuffer));

                if (currentLine >= linesToSkip) {
                    dstFile.write((const uint8_t*)lineBuffer, len);
                    dstFile.print("\n");
                }
                currentLine++;
            }

            srcFile.close();
            dstFile.close();

            LittleFS.remove(p->fileName);
            LittleFS.rename("/temp_trim.json", p->fileName);

        } else {
            if (srcFile) srcFile.close();
            if (dstFile) dstFile.close();
        }
        xSemaphoreGive(fsMutex);
    }

    free(p);
    vTaskDelete(NULL);
}


void trimFile(const char* fileName, size_t maxLines) {
    FileWriteParams* p = (FileWriteParams*)malloc(sizeof(FileWriteParams));
    if (p == nullptr) {
        logPrintf(LOG_ERROR, "FS", "trimFile: malloc failed");
        return;
    }
    strncpy(p->fileName, fileName, sizeof(p->fileName) - 1);
    p->fileName[sizeof(p->fileName) - 1] = '\0';
    p->maxLines = (uint16_t)maxLines;
    p->content = nullptr;
    p->length = 0;

    // Start task (slightly lower priority since it's a background job)
    xTaskCreate(trimFileTask, "trimFileTask", 8192, p, 1, NULL);
}




uint32_t getTOA(uint16_t payloadBytes) {
    uint8_t SF  = settings.loraSpreadingFactor; 
    uint32_t BW = settings.loraBandwidth * 1000; 
    uint8_t CR = (settings.loraCodingRate > 4) ? (settings.loraCodingRate - 4) : settings.loraCodingRate;
    if (BW == 0) return 0;
    bool DE = ( ( (1 << SF) * 1000 / BW ) > 16 ); 
    float Tsym = (float)(1 << SF) / (float)BW * 1000.0f;
    float Tpreamble = (settings.loraPreambleLength + 4.25f) * Tsym;
    float payloadBits = 8.0f * payloadBytes - 4.0f * SF + 28.0f + 16.0f; // +16 for CRC
    float bitsPerSymbol = 4.0f * (SF - (DE ? 2 : 0));
    float payloadSymbols = 8.0f + fmaxf(ceilf(payloadBits / bitsPerSymbol) * (CR + 4), 0.0f);
    return (uint32_t)roundf(Tpreamble + (payloadSymbols * Tsym));
}

uint32_t calculateAckTime() {
    uint32_t time = getTOA(10 + 2 * MAX_CALLSIGN_LENGTH); // Time for 1 ACK frame
    time = time * 20;   // 20 ACK frames
    time = random(0, time);
    return time;
}

uint32_t calculateRetryTime() {
    uint32_t time = 20 * getTOA(10 + 2 * MAX_CALLSIGN_LENGTH); // Wait for up to 20 ACK frames
    time = time + random(0, 5 * getTOA(255));  // 0...5 max-length message frames
    return time;
}


static inline bool isCont(uint8_t b) {
    return (b & 0xC0) == 0x80; // 0b10xxxxxx
}

void safeUtf8Copy(char* dest, const uint8_t* src, size_t srcLen, size_t dstSize) {
    size_t d = 0;

    for (size_t i = 0; i < srcLen; ) {
        uint8_t b = src[i];

        if (b == 0x00) break;

        // ASCII
        if (b < 0x80) {
            // JSON: only allowed ASCII characters
            if ((b >= 0x20 || b == '\n' || b == '\r' || b == '\t') && d + 1 < dstSize) {
                dest[d++] = b;
            }
            i++;
            continue;
        }

        // 2-Byte UTF-8
        if (b >= 0xC2 && b <= 0xDF) {
            if (i + 1 < srcLen && d + 2 < dstSize && isCont(src[i+1])) {
                dest[d++] = b;
                dest[d++] = src[i+1];
            }
            i += 2;
            continue;
        }

        // 3-Byte UTF-8
        if (b >= 0xE0 && b <= 0xEF) {
            if (i + 2 < srcLen && d + 3 < dstSize &&
                isCont(src[i+1]) &&
                isCont(src[i+2])) {

                dest[d++] = b;
                dest[d++] = src[i+1];
                dest[d++] = src[i+2];
            }
            i += 3;
            continue;
        }

        // 4-Byte UTF-8
        if (b >= 0xF0 && b <= 0xF4) {
            if (i + 3 < srcLen && d + 4 < dstSize &&
                isCont(src[i+1]) &&
                isCont(src[i+2]) &&
                isCont(src[i+3])) {

                dest[d++] = b;
                dest[d++] = src[i+1];
                dest[d++] = src[i+2];
                dest[d++] = src[i+3];
            }
            i += 4;
            continue;
        }

        // Discard everything else
        i++;
    }

    dest[d] = '\0';
}



void getFormattedTime(const char* format, char* outBuffer, size_t outSize) {
    time_t now = time(NULL);
    struct tm timeinfo;
    
    if (!localtime_r(&now, &timeinfo)) {
        snprintf(outBuffer, outSize, "Time error");
        return;
    }

    strftime(outBuffer, outSize, format, &timeinfo);
}


