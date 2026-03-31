#ifdef HAS_WIFI
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "reporting.h"
#include "serial.h"
#include "settings.h"
#include "peer.h"
#include "routing.h"
#include "config.h"

bool topologyChanged = false;
static uint32_t changeDebounceTimer = 0xFFFFFFFF;
static volatile bool reportingInProgress = false;

// Gibt true zurück, wenn der Node Internet-Uplink hat
// (WiFi-Client verbunden, nicht im AP-Mode)
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

    // JSON aufbauen
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

    // AFU-Flag und Band aus der eingestellten Frequenz ableiten
    doc["is_afu"] = isAmateurBand(settings.loraFrequency);
    doc["band"]   = isPublicBand(settings.loraFrequency) ? "868" : "433";

    // Peer-Liste (Snapshot, da peerList sich ändern kann)
    JsonArray peers = doc["peers"].to<JsonArray>();
    for (size_t i = 0; i < peerList.size(); i++) {
        if (!peerList[i].available) continue;
        JsonObject o = peers.add<JsonObject>();
        o["call"]      = peerList[i].nodeCall;
        o["rssi"]      = peerList[i].rssi;
        o["snr"]       = peerList[i].snr;
        o["port"]      = peerList[i].port;
        o["available"] = peerList[i].available;
    }

    // Routing-Tabelle
    JsonArray routes = doc["routes"].to<JsonArray>();
    for (size_t i = 0; i < routingList.size(); i++) {
        JsonObject o = routes.add<JsonObject>();
        o["src"]  = routingList[i].srcCall;
        o["via"]  = routingList[i].viaCall;
        o["hops"] = routingList[i].hopCount;
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
    if (serialDebug) Serial.printf("DBG:{\"event\":\"reporting\",\"action\":\"topology\",\"http_code\":%d}\n", code);
    reportingInProgress = false;
    vTaskDelete(NULL);
}

void reportTopology() {
    if (!hasInternetUplink()) return;
    if (strlen(settings.mycall) == 0) return;
    if (reportingInProgress) return;
    if (ESP.getFreeHeap() < 40000) {
        if (serialDebug) Serial.printf("DBG:{\"event\":\"reporting\",\"action\":\"skipped\",\"reason\":\"low_heap\",\"heap\":%u}\n", ESP.getFreeHeap());
        return;
    }
    reportingInProgress = true;

    xTaskCreate(reportTopologyTask, "ReportTopo", 4096, NULL, 1, NULL);
}

// Muss regelmäßig aus dem main loop aufgerufen werden
void reportTopologyIfChanged() {
    if (topologyChanged && (int32_t)(millis() - changeDebounceTimer) >= 0) {
        changeDebounceTimer = millis() + 0x7FFFFFFF; // effectively disabled
        reportTopology();
    }
}

// Wird aus peer.cpp / routing.cpp aufgerufen wenn sich etwas ändert
void markTopologyChanged() {
    topologyChanged = true;
    // Debounce: frühestens 30s nach der letzten Änderung reporten
    changeDebounceTimer = millis() + 30000;
}

#endif // HAS_WIFI
