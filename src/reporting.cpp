#ifdef HAS_WIFI
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "reporting.h"
#include "serial.h"
#include "settings.h"
#include "logging.h"
#include "peer.h"
#include "routing.h"
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

static void reportTopologyTask(void* pvParameters) {
    WiFiClient client;
    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(client, "http://www.rMesh.de:8082/report.php")) {
        reportingInProgress = false;
        vTaskDelete(NULL);
        return;
    }
    http.addHeader("Content-Type", "application/json");

    // Build JSON
    JsonDocument doc;
    doc["call"]      = settings.mycall;
    doc["position"]  = settings.position;
    doc["timestamp"] = (uint32_t)time(NULL);

    // Chip-ID (EFuse MAC)
    {
        uint64_t mac = ESP.getEfuseMac();
        char chipId[13];
        snprintf(chipId, sizeof(chipId), "%02X%02X%02X%02X%02X%02X",
            (uint8_t)(mac >> 40), (uint8_t)(mac >> 32), (uint8_t)(mac >> 24),
            (uint8_t)(mac >> 16), (uint8_t)(mac >> 8), (uint8_t)(mac));
        doc["chip_id"] = chipId;
    }

    // Derive amateur radio flag and band from the configured frequency
    doc["is_afu"] = isAmateurBand(settings.loraFrequency);
    doc["band"]   = isPublicBand(settings.loraFrequency) ? "868" : "433";

    // Snapshot lists under listMutex to avoid data races with the main loop
    std::vector<Peer> peerSnap;
    std::vector<Route> routeSnap;
    if (xSemaphoreTake(listMutex, pdMS_TO_TICKS(1000))) {
        peerSnap = peerList;
        routeSnap = routingList;
        xSemaphoreGive(listMutex);
    }

    JsonArray peers = doc["peers"].to<JsonArray>();
    for (size_t i = 0; i < peerSnap.size(); i++) {
        if (!peerSnap[i].available) continue;
        JsonObject o = peers.add<JsonObject>();
        o["call"]      = peerSnap[i].nodeCall;
        o["rssi"]      = peerSnap[i].rssi;
        o["snr"]       = peerSnap[i].snr;
        o["port"]      = peerSnap[i].port;
        o["available"] = peerSnap[i].available;
    }

    // Routing table
    JsonArray routes = doc["routes"].to<JsonArray>();
    for (size_t i = 0; i < routeSnap.size(); i++) {
        JsonObject o = routes.add<JsonObject>();
        o["src"]  = routeSnap[i].srcCall;
        o["via"]  = routeSnap[i].viaCall;
        o["hops"] = routeSnap[i].hopCount;
    }

    char* buf = (char*)malloc(4096);
    if (!buf) { http.end(); reportingInProgress = false; vTaskDelete(NULL); return; }
    size_t len = serializeJson(doc, buf, 4096);
    int code = http.POST((uint8_t*)buf, len);
    free(buf);
    http.end();

    if (code == 200) {
        topologyChanged = false;
    }
    logPrintf(LOG_DEBUG, "Report", "topology report http_code=%d", code);
    reportingInProgress = false;
    vTaskDelete(NULL);
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

    xTaskCreate(reportTopologyTask, "ReportTopo", 4096, NULL, 1, NULL);
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

    String body;
    serializeJson(doc, body);
    int code = http.POST(body);
    http.end();

    logPrintf(LOG_DEBUG, "Report", "command_log sender=%s command=%s http_code=%d", sender, command, code);
    return (code == 200);
}

#endif // HAS_WIFI
