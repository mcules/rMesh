#include <Arduino.h>
#include <ArduinoJson.h>

#include "frame.h"
#include "main.h"
#include "webFunctions.h"
#include "peer.h"
#include "reporting.h"
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

    //Doppelte Peers: WiFi (port 1) bevorzugen vor LoRa (port 0); bei gleichem Port -> besser SNR gewinnt
    for (size_t i = 0; i < peerList.size(); i++) {
        if (!peerList[i].available) continue;
        for (size_t j = i + 1; j < peerList.size(); j++) {
            if (!peerList[j].available) continue;
            if (strcmp(peerList[i].nodeCall, peerList[j].nodeCall) == 0) {
                bool iWifi = (peerList[i].port == 1);
                bool jWifi = (peerList[j].port == 1);
                if (iWifi && !jWifi) {
                    // i=WiFi, j=LoRa → LoRa deaktivieren
                    peerList[j].available = false;
                    update = true;
                } else if (!iWifi && jWifi) {
                    // i=LoRa, j=WiFi → LoRa deaktivieren
                    peerList[i].available = false;
                    update = true;
                    break;
                } else {
                    // Gleicher Port → besseren SNR bevorzugen
                    if (peerList[i].snr < peerList[j].snr) {
                        peerList[i].available = false;
                        update = true;
                        break;
                    } else {
                        peerList[j].available = false;
                        update = true;
                    }
                }
            }
        }
    }

    if (update == true) { sendPeerList(); markTopologyChanged(); }

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
    
    char* jsonBuffer = (char*)malloc(measureJson(doc) + 1);
    if (jsonBuffer != nullptr) {
        size_t len = serializeJson(doc, jsonBuffer, measureJson(doc) + 1);
        wsBroadcast(jsonBuffer, len);
        free(jsonBuffer);
    } else {
        Serial.println(F("Fehler: Kein RAM für Buffer"));
    }
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
    if (update == true) { sendPeerList(); markTopologyChanged(); }
}

void addPeerList(Frame &f) {
    if (strlen(f.nodeCall) == 0) {return;}
    if (strcmp(f.nodeCall, settings.mycall) == 0) {return;}

    // Suchen, ob Peer bereits existiert
    auto it = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) { return (strcmp(peer.nodeCall, f.nodeCall) == 0) && (peer.port == f.port) ; });

    bool isNew = (it == peerList.end());

    if (!isNew) {
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
        Serial.printf("[Reporting] Neuer Peer: %s (Port %d)\n", f.nodeCall, f.port);
    }

    // Sortieren nach SNR (absteigend)
    std::sort(peerList.begin(), peerList.end(), [](const Peer& a, const Peer& b) { return a.snr > b.snr; });
    sendPeerList();
    if (isNew) markTopologyChanged();
}




