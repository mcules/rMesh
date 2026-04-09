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
#include "logging.h"
#include "api.h"

//Routing list
std::vector<Route> routingList;

void getRoute(const char* dstCall, char* viaCall, size_t len) {
    //Serial.printf("dst:%s via:%s\r\n", dstCall, viaCall);
    viaCall[0] = '\0';
    //Search routing list
    auto it = std::find_if(routingList.begin(), routingList.end(), [&](const Route& r) { return (strcmp(r.srcCall, dstCall) == 0); });

    if (it != routingList.end()) {
        //Check if call is still in peer list
        auto it2 = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) { return (strcmp(peer.nodeCall, it->viaCall) == 0) && (peer.available == true) ; });
        if (it2 != peerList.end()) {
            strncpy(viaCall, it->viaCall, len - 1);
            viaCall[len - 1] = '\0';
        } 
    } 
}

void removeDirectRoute(const char* call) {
    // Remove a direct-neighbor route entry (srcCall == viaCall == call, hopCount 0)
    // so that an alternative multi-hop route can be learned via another peer.
    auto it = std::find_if(routingList.begin(), routingList.end(), [&](const Route& r) {
        return (strcmp(r.srcCall, call) == 0) && (strcmp(r.viaCall, call) == 0);
    });
    if (it != routingList.end()) {
        logPrintf(LOG_INFO, "Route", "Removing direct route to %s (SNR below threshold)", call);
        routingList.erase(it);
        routesDirty = true;
        sendRoutingList();
        markTopologyChanged();
    }
}

bool checkRoute(char* srcCall, char* viaCall) {
    //Search routing list
    auto it = std::find_if(routingList.begin(), routingList.end(), [&](const Route& r) { return (strcmp(r.srcCall, srcCall) == 0) && (strcmp(r.viaCall, viaCall) == 0); });
    if (it != routingList.end()) { return true; }
    return false;
}

void sendRoutingList() {
    // Replaced: no longer serializes full JSON on heap.
    // Sends lightweight notification; WebUI fetches /api/routes instead.
    notifyRoutingChanged();
}

void addRoutingList(const char* srcCall, const char* viaCall, uint8_t hopCount) {
    if (strlen(srcCall) == 0 || strlen(viaCall) == 0) return;
    if (strcmp(settings.mycall, srcCall) == 0) return;
    // Loop detection: reject routes that point back to ourselves.
    // viaCall == srcCall is the legitimate "direct neighbor" case (hopCount must be 0);
    // only reject when the hopCount is inconsistent with that.
    if (strcmp(viaCall, srcCall) == 0 && hopCount != 0) return;
    if (strcmp(viaCall, settings.mycall) == 0) return;

    // 1. Check if viaCall (the next hop) is in the peer list
    auto itt = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) { 
        return (strcmp(peer.nodeCall, viaCall) == 0) && (peer.available == true); 
    });
    if (itt == peerList.end()) return;

    // 2. Search routing list for the destination call (srcCall)
    auto it = std::find_if(routingList.begin(), routingList.end(), [&](const Route& r) {
        return (strcmp(r.srcCall, srcCall) == 0);
    });

    bool isNew = (it == routingList.end());

    if (isNew) {
        // Case A: Destination unknown -> Create new (evict oldest if full)
        if (routingList.size() >= ROUTING_BUFFER_SIZE) {
            // Evict oldest entry by timestamp
            auto oldest = std::min_element(routingList.begin(), routingList.end(),
                [](const Route& a, const Route& b) { return a.timestamp < b.timestamp; });
            if (oldest != routingList.end()) {
                logPrintf(LOG_WARN, "Route", "Table full, evicting %s", oldest->srcCall);
                routingList.erase(oldest);
            }
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
        logPrintf(LOG_INFO, "Route", "New route: %s via %s (%d hops)", srcCall, viaCall, hopCount);
    } else {
        // Case B: Destination exists -> Shortest path wins
        if (hopCount < it->hopCount) {
            // Shorter path found -> Update route
            routesDirty = true;
            logPrintf(LOG_INFO, "Route", "Updated route: %s via %s (%d hops, was %d)",
                      srcCall, viaCall, hopCount, it->hopCount);
            strncpy(it->viaCall, viaCall, MAX_CALLSIGN_LENGTH);
            it->viaCall[MAX_CALLSIGN_LENGTH] = '\0';
            it->timestamp = time(NULL);
            it->hopCount = hopCount;
        } else if (strcmp(it->viaCall, viaCall) == 0) {
            // Same via node -> only refresh timestamp
            it->timestamp = time(NULL);
        } else {
            // New path is longer or equal via different node -> ignore
            return;
        }
    }

    // 3. Sort (shortest hops first)
    std::sort(routingList.begin(), routingList.end(), [](const Route& a, const Route& b) {
        if (a.hopCount != b.hopCount) return a.hopCount < b.hopCount;
        return a.timestamp > b.timestamp;
    });

    sendRoutingList();
    if (isNew) markTopologyChanged();
}
