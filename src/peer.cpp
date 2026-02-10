#include <Arduino.h>
#include <ArduinoJson.h>

#include "frame.h"
#include "main.h"
#include "webFunctions.h"
#include "peer.h"
#include "helperFunctions.h"
#include "config.h"
#include "settings.h"

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
    auto doc = new JsonDocument();
    (*doc)["peerlist"]["peers"] = JsonArray();
    for (int i = 0; i < peerList.size(); i++) {
        JsonObject peer = (*doc)["peerlist"]["peers"].add<JsonObject>();
        peer["port"] = peerList[i].port;
        peer["call"] = peerList[i].nodeCall;
        peer["timestamp"] = peerList[i].timestamp;
        peer["rssi"] = peerList[i].rssi;
        peer["snr"] = peerList[i].snr;
        peer["frqError"] = peerList[i].frqError;
        peer["available"] = peerList[i].available;
    }
    
    size_t len = measureJson(*doc);
    AsyncWebSocketMessageBuffer * wsBuffer = ws.makeBuffer(len + 1); 
    if (wsBuffer != nullptr) {
        char* dataPtr = (char*)wsBuffer->get();
        if (dataPtr != nullptr) {
            // In den Buffer schreiben
            serializeJson(*doc, dataPtr, len + 1);
            
            // 4. Ohne Null-Byte senden (behebt den SyntaxError im Browser)
            for (auto & client : ws.getClients()) {
                if (client.status() == WS_CONNECTED) {
                    client.text(dataPtr, len); 
                }
            }
        }
    } else {
        Serial.println(F("CRITICAL: No memory for WebSocket buffer!"));
    }
    delete doc;


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
    if (strlen(f.nodeCall) == 0) {return;}
    if (strcmp(f.nodeCall, settings.mycall) == 0) {return;}

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




