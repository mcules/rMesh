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
    //Suchen, ob Peer bereits existiert
    auto it = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) { return (time(NULL) - peer.timestamp) > PEER_TIMEOUT; });
    if (it != peerList.end()) {
        //portENTER_CRITICAL(&peerListMux);
        peerList.erase(it);
        //portEXIT_CRITICAL(&peerListMux);
        sendPeerList();
    } 

    //Doppelte Peers -> mit weniger SNR -> available = false
    for (size_t i = 0; i < peerList.size(); i++) {
        if (!peerList[i].available) continue;
        for (size_t j = i + 1; j < peerList.size(); j++) {
            if (!peerList[j].available) continue;
            if (strcmp(peerList[i].nodeCall, peerList[j].nodeCall) == 0) {
                if (peerList[i].snr < peerList[j].snr) {
                    peerList[i].available = false;
                    break; 
                } else {
                    peerList[j].available = false;
                }
            }
        }
    }

    
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
    // Suchen, ob Peer bereits existiert
    auto it = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) { return (strcmp(peer.nodeCall, call) == 0) && (peer.port == port); });

    if (it != peerList.end()) {
        // Peer existiert: update
        it->available = available;
    }

    //Peer Liste neu senden
    checkPeerList();
    sendPeerList();
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
        //portENTER_CRITICAL(&peerListMux);
        peerList.push_back(p);
        //portEXIT_CRITICAL(&peerListMux);

    }

    // Sortieren nach SNR (absteigend)
    std::sort(peerList.begin(), peerList.end(), [](const Peer& a, const Peer& b) { return a.snr > b.snr; });
    checkPeerList();
    sendPeerList();
}




