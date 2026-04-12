#ifdef HAS_WIFI
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "mesh/reporting.h"
#include "util/heapdbg.h"
#include "util/bgWorker.h"
#include "util/serial.h"
#include "hal/settings.h"
#include "util/logging.h"
#include "mesh/peer.h"
#include "mesh/routing.h"
#include "config.h"
#include "main.h"

volatile bool topologyChanged = false;
static volatile uint32_t changeDebounceTimer = 0xFFFFFFFF;
static volatile bool reportingInProgress = false;

// Returns true if the node has internet uplink
// (WiFi client connected, not in AP mode)
static bool hasInternetUplink() {
    if (settings.apMode) return false;
    return (WiFi.status() == WL_CONNECTED);
}

// Persistent WiFiClient — keeping this as a file-level static means lwIP
// socket state is allocated once (when the first report runs, while heap
// is still unfragmented) and then reused. Avoids the repeated TIME_WAIT
// accumulation that was bleeding ~6 KB per report.
static WiFiClient s_reportClient;

static void reportTopologyWork() {
    // Manual heap accounting — the bgWorker runs this inline, but we still
    // want a scoped before/after record in the heapdbg ring.
    uint32_t _heapFree0 = ESP.getFreeHeap();
    uint32_t _heapMax0  = ESP.getMaxAllocHeap();

    HTTPClient http;
    http.setTimeout(10000);
    http.setReuse(true);
    if (!http.begin(s_reportClient, "http://www.rMesh.de:8082/report.php")) {
        heapRecord("reportTopo/beginFail", _heapFree0, _heapMax0);
        reportingInProgress = false;
        return;
    }
    http.addHeader("Content-Type", "application/json");

    // Build JSON directly into a stack buffer with snprintf — no JsonDocument,
    // no std::vector snapshot copies, no malloc. This eliminates the main
    // per-call heap churn that was fragmenting maxBlock.
    static char body[4096];
    size_t pos = 0;
    uint64_t mac = ESP.getEfuseMac();
    pos += snprintf(body + pos, sizeof(body) - pos,
        "{\"call\":\"%s\",\"position\":\"%s\",\"timestamp\":%u,"
        "\"chip_id\":\"%02X%02X%02X%02X%02X%02X\","
        "\"is_afu\":%s,\"band\":\"%s\",\"version\":\"%s\",\"device\":\"%s\",\"peers\":[",
        settings.mycall, settings.position, (unsigned)time(NULL),
        (uint8_t)(mac >> 40), (uint8_t)(mac >> 32), (uint8_t)(mac >> 24),
        (uint8_t)(mac >> 16), (uint8_t)(mac >> 8), (uint8_t)mac,
        isAmateurBand(settings.loraFrequency) ? "true" : "false",
        isPublicBand(settings.loraFrequency) ? "868" : "433",
        VERSION, PIO_ENV_NAME);

    // Iterate peer/route lists directly under mutex — no heap snapshot.
    if (xSemaphoreTake(listMutex, pdMS_TO_TICKS(1000))) {
        bool firstPeer = true;
        for (size_t i = 0; i < peerList.size() && pos < sizeof(body) - 128; i++) {
            const Peer& p = peerList[i];
            if (!p.available) continue;
            pos += snprintf(body + pos, sizeof(body) - pos,
                "%s{\"call\":\"%s\",\"rssi\":%.1f,\"snr\":%.1f,\"port\":%u,\"available\":true}",
                firstPeer ? "" : ",", p.nodeCall, p.rssi, p.snr, p.port);
            firstPeer = false;
        }
        pos += snprintf(body + pos, sizeof(body) - pos, "],\"routes\":[");
        bool firstRoute = true;
        for (size_t i = 0; i < routingList.size() && pos < sizeof(body) - 96; i++) {
            const Route& r = routingList[i];
            pos += snprintf(body + pos, sizeof(body) - pos,
                "%s{\"src\":\"%s\",\"via\":\"%s\",\"hops\":%u}",
                firstRoute ? "" : ",", r.srcCall, r.viaCall, r.hopCount);
            firstRoute = false;
        }
        xSemaphoreGive(listMutex);
    } else {
        pos += snprintf(body + pos, sizeof(body) - pos, "],\"routes\":[");
    }
    pos += snprintf(body + pos, sizeof(body) - pos, "]}");

    if (pos >= sizeof(body) - 1) {
        logPrintf(LOG_WARN, "Report", "topology body truncated (pos=%u)", (unsigned)pos);
    }

    int code = http.POST((uint8_t*)body, pos);
    http.end();

    if (code == 200) {
        topologyChanged = false;
    }
    logPrintf(LOG_DEBUG, "Report", "topology report http_code=%d len=%u", code, (unsigned)pos);
    heapRecord("reportTopo/done", _heapFree0, _heapMax0);
    reportingInProgress = false;
}

void reportTopology() {
    if (!hasInternetUplink()) return;
    if (strlen(settings.mycall) == 0) return;
    if (reportingInProgress) return;
    if (ESP.getFreeHeap() < 40000) {
        logPrintf(LOG_DEBUG, "Report", "topology skipped, low heap=%u", ESP.getFreeHeap());
        return;
    }
    reportingInProgress = true;

    HEAP_MARK("reportTopo/enq");
    if (!bgWorkerEnqueue(reportTopologyWork)) {
        logPrintf(LOG_WARN, "Report", "reportTopo enqueue failed (queue full?)");
        reportingInProgress = false;
    }
}

// Must be called regularly from the main loop
void reportTopologyIfChanged() {
    if (topologyChanged && (int32_t)(millis() - changeDebounceTimer) >= 0) {
        changeDebounceTimer = millis() + 0x7FFFFFFF; // effectively disabled
        reportTopology();
    }
}

// Called from peer.cpp / routing.cpp when something changes
void markTopologyChanged() {
    topologyChanged = true;
    // Debounce: report at earliest 30 s after the last change
    changeDebounceTimer = millis() + 30000;
}

bool logRemoteCommand(const char* sender, const char* command) {
    if (!hasInternetUplink()) return false;
    if (strlen(settings.mycall) == 0) return false;

    WiFiClient client;
    HTTPClient http;
    http.setTimeout(5000);
    if (!http.begin(client, "http://www.rMesh.de:8082/command_log.php")) return false;
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["call"]    = settings.mycall;
    doc["sender"]  = sender;
    doc["command"] = command;

    char body[256];
    size_t len = serializeJson(doc, body, sizeof(body));
    int code = http.POST((uint8_t*)body, len);
    http.end();

    logPrintf(LOG_DEBUG, "Report", "command_log sender=%s command=%s http_code=%d", sender, command, code);
    return (code == 200);
}

#endif // HAS_WIFI
