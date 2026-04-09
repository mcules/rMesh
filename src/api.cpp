#ifdef HAS_WIFI

#include "api.h"
#include "apiAuth.h"
#include "auth.h"
#include "config.h"
#include "settings.h"
#include "peer.h"
#include "routing.h"
#include "main.h"
#include "helperFunctions.h"
#include "hal.h"
#include "logging.h"
#include "heapdbg.h"
#include "bgWorker.h"

#include <WiFi.h>
#include <LittleFS.h>
#include <cstdarg>
#include <time.h>
#include <esp_sntp.h>
#include <Preferences.h>

#include "wifiFunctions.h"
#include "webFunctions.h"
#include "serial.h"

// ── NTP sync tracking ──────────────────────────────────────────────────────
static uint32_t lastNtpSyncTime = 0;

static void onNtpSync(struct timeval *tv) {
    lastNtpSyncTime = (uint32_t)time(nullptr);
}

// ── Reset counter (NVS) ────────────────────────────────────────────────────
static uint32_t nvsResetCount = 0;

// ── LoRa frame counters ─────────────────────────────────────────────────────
uint32_t apiTxTotal = 0;
uint32_t apiRxTotal = 0;

// Drop counters defined in main.cpp (#14)
extern uint32_t droppedFrames;
extern uint32_t droppedBufferFull;
extern uint32_t droppedRetryExhaust;
extern uint32_t droppedPeerDead;
extern uint32_t droppedMessage;
extern uint32_t droppedAck;
extern uint32_t droppedAnnounce;
extern uint32_t droppedAnnounceAck;
extern uint32_t droppedOther;

// ── Message ring buffer ─────────────────────────────────────────────────────
static ApiMessage msgBuffer[API_MSG_BUFFER_SIZE];
static uint8_t msgHead = 0;
static uint8_t msgCount = 0;

// ── Event ring buffer ───────────────────────────────────────────────────────
static ApiEvent evtBuffer[API_EVT_BUFFER_SIZE];
static uint8_t evtHead = 0;
static uint8_t evtCount = 0;

// ── LittleFS persistence ────────────────────────────────────────────────────
// Only the event ring buffer is persisted to flash. The message ring buffer
// is rebuilt at boot from /messages.json (which is also the WebUI's archive
// source) — no separate /api_msgs.bin file.
volatile bool apiBuffersDirty = false;
static volatile bool apiSaveInProgress = false;
static const char* API_EVTS_FILE = "/api_evts.bin";
// Bump on every ApiEvent struct change so old files are rejected at load
// time and replaced on next save.
static const uint8_t API_FILE_VERSION = 3;

// ── Static JSON build buffer ───────────────────────────────────────────────
// AsyncWebServer dispatches one request handler at a time on the async TCP
// task, so a single shared buffer is safe.  Writing directly into a fixed
// buffer via snprintf eliminates the dozens of intermediate String heap
// allocations that String += causes, preventing heap fragmentation on
// long-running nodes.
// Shared JSON build buffer. Sized for the largest individual endpoint
// response (peers / routes / messages / events with full lists). The
// aggregated /api/poll endpoint was removed to keep this at 10 KB; clients
// fetch the individual endpoints instead.
static char jBuf[10240];
static int  jPos;
// Set when any append could not fit and got dropped. Endpoints can inspect
// this to fail loudly instead of returning malformed JSON.
static bool jOverflow = false;

static void jReset() { jPos = 0; jBuf[0] = '\0'; jOverflow = false; }

__attribute__((format(printf, 1, 2)))
static void jPrintf(const char *fmt, ...) {
    if (jPos >= (int)sizeof(jBuf) - 1) { jOverflow = true; return; }
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(jBuf + jPos, sizeof(jBuf) - jPos, fmt, ap);
    va_end(ap);
    if (n < 0) { jOverflow = true; return; }
    if (jPos + n >= (int)sizeof(jBuf)) {
        // vsnprintf wrote a truncated copy; rewind to last good position.
        jBuf[jPos] = '\0';
        jOverflow = true;
        return;
    }
    jPos += n;
}

// Append a JSON-escaped, double-quoted string directly into jBuf.
static void jStr(const char *s) {
    const int cap = (int)sizeof(jBuf);
    if (jPos + 2 >= cap) { jOverflow = true; return; }
    jBuf[jPos++] = '"';
    for (const char *p = s; *p; p++) {
        if (jPos + 7 >= cap) { jOverflow = true; break; }   // room for \uXXXX + closing "
        switch (*p) {
            case '"':  jBuf[jPos++] = '\\'; jBuf[jPos++] = '"';  break;
            case '\\': jBuf[jPos++] = '\\'; jBuf[jPos++] = '\\'; break;
            case '\n': jBuf[jPos++] = '\\'; jBuf[jPos++] = 'n';  break;
            case '\r': jBuf[jPos++] = '\\'; jBuf[jPos++] = 'r';  break;
            case '\t': jBuf[jPos++] = '\\'; jBuf[jPos++] = 't';  break;
            default:
                if ((uint8_t)*p < 0x20)
                    jPos += snprintf(jBuf + jPos, cap - jPos, "\\u%04x", (uint8_t)*p);
                else
                    jBuf[jPos++] = *p;
        }
    }
    jBuf[jPos++] = '"';
    jBuf[jPos] = '\0';
}

// Send jBuf using AsyncProgmemResponse which reads directly from the source
// pointer without any heap allocation (no String copy, no chunked buffer).
// On ESP32, memcpy_P == memcpy, so reading from static RAM works fine.

static void jSend(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *resp = request->beginResponse(
        200, "application/json", (const uint8_t *)jBuf, (size_t)jPos);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    request->send(resp);
}

// ── Ring buffer record functions ────────────────────────────────────────────

void apiRecordMessage(const Frame &f, bool isTx) {
    ApiMessage &m = msgBuffer[msgHead];
    memset(&m, 0, sizeof(m));
    m.id = f.id;
    m.time = (uint32_t)f.timestamp;
    strlcpy(m.src, f.srcCall, sizeof(m.src));
    strlcpy(m.group, f.dstGroup, sizeof(m.group));
    if (f.messageLength > 0 && (f.messageType == Frame::MessageTypes::TEXT_MESSAGE ||
                                 f.messageType == Frame::MessageTypes::TRACE_MESSAGE)) {
        size_t len = f.messageLength < sizeof(m.text) - 1 ? f.messageLength : sizeof(m.text) - 1;
        memcpy(m.text, f.message, len);
        m.text[len] = '\0';
    }
    if (!isTx) {
        strlcpy(m.via, f.nodeCall, sizeof(m.via));
    }
    m.hops = f.hopCount;
    m.rssi = isTx ? 0 : (int16_t)f.rssi;
    m.snr = isTx ? 0 : (int8_t)f.snr;
    m.dir = isTx ? 1 : 0;
    m.acked = false;

    msgHead = (msgHead + 1) % API_MSG_BUFFER_SIZE;
    if (msgCount < API_MSG_BUFFER_SIZE) msgCount++;
    // Note: messages are not persisted to a separate .bin file — they live
    // in /messages.json and are reseeded from there at boot.
}

static void pushEvent(const ApiEvent &e) {
    evtBuffer[evtHead] = e;
    evtHead = (evtHead + 1) % API_EVT_BUFFER_SIZE;
    if (evtCount < API_EVT_BUFFER_SIZE) evtCount++;
    apiBuffersDirty = true;
}

void apiRecordRxEvent(const Frame &f) {
    apiRxTotal++;
    ApiEvent e;
    memset(&e, 0, sizeof(e));
    e.time = (uint32_t)f.timestamp;
    e.eventType = API_EVT_RX;
    e.frameType = f.frameType;
    strlcpy(e.nodeCall, f.nodeCall, sizeof(e.nodeCall));
    strlcpy(e.viaCall, f.viaCall, sizeof(e.viaCall));
    strlcpy(e.srcCall, f.srcCall, sizeof(e.srcCall));
    e.id = f.id;
    e.rssi = (int16_t)f.rssi;
    e.snr = (int8_t)f.snr;
    e.port = f.port;
    pushEvent(e);
}

void apiRecordTxEvent(const Frame &f) {
    apiTxTotal++;
    ApiEvent e;
    memset(&e, 0, sizeof(e));
    e.time = (uint32_t)time(nullptr);
    e.eventType = API_EVT_TX;
    e.frameType = f.frameType;
    strlcpy(e.nodeCall, f.nodeCall, sizeof(e.nodeCall));
    strlcpy(e.viaCall, f.viaCall, sizeof(e.viaCall));
    strlcpy(e.srcCall, f.srcCall, sizeof(e.srcCall));
    e.id = f.id;
    e.port = f.port;
    pushEvent(e);
}

void apiRecordAckEvent(const Frame &f) {
    ApiEvent e;
    memset(&e, 0, sizeof(e));
    e.time = (uint32_t)f.timestamp;
    e.eventType = API_EVT_ACK;
    strlcpy(e.srcCall, f.srcCall, sizeof(e.srcCall));
    strlcpy(e.nodeCall, f.nodeCall, sizeof(e.nodeCall));
    e.id = f.id;
    pushEvent(e);
}

void apiMarkMessageAcked(const char* srcCall, uint32_t id) {
    // Scan the ring buffer for matching message
    for (uint8_t i = 0; i < msgCount; i++) {
        uint8_t idx = (msgHead - msgCount + i + API_MSG_BUFFER_SIZE) % API_MSG_BUFFER_SIZE;
        if (msgBuffer[idx].id == id && strcmp(msgBuffer[idx].src, srcCall) == 0) {
            msgBuffer[idx].acked = true;
            // Not persisted — ack state is rebuilt at boot from ack.json.
            return;
        }
    }
}

// ── LittleFS persistence (load + save) ─────────────────────────────────────
// The buffers are persisted so multiple REST clients can each fetch the
// full live tail without destroying it for the others, and so the tail
// survives reboots.

// Seed the in-RAM message ring buffer from /messages.json. Called at boot
// from apiLoadBuffers(). Reads JSONL lines, keeps the last API_MSG_BUFFER_SIZE
// (the ring buffer wraps automatically). Caller must hold fsMutex.
static void loadMessagesFromJson() {
    File f = LittleFS.open("/messages.json", "r");
    if (!f) return;

    int loaded = 0;
    char line[1024];
    while (f.available()) {
        size_t n = f.readBytesUntil('\n', line, sizeof(line) - 1);
        if (n == 0) continue;
        line[n] = '\0';

        JsonDocument doc;
        if (deserializeJson(doc, line)) continue;
        JsonObject m = doc["message"];
        if (m.isNull()) continue;

        // Only text/trace messages — matches what apiRecordMessage stores
        uint8_t mt = m["messageType"] | 0xFF;
        if (mt != 0 && mt != 1) continue;

        ApiMessage &msg = msgBuffer[msgHead];
        memset(&msg, 0, sizeof(msg));
        msg.id   = m["id"]        | 0;
        msg.time = m["timestamp"] | 0;
        strlcpy(msg.src,   m["srcCall"]  | "", sizeof(msg.src));
        strlcpy(msg.group, m["dstGroup"] | "", sizeof(msg.group));
        const char* txt = m["text"] | "";
        strlcpy(msg.text, txt, sizeof(msg.text));
        msg.hops  = m["hopCount"] | 0;
        msg.dir   = (m["tx"] | false) ? 1 : 0;
        msg.acked = false;
        // via / rssi / snr are not present in messages.json — left as 0

        msgHead = (msgHead + 1) % API_MSG_BUFFER_SIZE;
        if (msgCount < API_MSG_BUFFER_SIZE) msgCount++;
        loaded++;
    }
    f.close();

    if (loaded > 0) {
        logPrintf(LOG_INFO, "API", "Loaded %d messages from /messages.json (kept last %d)",
                  loaded, msgCount);
    }
}

void apiLoadBuffers() {
    if (!xSemaphoreTake(fsMutex, pdMS_TO_TICKS(5000))) {
        logPrintf(LOG_ERROR, "API", "fsMutex timeout in apiLoadBuffers");
        return;
    }
    // Messages: always reseed from /messages.json (no separate .bin)
    loadMessagesFromJson();

    // Events: load from /api_evts.bin if present and version matches
    File g = LittleFS.open(API_EVTS_FILE, "r");
    if (g) {
        uint8_t v = 0; g.read(&v, 1);
        if (v == API_FILE_VERSION) {
            uint16_t n = 0; g.read((uint8_t*)&n, 2);
            if (n > API_EVT_BUFFER_SIZE) n = API_EVT_BUFFER_SIZE;
            evtCount = 0; evtHead = 0;
            for (uint16_t i = 0; i < n; i++) {
                if (g.read((uint8_t*)&evtBuffer[evtHead], sizeof(ApiEvent)) != sizeof(ApiEvent)) break;
                evtHead = (evtHead + 1) % API_EVT_BUFFER_SIZE;
                if (evtCount < API_EVT_BUFFER_SIZE) evtCount++;
            }
            logPrintf(LOG_INFO, "API", "Loaded %d events from %s", evtCount, API_EVTS_FILE);
        }
        g.close();
    }

    // Clean up the now-obsolete /api_msgs.bin from older firmware revisions.
    if (LittleFS.exists("/api_msgs.bin")) {
        LittleFS.remove("/api_msgs.bin");
        logPrintf(LOG_INFO, "API", "Removed obsolete /api_msgs.bin");
    }

    apiBuffersDirty = false;
    xSemaphoreGive(fsMutex);
}

static void apiSaveBuffersWork() {
    if (!xSemaphoreTake(fsMutex, pdMS_TO_TICKS(30000))) {
        logPrintf(LOG_ERROR, "API", "fsMutex timeout in apiSaveBuffers");
        apiSaveInProgress = false;
        return;
    }
    File g = LittleFS.open(API_EVTS_FILE, "w");
    if (g) {
        g.write(&API_FILE_VERSION, 1);
        uint16_t n = evtCount;
        g.write((uint8_t*)&n, 2);
        for (uint8_t i = 0; i < evtCount; i++) {
            uint8_t idx = (evtHead - evtCount + i + API_EVT_BUFFER_SIZE) % API_EVT_BUFFER_SIZE;
            g.write((uint8_t*)&evtBuffer[idx], sizeof(ApiEvent));
        }
        g.close();
    }
    xSemaphoreGive(fsMutex);
    apiBuffersDirty = false;
    apiSaveInProgress = false;
    logPrintf(LOG_DEBUG, "API", "Persisted %d events", evtCount);
}

void apiSaveBuffers() {
    if (apiSaveInProgress) return;
    apiSaveInProgress = true;
    if (!bgWorkerEnqueue(apiSaveBuffersWork)) {
        logPrintf(LOG_WARN, "API", "apiSaveBuffers enqueue failed");
        apiSaveInProgress = false;
    }
}

// ── Helper: get board name string ───────────────────────────────────────────
static const char* getBoardName() {
    return PIO_ENV_NAME;
}

// ── Helper: get reset reason (ESP32 only) ───────────────────────────────────
#ifndef NRF52_PLATFORM
static const char* apiGetResetReason() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:   return "power-on";
        case ESP_RST_SW:        return "software";
        case ESP_RST_PANIC:     return "panic/crash";
        case ESP_RST_INT_WDT:   return "interrupt-watchdog";
        case ESP_RST_TASK_WDT:  return "task-watchdog";
        case ESP_RST_WDT:       return "other-watchdog";
        case ESP_RST_DEEPSLEEP: return "deep-sleep";
        case ESP_RST_BROWNOUT:  return "brownout";
        default:                return "unknown";
    }
}
#endif

// ── Heap guard ──────────────────────────────────────────────────────────────
static bool heapGuard(AsyncWebServerRequest *request) {
    if (ESP.getFreeHeap() < 20000 || ESP.getMaxAllocHeap() < 10000) {
        request->send(503, "application/json", "{\"error\":\"heap too low\"}");
        return false;
    }
    return true;
}

// ── JSON section builders (write into jBuf without jReset/jSend) ────────────

static void buildStatus() {
    jPrintf("\"call\":"); jStr(settings.mycall);
    jPrintf(",\"version\":"); jStr(VERSION);
    jPrintf(",\"board\":"); jStr(getBoardName());
    jPrintf(",\"uptime\":%lu,\"heap\":%u", millis() / 1000, ESP.getFreeHeap());
#ifndef NRF52_PLATFORM
    jPrintf(",\"resetReason\":\"%s\"", apiGetResetReason());
#endif
    jPrintf(",\"wifi\":{\"connected\":%s", WiFi.isConnected() ? "true" : "false");
    if (WiFi.isConnected()) {
        jPrintf(",\"rssi\":%d,\"ip\":\"%s\"", WiFi.RSSI(), WiFi.localIP().toString().c_str());
    }
    jPrintf("}");
    jPrintf(",\"lora\":{\"txQueue\":%u,\"txTotal\":%lu,\"rxTotal\":%lu,\"dropped\":%lu",
            (unsigned)txBuffer.size(), (unsigned long)apiTxTotal, (unsigned long)apiRxTotal,
            (unsigned long)droppedFrames);
    jPrintf(",\"droppedBy\":{\"bufferFull\":%lu,\"retryExhaust\":%lu,\"peerDead\":%lu,"
            "\"msg\":%lu,\"ack\":%lu,\"ann\":%lu,\"annAck\":%lu,\"other\":%lu}}",
            (unsigned long)droppedBufferFull, (unsigned long)droppedRetryExhaust, (unsigned long)droppedPeerDead,
            (unsigned long)droppedMessage, (unsigned long)droppedAck,
            (unsigned long)droppedAnnounce, (unsigned long)droppedAnnounceAck, (unsigned long)droppedOther);
    jPrintf(",\"peers\":%u,\"routes\":%u", (unsigned)peerList.size(), (unsigned)routingList.size());
    jPrintf(",\"time\":%ld", (long)time(nullptr));
}

static void buildPeers() {
    jPrintf("\"peers\":[");
    for (size_t i = 0; i < peerList.size(); i++) {
        if (i > 0) jPrintf(",");
        const Peer &p = peerList[i];
        bool dualPath = false;
        for (size_t j = 0; j < peerList.size(); j++) {
            if (i != j && strcmp(peerList[i].nodeCall, peerList[j].nodeCall) == 0
                       && peerList[i].port != peerList[j].port) {
                dualPath = true;
                break;
            }
        }
        jPrintf("{\"call\":"); jStr(p.nodeCall);
        jPrintf(",\"port\":%u,\"timestamp\":%ld,\"rssi\":%.1f,\"snr\":%.1f,"
                "\"frqError\":%.1f,\"available\":%s",
                p.port, (long)p.timestamp, p.rssi, p.snr, p.frqError,
                p.available ? "true" : "false");
        if (dualPath) {
            jPrintf(",\"preferred\":%s", p.available ? "true" : "false");
        }
        jPrintf("}");
        if (ESP.getFreeHeap() < 20000) break;
    }
    jPrintf("]");
}

static void buildRoutes() {
    jPrintf("\"routes\":[");
    for (size_t i = 0; i < routingList.size(); i++) {
        if (i > 0) jPrintf(",");
        const Route &rt = routingList[i];
        jPrintf("{\"srcCall\":"); jStr(rt.srcCall);
        jPrintf(",\"viaCall\":"); jStr(rt.viaCall);
        jPrintf(",\"timestamp\":%ld,\"hopCount\":%u}", (long)rt.timestamp, rt.hopCount);
        if (ESP.getFreeHeap() < 20000) break;
    }
    jPrintf("]");
}

static void buildMessages(uint32_t since, int limit, const char* groupFilter) {
    jPrintf("\"messages\":[");
    int count = 0;
    for (int i = msgCount - 1; i >= 0 && count < limit; i--) {
        uint8_t idx = (msgHead - msgCount + i + API_MSG_BUFFER_SIZE) % API_MSG_BUFFER_SIZE;
        const ApiMessage &m = msgBuffer[idx];
        if (m.time <= since) continue;
        if (groupFilter && strcmp(m.group, groupFilter) != 0) continue;
        if (count > 0) jPrintf(",");
        jPrintf("{\"id\":%lu,\"time\":%lu,\"src\":", (unsigned long)m.id, (unsigned long)m.time);
        jStr(m.src);
        jPrintf(",\"group\":"); jStr(m.group);
        jPrintf(",\"text\":"); jStr(m.text);
        if (m.via[0] != '\0') {
            jPrintf(",\"via\":"); jStr(m.via);
        } else {
            jPrintf(",\"via\":null");
        }
        jPrintf(",\"hops\":%u,\"rssi\":%d,\"snr\":%d,\"dir\":\"%s\",\"ack\":%s}",
                m.hops, m.rssi, (int)m.snr,
                m.dir == 1 ? "tx" : "rx",
                m.acked ? "true" : "false");
        count++;
        if (ESP.getFreeHeap() < 20000) break;
    }
    jPrintf("]");
}

static const char* apiEventTypeName(uint8_t t) {
    switch (t) {
        case API_EVT_RX:  return "rx";
        case API_EVT_TX:  return "tx";
        case API_EVT_ACK: return "ack";
        default:          return "?";
    }
}

static void buildEvents(uint32_t since, int limit, const char* typeFilter) {
    jPrintf("\"events\":[");
    int count = 0;
    for (int i = evtCount - 1; i >= 0 && count < limit; i--) {
        uint8_t idx = (evtHead - evtCount + i + API_EVT_BUFFER_SIZE) % API_EVT_BUFFER_SIZE;
        const ApiEvent &e = evtBuffer[idx];
        if (e.time <= since) continue;
        const char* evName = apiEventTypeName(e.eventType);
        if (typeFilter && strcmp(evName, typeFilter) != 0) continue;
        if (count > 0) jPrintf(",");
        jPrintf("{\"time\":%lu,\"event\":\"%s\"", (unsigned long)e.time, evName);
        if (e.eventType == API_EVT_RX || e.eventType == API_EVT_TX) {
            jPrintf(",\"frameType\":%u", e.frameType);
            if (e.nodeCall[0]) { jPrintf(",\"nodeCall\":"); jStr(e.nodeCall); }
            if (e.viaCall[0])  { jPrintf(",\"viaCall\":");  jStr(e.viaCall); }
            if (e.srcCall[0])  { jPrintf(",\"srcCall\":");  jStr(e.srcCall); }
            jPrintf(",\"id\":%lu", (unsigned long)e.id);
            if (e.eventType == API_EVT_RX) {
                jPrintf(",\"rssi\":%d,\"snr\":%d", e.rssi, (int)e.snr);
            }
            jPrintf(",\"port\":%u", e.port);
        } else if (e.eventType == API_EVT_ACK) {
            if (e.srcCall[0])  { jPrintf(",\"srcCall\":"); jStr(e.srcCall); }
            if (e.nodeCall[0]) { jPrintf(",\"nodeCall\":"); jStr(e.nodeCall); }
            jPrintf(",\"id\":%lu", (unsigned long)e.id);
        }
        jPrintf("}");
        count++;
        if (ESP.getFreeHeap() < 20000) break;
    }
    jPrintf("]");
}

static void buildGroups() {
    jPrintf("\"groups\":[");
    int count = 0;
    for (int i = 1; i <= MAX_CHANNELS; i++) {
        if (groupNames[i][0] == '\0') continue;
        if (count > 0) jPrintf(",");
        jPrintf("{\"name\":"); jStr(groupNames[i]);
        jPrintf(",\"id\":%d,\"mode\":\"rw\"}", i);
        count++;
    }
    jPrintf("]");
}

// ── Endpoint handlers (thin wrappers around builders) ──────────────────────

static void handleStatus(AsyncWebServerRequest *request) {
    if (!checkApiAuth(request)) return;
    if (!heapGuard(request)) return;
    jReset();
    jPrintf("{"); buildStatus(); jPrintf("}");
    jSend(request);
}

static void handlePeers(AsyncWebServerRequest *request) {
    if (!checkApiAuth(request)) return;
    if (!heapGuard(request)) return;
    jReset();
    jPrintf("{"); buildPeers(); jPrintf("}");
    jSend(request);
}

static void handleRoutes(AsyncWebServerRequest *request) {
    if (!checkApiAuth(request)) return;
    if (!heapGuard(request)) return;
    jReset();
    jPrintf("{"); buildRoutes(); jPrintf("}");
    jSend(request);
}

static void handleGetMessages(AsyncWebServerRequest *request) {
    if (!checkApiAuth(request)) return;
    if (!heapGuard(request)) return;

    const char* groupFilter = nullptr;
    uint32_t since = 0;
    int limit = 20;
    if (request->hasParam("group")) groupFilter = request->getParam("group")->value().c_str();
    if (request->hasParam("since")) since = request->getParam("since")->value().toInt();
    if (request->hasParam("limit")) {
        limit = request->getParam("limit")->value().toInt();
        if (limit < 1) limit = 1;
        if (limit > API_MSG_BUFFER_SIZE) limit = API_MSG_BUFFER_SIZE;
    }
    // Note: `ack` parameter is silently accepted but ignored — buffers are
    // shared across multiple REST clients and must not be purged on read.

    jReset();
    jPrintf("{"); buildMessages(since, limit, groupFilter); jPrintf("}");
    jSend(request);
}

static void handlePostMessage(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkApiAuth(request)) return;

    // ArduinoJson to parse the body
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        request->send(400, "application/json", "{\"error\":\"invalid json\"}");
        return;
    }

    const char* group = doc["group"] | "";
    const char* text = doc["text"] | "";

    if (strlen(group) == 0 || strlen(text) == 0) {
        request->send(400, "application/json", "{\"error\":\"group and text required\"}");
        return;
    }

    // Build and send the frame
    Frame f;
    f.frameType = Frame::FrameTypes::MESSAGE_FRAME;
    f.messageType = Frame::MessageTypes::TEXT_MESSAGE;
    strlcpy(f.srcCall, settings.mycall, sizeof(f.srcCall));
    safeUtf8Copy((char*)f.dstGroup, (const uint8_t*)group, MAX_CALLSIGN_LENGTH, sizeof(f.dstGroup));
    safeUtf8Copy((char*)f.message, (const uint8_t*)text, sizeof(f.message), sizeof(f.message));
    f.messageLength = strlen((char*)f.message);
    f.tx = true;

    // Record in API ring buffer before sending
    apiRecordMessage(f, true);

    sendFrame(f);

    char resp[64];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"id\":%lu}", (unsigned long)f.id);
    request->send(200, "application/json", resp);
}

// One-shot client → node migration: a browser whose localStorage holds
// messages that were never persisted on the node (e.g. from before the
// server-side persistence was added) can POST them here. Duplicates are
// detected by (srcCall, id) and skipped.
static void handleImportMessages(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkApiAuth(request)) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        request->send(400, "application/json", "{\"error\":\"invalid json\"}");
        return;
    }
    JsonArray arr = doc["messages"].as<JsonArray>();
    if (arr.isNull()) {
        request->send(400, "application/json", "{\"error\":\"messages array required\"}");
        return;
    }

    // Open /messages.json directly under fsMutex and append all imported
    // lines synchronously. Going through addJSONtoFile() would push them
    // through a 16-slot queue with a non-blocking round-robin allocator
    // that overwrites unprocessed slots when the queue fills up — exactly
    // what happens during a bulk import of 100+ messages from a browser.
    File msgFile;
    bool fsLocked = false;
    if (xSemaphoreTake(fsMutex, pdMS_TO_TICKS(5000))) {
        fsLocked = true;
        msgFile = LittleFS.open("/messages.json", "a");
    }

    int imported = 0, skipped = 0;
    for (JsonObject m : arr) {
        uint8_t mt = m["messageType"] | 0xFF;
        if (mt != 0 && mt != 1) { skipped++; continue; }

        uint32_t mid = m["id"] | 0;
        const char* src = m["srcCall"] | "";
        if (mid == 0 || src[0] == '\0') { skipped++; continue; }

        // Duplicate check against current ring buffer
        bool dup = false;
        for (uint8_t i = 0; i < msgCount; i++) {
            uint8_t idx = (msgHead - msgCount + i + API_MSG_BUFFER_SIZE) % API_MSG_BUFFER_SIZE;
            if (msgBuffer[idx].id == mid && strcmp(msgBuffer[idx].src, src) == 0) {
                dup = true; break;
            }
        }
        if (dup) { skipped++; continue; }

        ApiMessage &msg = msgBuffer[msgHead];
        memset(&msg, 0, sizeof(msg));
        msg.id   = mid;
        msg.time = m["timestamp"] | 0;
        strlcpy(msg.src,   src, sizeof(msg.src));
        strlcpy(msg.group, m["dstGroup"] | "", sizeof(msg.group));
        strlcpy(msg.text,  m["text"]     | "", sizeof(msg.text));
        msg.hops  = m["hopCount"] | 0;
        msg.dir   = (m["tx"] | false) ? 1 : 0;
        msg.acked = false;

        msgHead = (msgHead + 1) % API_MSG_BUFFER_SIZE;
        if (msgCount < API_MSG_BUFFER_SIZE) msgCount++;

        // Also append to /messages.json (durable 1000-entry archive that the
        // web client reads on init). Direct write under fsMutex — see comment
        // above the loop for why we do not use addJSONtoFile() here.
        if (msgFile) {
            JsonDocument out;
            JsonObject mo = out["message"].to<JsonObject>();
            mo["text"]        = msg.text;
            mo["messageType"] = mt;
            mo["dstCall"]     = "";
            mo["dstGroup"]    = msg.group;
            mo["srcCall"]     = msg.src;
            mo["id"]          = msg.id;
            mo["tx"]          = (msg.dir != 0);
            mo["timestamp"]   = msg.time;
            mo["hopCount"]    = msg.hops;
            char line[1024];
            size_t ln = serializeJson(out, line, sizeof(line));
            if (ln > 0 && ln < sizeof(line)) {
                msgFile.write((const uint8_t*)line, ln);
                msgFile.print("\n");
            }
        }

        imported++;
    }

    if (msgFile) msgFile.close();
    if (fsLocked) xSemaphoreGive(fsMutex);

    // Trim if we exceeded the durable cap (async, queued — single trim, fine).
    if (imported > 0) {
        trimFile("/messages.json", MAX_STORED_MESSAGES);
    }

    char resp[96];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"imported\":%d,\"skipped\":%d}", imported, skipped);
    request->send(200, "application/json", resp);
}

static void handleGroups(AsyncWebServerRequest *request) {
    if (!checkApiAuth(request)) return;
    if (!heapGuard(request)) return;
    jReset();
    jPrintf("{"); buildGroups(); jPrintf("}");
    jSend(request);
}

static void handleEvents(AsyncWebServerRequest *request) {
    if (!checkApiAuth(request)) return;
    if (!heapGuard(request)) return;

    const char* typeFilter = nullptr;
    uint32_t since = 0;
    int limit = 50;
    if (request->hasParam("type")) typeFilter = request->getParam("type")->value().c_str();
    if (request->hasParam("since")) since = request->getParam("since")->value().toInt();
    if (request->hasParam("limit")) {
        limit = request->getParam("limit")->value().toInt();
        if (limit < 1) limit = 1;
        if (limit > API_EVT_BUFFER_SIZE) limit = API_EVT_BUFFER_SIZE;
    }
    // `ack` parameter accepted but ignored (see /api/messages above).

    jReset();
    jPrintf("{"); buildEvents(since, limit, typeFilter); jPrintf("}");
    jSend(request);
}

// ── Settings endpoint ───────────────────────────────────────────────────────

static void handleSettings(AsyncWebServerRequest *request) {
    if (!checkApiAuth(request)) return;

    if (ESP.getFreeHeap() < 15000) {
        request->send(503, "application/json", "{\"error\":\"heap too low\"}");
        return;
    }

    jReset();
    jPrintf("{\"settings\":{");

    // Basic info
    jPrintf("\"mycall\":"); jStr(settings.mycall);
    jPrintf(",\"position\":"); jStr(settings.position);
    jPrintf(",\"ntp\":"); jStr(settings.ntpServer);

    // Device info
    jPrintf(",\"version\":"); jStr(VERSION);
    jPrintf(",\"name\":"); jStr(NAME);
    jPrintf(",\"hardware\":"); jStr(PIO_ENV_NAME);

    // Chip ID
    {
        uint64_t mac = ESP.getEfuseMac();
        char chipId[13];
        snprintf(chipId, sizeof(chipId), "%02X%02X%02X%02X%02X%02X",
            (uint8_t)(mac >> 40), (uint8_t)(mac >> 32), (uint8_t)(mac >> 24),
            (uint8_t)(mac >> 16), (uint8_t)(mac >> 8), (uint8_t)(mac));
        jPrintf(",\"chipId\":"); jStr(chipId);
    }

    jPrintf(",\"webPasswordSet\":%s", !webPasswordHash.isEmpty() ? "true" : "false");

    // WiFi settings
    jPrintf(",\"dhcpActive\":%s,\"apMode\":%s",
            settings.dhcpActive ? "true" : "false",
            settings.apMode ? "true" : "false");

    jPrintf(",\"wifiSSID\":"); jStr(settings.wifiSSID);
    jPrintf(",\"wifiPassword\":"); jStr((settings.wifiPassword[0] != '\0') ? "***" : "");

    jPrintf(",\"apName\":"); jStr(apName.c_str());
    jPrintf(",\"apPassword\":"); jStr(apPassword.c_str());

    // WiFi networks array
    jPrintf(",\"wifiNetworks\":[");
    for (size_t i = 0; i < wifiNetworks.size(); i++) {
        if (i > 0) jPrintf(",");
        jPrintf("{\"ssid\":"); jStr(wifiNetworks[i].ssid);
        jPrintf(",\"password\":"); jStr((wifiNetworks[i].password[0] != '\0') ? "***" : "");
        jPrintf(",\"favorite\":%s}", wifiNetworks[i].favorite ? "true" : "false");
    }
    jPrintf("]");

    // Static IP addresses
    jPrintf(",\"wifiIP\":[%u,%u,%u,%u]",
            settings.wifiIP[0], settings.wifiIP[1], settings.wifiIP[2], settings.wifiIP[3]);
    jPrintf(",\"wifiNetMask\":[%u,%u,%u,%u]",
            settings.wifiNetMask[0], settings.wifiNetMask[1], settings.wifiNetMask[2], settings.wifiNetMask[3]);
    jPrintf(",\"wifiGateway\":[%u,%u,%u,%u]",
            settings.wifiGateway[0], settings.wifiGateway[1], settings.wifiGateway[2], settings.wifiGateway[3]);
    jPrintf(",\"wifiDNS\":[%u,%u,%u,%u]",
            settings.wifiDNS[0], settings.wifiDNS[1], settings.wifiDNS[2], settings.wifiDNS[3]);
    jPrintf(",\"wifiBrodcast\":[%u,%u,%u,%u]",
            settings.wifiBrodcast[0], settings.wifiBrodcast[1], settings.wifiBrodcast[2], settings.wifiBrodcast[3]);

    // Current IP (if connected)
    if (WiFi.status() == WL_CONNECTED) {
        jPrintf(",\"currentIP\":[%u,%u,%u,%u]",
                WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
        jPrintf(",\"currentNetMask\":[%u,%u,%u,%u]",
                WiFi.subnetMask()[0], WiFi.subnetMask()[1], WiFi.subnetMask()[2], WiFi.subnetMask()[3]);
        jPrintf(",\"currentGateway\":[%u,%u,%u,%u]",
                WiFi.gatewayIP()[0], WiFi.gatewayIP()[1], WiFi.gatewayIP()[2], WiFi.gatewayIP()[3]);
        jPrintf(",\"currentDNS\":[%u,%u,%u,%u]",
                WiFi.dnsIP()[0], WiFi.dnsIP()[1], WiFi.dnsIP()[2], WiFi.dnsIP()[3]);
    }

    // LoRa settings
    jPrintf(",\"loraFrequency\":%.4f,\"loraOutputPower\":%d,\"loraBandwidth\":%.1f",
            settings.loraFrequency, (int)settings.loraOutputPower, settings.loraBandwidth);
    jPrintf(",\"loraSyncWord\":%u,\"loraCodingRate\":%u,\"loraSpreadingFactor\":%u",
            (unsigned)settings.loraSyncWord, (unsigned)settings.loraCodingRate, (unsigned)settings.loraSpreadingFactor);
    jPrintf(",\"loraPreambleLength\":%d,\"loraRepeat\":%s,\"loraMaxMessageLength\":%u",
            (int)settings.loraPreambleLength, settings.loraRepeat ? "true" : "false",
            (unsigned)settings.loraMaxMessageLength);
    jPrintf(",\"loraEnabled\":%s", loraEnabled ? "true" : "false");

    // UDP peers array
    jPrintf(",\"udpPeers\":[");
    for (size_t i = 0; i < udpPeers.size(); i++) {
        if (i > 0) jPrintf(",");
        jPrintf("{\"ip\":[%u,%u,%u,%u],\"legacy\":%s,\"enabled\":%s,\"call\":",
                udpPeers[i][0], udpPeers[i][1], udpPeers[i][2], udpPeers[i][3],
                udpPeerLegacy[i] ? "true" : "false",
                udpPeerEnabled[i] ? "true" : "false");
        jStr((i < udpPeerCall.size()) ? udpPeerCall[i].c_str() : "");
        jPrintf("}");
    }
    jPrintf("]");

    // Hop limits and SNR
    jPrintf(",\"maxHopMessage\":%u,\"maxHopPosition\":%u,\"maxHopTelemetry\":%u,\"minSnr\":%d",
            (unsigned)extSettings.maxHopMessage, (unsigned)extSettings.maxHopPosition,
            (unsigned)extSettings.maxHopTelemetry, (int)extSettings.minSnr);

    // Misc settings
    jPrintf(",\"updateChannel\":%u", (unsigned)updateChannel);

#ifdef HAS_BATTERY_ADC
    jPrintf(",\"hasBattery\":true");
#else
    jPrintf(",\"hasBattery\":false");
#endif
    jPrintf(",\"batteryEnabled\":%s,\"batteryFullVoltage\":%.1f",
            batteryEnabled ? "true" : "false", batteryFullVoltage);
    jPrintf(",\"wifiTxPower\":%u,\"wifiMaxTxPower\":%u",
            (unsigned)wifiTxPower, (unsigned)WIFI_MAX_TX_POWER_DBM);
    jPrintf(",\"displayBrightness\":%u,\"cpuFrequency\":%u",
            (unsigned)displayBrightness, (unsigned)cpuFrequency);
    jPrintf(",\"oledEnabled\":%s,\"serialDebug\":%s,\"heapDebug\":%s",
            oledEnabled ? "true" : "false",
            serialDebug ? "true" : "false",
            heapDebugEnabled ? "true" : "false");
    jPrintf(",\"oledDisplayGroup\":"); jStr(oledDisplayGroup);
    jPrintf(",\"oledPageInterval\":%u,\"oledPageMask\":%u,\"oledButtonPin\":%d",
            (unsigned)oledPageInterval, (unsigned)oledPageMask, (int)oledButtonPin);

    // Group names
    jPrintf(",\"groupNames\":{");
    bool firstGroup = true;
    for (int i = 3; i <= MAX_CHANNELS; i++) {
        if (!firstGroup) jPrintf(",");
        jPrintf("\"%d\":", i); jStr(groupNames[i]);
        firstGroup = false;
    }
    jPrintf("}");

    jPrintf("}}");
    jSend(request);
}

// ── Diagnostics ────────────────────────────────────────────────────────────

static void buildDiagnostics() {
    jPrintf("\"diagnostics\":{");
    jPrintf("\"heap\":{\"free\":%u,\"minEver\":%u,\"maxBlock\":%u}",
            ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    // Recent heap events from heapdbg ring buffer
    jPrintf(",\"heapLog\":[");
    {
        size_t n = heapDbgCount();
        for (size_t i = 0; i < n; i++) {
            const HeapEvent &e = heapDbgAt(i);
            if (i) jPrintf(",");
            jPrintf("{\"up\":%lu,\"tag\":", (unsigned long)e.uptime);
            jStr(e.tag);
            jPrintf(",\"fd\":%ld,\"md\":%ld,\"f\":%u,\"mb\":%u}",
                    (long)e.freeDelta, (long)e.maxBlockDelta,
                    (unsigned)e.freeAfter, (unsigned)e.maxBlockAfter);
        }
    }
    jPrintf("]");
    jPrintf(",\"wifi\":{\"connected\":%s,\"rssi\":%d",
            WiFi.isConnected() ? "true" : "false", WiFi.RSSI());
    jPrintf(",\"ip\":\"%s\"", WiFi.localIP().toString().c_str());
    jPrintf(",\"mac\":\"%s\"", WiFi.macAddress().c_str());
    jPrintf(",\"ssid\":"); jStr(WiFi.SSID().c_str());
    jPrintf(",\"channel\":%d,\"disconnects\":%lu,\"lastDisconnectReason\":%u,\"lastDisconnectTime\":%lu}",
            WiFi.channel(), (unsigned long)wifiDisconnectCount,
            lastWifiDisconnectReason, (unsigned long)lastWifiDisconnectTime);
    jPrintf(",\"lora\":{\"txQueue\":%u,\"txTotal\":%lu,\"rxTotal\":%lu,\"dropped\":%lu",
            (unsigned)txBuffer.size(), (unsigned long)apiTxTotal, (unsigned long)apiRxTotal,
            (unsigned long)droppedFrames);
    jPrintf(",\"droppedBy\":{\"bufferFull\":%lu,\"retryExhaust\":%lu,\"peerDead\":%lu,"
            "\"msg\":%lu,\"ack\":%lu,\"ann\":%lu,\"annAck\":%lu,\"other\":%lu}}",
            (unsigned long)droppedBufferFull, (unsigned long)droppedRetryExhaust, (unsigned long)droppedPeerDead,
            (unsigned long)droppedMessage, (unsigned long)droppedAck,
            (unsigned long)droppedAnnounce, (unsigned long)droppedAnnounceAck, (unsigned long)droppedOther);
    jPrintf(",\"mesh\":{\"peerCount\":%u,\"routeCount\":%u,\"msgBufferUsed\":%u,\"msgBufferMax\":%u,\"eventBufferUsed\":%u,\"eventBufferMax\":%u}",
            (unsigned)peerList.size(), (unsigned)routingList.size(),
            (unsigned)msgCount, (unsigned)API_MSG_BUFFER_SIZE,
            (unsigned)evtCount, (unsigned)API_EVT_BUFFER_SIZE);
    jPrintf(",\"fileWriter\":{\"pending\":%u,\"maxPending\":%u,\"slots\":%u,\"writes\":%lu,\"dropped\":%lu}",
            (unsigned)fileWriterPending(), (unsigned)fileWriterMaxPending(),
            (unsigned)fileWriterSlotCount(),
            (unsigned long)fileWriterTotal(), (unsigned long)fileWriterDropped());
    jPrintf(",\"system\":{\"version\":\"%s\",\"board\":\"%s\"", VERSION, getBoardName());
    jPrintf(",\"uptime\":%lu", millis() / 1000);
    jPrintf(",\"resetReason\":\"%s\"", apiGetResetReason());
    jPrintf(",\"resetCount\":%lu", (unsigned long)nvsResetCount);
    jPrintf(",\"cpuFreqMHz\":%u", (unsigned)ESP.getCpuFreqMHz());
    jPrintf(",\"flashSizeKB\":%lu", (unsigned long)(ESP.getFlashChipSize() / 1024));
    jPrintf(",\"sdkVersion\":\"%s\"", ESP.getSdkVersion());
    jPrintf(",\"compileTime\":\"%s %s\"}", __DATE__, __TIME__);
    time_t now = time(nullptr);
    jPrintf(",\"ntp\":{\"synced\":%s,\"lastSyncTime\":%lu",
            now > 1700000000 ? "true" : "false", (unsigned long)lastNtpSyncTime);
    jPrintf(",\"server\":"); jStr(settings.ntpServer);
    jPrintf("}");
    jPrintf(",\"tasks\":[");
    if (mainLoopTaskHandle) {
        uint32_t hwm = uxTaskGetStackHighWaterMark(mainLoopTaskHandle) * sizeof(StackType_t);
        jPrintf("{\"name\":\"loopTask\",\"stackHighWater\":%lu,\"priority\":%lu,\"core\":%d}",
                (unsigned long)hwm,
                (unsigned long)uxTaskPriorityGet(mainLoopTaskHandle),
                (int)xTaskGetAffinity(mainLoopTaskHandle));
    }
    jPrintf("]");
    jPrintf(",\"ws\":{\"clients\":%u}", (unsigned)ws.count());
    jPrintf(",\"freertos\":{\"taskCount\":%u}", (unsigned)uxTaskGetNumberOfTasks());
    jPrintf("}");
}

static void handleDiagnostics(AsyncWebServerRequest *request) {
    if (!checkApiAuth(request)) return;
    if (!heapGuard(request)) return;
    jReset();
    jPrintf("{"); buildDiagnostics(); jPrintf("}");
    jSend(request);
}

// ── Register endpoints ──────────────────────────────────────────────────────

void setupApiEndpoints(AsyncWebServer &server) {
    // Read and increment reset counter from NVS
    Preferences diagPrefs;
    diagPrefs.begin("rmesh_diag", false);
    nvsResetCount = diagPrefs.getUInt("resets", 0) + 1;
    diagPrefs.putUInt("resets", nvsResetCount);
    diagPrefs.end();

    // Register NTP sync callback
    sntp_set_time_sync_notification_cb(onNtpSync);

    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/peers", HTTP_GET, handlePeers);
    server.on("/api/routes", HTTP_GET, handleRoutes);
    server.on("/api/messages", HTTP_GET, handleGetMessages);
    server.on("/api/groups", HTTP_GET, handleGroups);
    server.on("/api/events", HTTP_GET, handleEvents);
    server.on("/api/settings", HTTP_GET, handleSettings);
    server.on("/api/diagnostics", HTTP_GET, handleDiagnostics);

    // POST /api/messages with body handler
    server.on("/api/messages", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        nullptr,
        handlePostMessage
    );

    // POST /api/messages/import — one-shot client → node migration
    server.on("/api/messages/import", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        nullptr,
        handleImportMessages
    );

    logPrintf(LOG_INFO, "API", "REST API endpoints registered");
}

#endif
