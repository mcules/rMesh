#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#include "reporting.h"
#include "settings.h"
#include "peer.h"
#include "routing.h"
#include "config.h"

bool topologyChanged = false;
static uint32_t changeDebounceTimer = 0xFFFFFFFF;

// Gibt true zurück, wenn der Node Internet-Uplink hat
// (WiFi-Client verbunden, nicht im AP-Mode)
static bool hasInternetUplink() {
    if (settings.apMode) return false;
    return (WiFi.status() == WL_CONNECTED);
}

void reportTopology() {
    if (!hasInternetUplink()) return;
    if (strlen(settings.mycall) == 0) return;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, "https://www.rMesh.de/report.php");
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

    // Peer-Liste
    JsonArray peers = doc["peers"].to<JsonArray>();
    for (auto& p : peerList) {
        if (!p.available) continue;
        JsonObject o = peers.add<JsonObject>();
        o["call"]      = p.nodeCall;
        o["rssi"]      = p.rssi;
        o["snr"]       = p.snr;
        o["port"]      = p.port;
        o["available"] = p.available;
    }

    // Routing-Tabelle
    JsonArray routes = doc["routes"].to<JsonArray>();
    for (auto& r : routingList) {
        JsonObject o = routes.add<JsonObject>();
        o["src"]  = r.srcCall;
        o["via"]  = r.viaCall;
        o["hops"] = r.hopCount;
    }

    char* buf = (char*)malloc(4096);
    if (!buf) { http.end(); return; }
    size_t len = serializeJson(doc, buf, 4096);
    int code = http.POST((uint8_t*)buf, len);
    free(buf);
    http.end();

    if (code == 200) {
        Serial.println("Topology reported to rMesh.de");
        topologyChanged = false;
    } else {
        Serial.printf("Topology report failed: HTTP %d\n", code);
    }
}

// Muss regelmäßig aus dem main loop aufgerufen werden
void reportTopologyIfChanged() {
    if (topologyChanged && millis() > changeDebounceTimer) {
        changeDebounceTimer = 0xFFFFFFFF;
        reportTopology();
    }
}

// Wird aus peer.cpp / routing.cpp aufgerufen wenn sich etwas ändert
void markTopologyChanged() {
    topologyChanged = true;
    // Debounce: frühestens 30s nach der letzten Änderung reporten
    changeDebounceTimer = millis() + 30000;
}
