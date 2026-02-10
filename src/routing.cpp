#include <Arduino.h>
#include <ArduinoJson.h>

#include "routing.h"
#include "webFunctions.h"
#include "peer.h"
#include "helperFunctions.h"
#include "config.h"
#include "settings.h"

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
    JsonDocument doc;
    doc["routingList"]["routes"] = JsonArray();
    for (int i = 0; i < routingList.size(); i++) {
        JsonObject route = doc["routingList"]["routes"].add<JsonObject>();
        route["srcCall"] = routingList[i].srcCall;
        route["viaCall"] = routingList[i].viaCall;
        route["timestamp"] = routingList[i].timestamp;
        route["hopCount"] = routingList[i].hopCount;
    }
    char* jsonBuffer = (char*)malloc(2048);
    size_t len = serializeJson(doc, jsonBuffer, 2048);
    ws.textAll(jsonBuffer, len);
    free(jsonBuffer);
}

void addRoutingList(const char* srcCall, const char* viaCall, uint8_t hopCount) {
    //Serial.printf("src:%s via:%s\n", srcCall, viaCall);
    if (strlen(srcCall) == 0) {return;}
    if (strlen(viaCall) == 0) {return;}
    if (strcmp(settings.mycall, srcCall) == 0) {return;}

    //Prüfen, ob viaCall in Peer Liste ist. Wenn nicht -> Abbruch
    auto itt = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) { return (strcmp(peer.nodeCall, viaCall) == 0) && (peer.available == true); });
    if (itt == peerList.end()) { return; }

    //Routing Liste duchsuchen, ob Call bereits existiert
    auto it = std::find_if(routingList.begin(), routingList.end(), [&](const Route& r) {
        return (strcmp(r.srcCall, srcCall) == 0) && (strcmp(r.viaCall, viaCall) == 0);
    });

    if (it == routingList.end()) {
        //Neu
        Route r;
        memcpy(r.srcCall, srcCall, MAX_CALLSIGN_LENGTH + 1);
        memcpy(r.viaCall, viaCall, MAX_CALLSIGN_LENGTH + 1);
        r.timestamp = time(NULL);
        r.hopCount = hopCount;
        routingList.push_back(r);
    } else {
        //Aktualisieren
        it->timestamp = time(NULL);
        it->hopCount = hopCount;
    }

    std::sort(routingList.begin(), routingList.end(), [](const Route& a, const Route& b) {
        if (a.hopCount != b.hopCount) {
            return a.hopCount < b.hopCount; 
        }
        return a.timestamp > b.timestamp;
    });

    sendRoutingList();

}
