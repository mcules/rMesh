#include <Arduino.h>
#include <ArduinoJson.h>

#include "frame.h"
#include "main.h"
#include "webFunctions.h"
#include "peer.h"

//Peer-Liste
std::vector<Peer> peerList;
//portMUX_TYPE peerListMux = portMUX_INITIALIZER_UNLOCKED;

void checkPeerList() {
    bool update = false;
    //Suchen, ob Peer bereits existiert
    auto it = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) { return (time(NULL) - peer.timestamp) > PEER_TIMEOUT; });
    if (it != peerList.end()) {
        peerList.erase(it);
        update = true;
    } 

    //Doppelte Peers -> mit weniger SNR -> available = false
    for (size_t i = 0; i < peerList.size(); i++) {
        if (!peerList[i].available) continue;
        for (size_t j = i + 1; j < peerList.size(); j++) {
            if (!peerList[j].available) continue;
            if (strcmp(peerList[i].nodeCall, peerList[j].nodeCall) == 0) {
                if (peerList[i].snr < peerList[j].snr) {
                    if (peerList[i].available != false) {
                        peerList[i].available = false;
                        update = true;
                    }
                    break; 
                } else {
                    if (peerList[j].available != false) {
                        peerList[j].available = false;
                        update = true;
                    }
                }
            }
        }
    }

    if (update == true) { sendPeerList(); }
   
}

void sendPeerList() {
    JsonDocument doc;
    doc["peerlist"]["peers"] = JsonArray();
    for (int i = 0; i < peerList.size(); i++) {
        JsonObject peer = doc["peerlist"]["peers"].add<JsonObject>();
        peer["port"] = peerList[i].port;
        peer["call"] = peerList[i].nodeCall;
        peer["timestamp"] = peerList[i].timestamp;
        peer["rssi"] = peerList[i].rssi;
        peer["snr"] = peerList[i].snr;
        peer["frqError"] = peerList[i].frqError;
        peer["available"] = peerList[i].available;
    }
    char* jsonBuffer = (char*)malloc(2048);
    size_t len = serializeJson(doc, jsonBuffer, 2048);
    ws.textAll(jsonBuffer, len);
    free(jsonBuffer);
}



void availablePeerList(const char* call, bool available, uint8_t port) {
    bool update = false;
    // Suchen, ob Peer bereits existiert
    auto it = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) { return (strcmp(peer.nodeCall, call) == 0) && (peer.port == port); });

    if (it != peerList.end()) {
        // Peer existiert: update
        if (it->available != available) {
            update = true;
            it->available = available;
        }
    }
    //Peer Liste neu senden
    if (update == true) { sendPeerList(); }
}

void addPeerList(Frame &f) {
    // Suchen, ob Peer bereits existiert
    auto it = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) { return (strcmp(peer.nodeCall, f.nodeCall) == 0) && (peer.port == f.port) ; });

    if (it != peerList.end()) {
        // Peer existiert: update, aber available Flag behalten
        it->timestamp = f.timestamp;
        it->rssi = f.rssi;
        it->snr = f.snr;
        it->frqError = f.frqError;
        it->port = f.port;
    } else {
        // Peer nicht gefunden: hinzufügen
        Peer p;
        memcpy(p.nodeCall, f.nodeCall, sizeof(p.nodeCall));
        p.timestamp = f.timestamp;
        p.rssi = f.rssi;
        p.snr = f.snr;
        p.frqError = f.frqError;
        p.port = f.port;
        p.available = false;
        peerList.push_back(p);
    }

    // Sortieren nach SNR (absteigend)
    std::sort(peerList.begin(), peerList.end(), [](const Peer& a, const Peer& b) { return a.snr > b.snr; });
    sendPeerList(); 
}




