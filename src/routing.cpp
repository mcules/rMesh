#include <Arduino.h>
#include <ArduinoJson.h>

#include "routing.h"
#include "reporting.h"
#include "webFunctions.h"
#include "peer.h"
#include "helperFunctions.h"
#include "config.h"
#include "settings.h"
#include "persistence.h"

//Routing Liste
std::vector<Route> routingList;

void getRoute(const char* dstCall, char* viaCall, size_t len) {
    //Serial.printf("dst:%s via:%s\n", dstCall, viaCall);
    viaCall[0] = '\0';
    //Routing Liste duchsuchen
    auto it = std::find_if(routingList.begin(), routingList.end(), [&](const Route& r) { return (strcmp(r.srcCall, dstCall) == 0); });

    if (it != routingList.end()) {
        //Prüfen, ob Call "noch" in Peer-Liste
        auto it2 = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) { return (strcmp(peer.nodeCall, it->viaCall) == 0) && (peer.available == true) ; });
        if (it2 != peerList.end()) {
            strncpy(viaCall, it->viaCall, len - 1);
            viaCall[len - 1] = '\0';
        } 
    } 
}

bool checkRoute(char* srcCall, char* viaCall) {
    //Routing Liste duchsuchen
    auto it = std::find_if(routingList.begin(), routingList.end(), [&](const Route& r) { return (strcmp(r.srcCall, srcCall) == 0) && (strcmp(r.viaCall, viaCall) == 0); });
    if (it != routingList.end()) { return true; }
    return false;
}

void sendRoutingList() {
    // Estimate required buffer size: ~80 bytes per route entry + overhead
    size_t bufSize = 40 + routingList.size() * 90;
    if (bufSize < 256) bufSize = 256;
    char* jsonBuffer = (char*)malloc(bufSize);
    if (jsonBuffer == nullptr) {
        Serial.println(F("[OOM] sendRoutingList: malloc failed"));
        return;
    }
    size_t pos = 0;

    pos += snprintf(jsonBuffer + pos, bufSize - pos, "{\"routingList\":{\"routes\":[");

    for (size_t i = 0; i < routingList.size() && pos < bufSize - 120; i++) {
        if (i > 0) jsonBuffer[pos++] = ',';
        pos += snprintf(jsonBuffer + pos, bufSize - pos,
            "{\"srcCall\":\"%s\",\"viaCall\":\"%s\",\"timestamp\":%ld,\"hopCount\":%u}",
            routingList[i].srcCall, routingList[i].viaCall,
            (long)routingList[i].timestamp, routingList[i].hopCount);
    }

    pos += snprintf(jsonBuffer + pos, bufSize - pos, "]}}");
    if (pos < bufSize) {
        wsBroadcast(jsonBuffer, pos);
    }
    free(jsonBuffer);
}

void addRoutingList(const char* srcCall, const char* viaCall, uint8_t hopCount) {
    if (strlen(srcCall) == 0 || strlen(viaCall) == 0) return;
    if (strcmp(settings.mycall, srcCall) == 0) return;

    // 1. Prüfen, ob viaCall (der nächste Hop) in Peer Liste ist
    auto itt = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) { 
        return (strcmp(peer.nodeCall, viaCall) == 0) && (peer.available == true); 
    });
    if (itt == peerList.end()) return;

    // 2. Routing Liste nach dem Ziel-Call (srcCall) durchsuchen
    auto it = std::find_if(routingList.begin(), routingList.end(), [&](const Route& r) {
        return (strcmp(r.srcCall, srcCall) == 0);
    });

    bool isNew = (it == routingList.end());

    if (isNew) {
        // Fall A: Ziel unbekannt -> Neu anlegen (enforce capacity limit)
        if (routingList.size() >= ROUTING_BUFFER_SIZE) {
            Serial.printf("[Routing] Table full (%d), ignoring route to %s\n", ROUTING_BUFFER_SIZE, srcCall);
            return;
        }
        Route r;
        strncpy(r.srcCall, srcCall, MAX_CALLSIGN_LENGTH);
        r.srcCall[MAX_CALLSIGN_LENGTH] = '\0';
        strncpy(r.viaCall, viaCall, MAX_CALLSIGN_LENGTH);
        r.viaCall[MAX_CALLSIGN_LENGTH] = '\0';
        r.timestamp = time(NULL);
        r.hopCount = hopCount;
        routingList.push_back(r);
        routesDirty = true;
        Serial.printf("[Reporting] Neue Route: %s via %s (%d Hops)\n", srcCall, viaCall, hopCount);
    } else {
        // Fall B: Ziel existiert -> Kürzester Weg gewinnt
        if (hopCount < it->hopCount || strcmp(it->viaCall, viaCall) == 0) {
            // Mark dirty if via-call or hop count actually changed
            if (strcmp(it->viaCall, viaCall) != 0 || it->hopCount != hopCount) {
                routesDirty = true;
            }
            strncpy(it->viaCall, viaCall, MAX_CALLSIGN_LENGTH);
            it->viaCall[MAX_CALLSIGN_LENGTH] = '\0';
            it->timestamp = time(NULL);
            it->hopCount = hopCount;
        } else {
            // Neuer Pfad ist länger oder gleich lang über anderen Knoten -> ignorieren
            return;
        }
    }

    // 3. Sortierung (kürzeste Hops nach oben)
    std::sort(routingList.begin(), routingList.end(), [](const Route& a, const Route& b) {
        if (a.hopCount != b.hopCount) return a.hopCount < b.hopCount;
        return a.timestamp > b.timestamp;
    });

    sendRoutingList();
    if (isNew) markTopologyChanged();
}
