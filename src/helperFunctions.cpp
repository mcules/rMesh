#include <Arduino.h>
#ifdef NRF52_PLATFORM
#include "platform_nrf52.h"
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#else
#include <LittleFS.h>
#endif

#include "settings.h"
#include "helperFunctions.h"
#include "frame.h"
#include "main.h"
#include "webFunctions.h"
#include "peer.h"
#include "config.h"
#include "routing.h"
#include "logging.h"


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

// void addJSONtoFileTask(void * pvParameters) {
//     FileWriteParams* p = (FileWriteParams*) pvParameters;

//     // Wait until the filesystem is free (max 30 seconds)
//     if (xSemaphoreTake(fsMutex, pdMS_TO_TICKS(30000))) {
//         //Zeilen zählen
//         size_t lineCount = 0;
//         File countFile = LittleFS.open(p->fileName, "r");
//         if (countFile) {
//             while (countFile.available()) {
//                 if (countFile.read() == '\n') lineCount++;
//             }
//             countFile.close();
//         }
//         size_t linesToSkip = (lineCount >= p->maxLines) ? (lineCount - p->maxLines - 1) : 0;

//         File srcFile = LittleFS.open(p->fileName, "r");   
//         File dstFile = LittleFS.open("/temp.json", "w");   
//         //char lineBuffer[2048]; // Puffer für eine Zeile
//         char* lineBuffer = (char*)malloc(2048);
//         size_t currentLine = 0;
//         if (srcFile) {
//             while (srcFile.available()) {
//                 int len = srcFile.readBytesUntil('\n', lineBuffer, 2048);
//                 // Nur Zeilen kopieren, die nach dem Skip-Limit liegen
//                 if (currentLine >= linesToSkip) {
//                     dstFile.write((const uint8_t*)lineBuffer, len);
//                     dstFile.print("\n");
//                 }
//                 currentLine++;
//             }
//             srcFile.close();
//         }
//         free(lineBuffer);
//         lineBuffer = nullptr;    

//         if (p->content != nullptr && p->length > 0) {
//             dstFile.write((const uint8_t*)p->content, p->length);
//             dstFile.print("\n");
//         }
//         dstFile.close();

//         LittleFS.remove(p->fileName);
//         LittleFS.rename("/temp.json", p->fileName);

//         xSemaphoreGive(fsMutex); //freigeben
//     } 

//     free(p->content);
//     delete p;
//     vTaskDelete(NULL);
// }


// ── Single file-write worker task with static ring buffer ────────────────────
// Zero heap allocation: uses a fixed-size ring buffer in BSS instead of
// malloc/free per write.  Each slot holds up to 1024 bytes of content.

#define FW_SLOTS     16
#define FW_CONTENT   1024

struct FileWriteSlot {
    char content[FW_CONTENT];
    char fileName[32];
    size_t length;
};

static FileWriteSlot  fwSlots[FW_SLOTS];
static QueueHandle_t  fwQueue = NULL;  // queue of slot indices (uint8_t)

static void fileWriteWorkerTask(void *) {
    uint8_t idx;
    for (;;) {
        if (xQueueReceive(fwQueue, &idx, portMAX_DELAY) != pdTRUE) continue;
        FileWriteSlot &s = fwSlots[idx];
        if (xSemaphoreTake(fsMutex, pdMS_TO_TICKS(30000))) {
            #ifndef NRF52_PLATFORM
            size_t freeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
            if (freeSpace < FS_MIN_FREE_BYTES) {
                logPrintf(LOG_WARN, "FS", "Low space (%u bytes free) — skipping write, requesting trim", freeSpace);
                trimNeeded = true;
                xSemaphoreGive(fsMutex);
                continue;
            }
            #endif

            File file = LittleFS.open(s.fileName, "a");
            if (file) {
                if (s.length > 0) {
                    file.write((const uint8_t*)s.content, s.length);
                    file.print("\n");
                }
                file.close();
            } else {
                logPrintf(LOG_ERROR, "FS", "Could not open file %s for appending", s.fileName);
            }
            xSemaphoreGive(fsMutex);
        } else {
            logPrintf(LOG_ERROR, "FS", "fsMutex timeout in fileWriteWorker");
        }
    }
}

void initFileWriteWorker() {
    fwQueue = xQueueCreate(FW_SLOTS, sizeof(uint8_t));
    xTaskCreate(fileWriteWorkerTask, "FileWriter", 4096, NULL, 1, NULL);
}

static uint8_t fwNextSlot = 0;

void addJSONtoFile(char* buffer, size_t length, const char* file, const uint16_t lines) {
    if (!fwQueue) return;
    if (length > FW_CONTENT) {
        logPrintf(LOG_WARN, "FS", "addJSONtoFile: content too large (%u > %u)", length, FW_CONTENT);
        length = FW_CONTENT;
    }

    uint8_t idx = fwNextSlot;
    fwNextSlot = (fwNextSlot + 1) % FW_SLOTS;

    FileWriteSlot &s = fwSlots[idx];
    memcpy(s.content, buffer, length);
    s.length = length;
    strncpy(s.fileName, file, sizeof(s.fileName) - 1);
    s.fileName[sizeof(s.fileName) - 1] = '\0';

    if (xQueueSend(fwQueue, &idx, pdMS_TO_TICKS(5000)) != pdTRUE) {
        logPrintf(LOG_ERROR, "FS", "File write queue full after 5s, dropping write");
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


