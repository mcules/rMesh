#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "udp.h"
#include "frame.h"
#include "settings.h"
#include "webFunctions.h"


WiFiUDP udp;

void initUDP() {
    udp.begin(UDP_PORT);
    esp_log_level_set("NetworkUdp", ESP_LOG_NONE);
}


bool checkUDP(Frame &f) {
    if (WiFi.status() != WL_CONNECTED) return false;
    size_t packetSize = udp.parsePacket();
    if (packetSize) {
        uint8_t packetBuffer[256];
        int len = udp.read(packetBuffer, sizeof(packetBuffer));
        // Erstes Byte ist das SyncWord des Senders – nur gleiche Netze akzeptieren.
        // Alte Firmware ohne SyncWord-Präfix wird als 433-MHz-Netz (AMATEUR_SYNCWORD) behandelt.
        if (len < 1) return false;
        bool hasSyncword = (packetBuffer[0] == AMATEUR_SYNCWORD || packetBuffer[0] == PUBLIC_SYNCWORD);
        uint8_t pktSyncword = hasSyncword ? packetBuffer[0] : AMATEUR_SYNCWORD;
        if (pktSyncword != settings.loraSyncWord) return false;

        f.importBinary(hasSyncword ? packetBuffer + 1 : packetBuffer,
                       hasSyncword ? len - 1 : len);

        // Sender-IP im Peer-Vektor suchen
        IPAddress senderIP = udp.remoteIP();
        int peerIdx = -1;
        for (size_t i = 0; i < udpPeers.size(); i++) {
            if (udpPeers[i] == senderIP) { peerIdx = (int)i; break; }
        }

        if (f.frameType == Frame::FrameTypes::ANNOUNCE_FRAME && peerIdx < 0) {
            // Neuen Peer aus Broadcast-Announce anlegen
            udpPeers.push_back(senderIP);
            udpPeerLegacy.push_back(!hasSyncword);
            udpPeerEnabled.push_back(true);
            udpPeerCall.push_back(strlen(f.nodeCall) > 0 ? String(f.nodeCall) : "");
            saveUdpPeers();
            Serial.printf("UDP Peer von Announce eingetragen: %d.%d.%d.%d (%s)\n",
                senderIP[0], senderIP[1], senderIP[2], senderIP[3], f.nodeCall);
        } else if (!hasSyncword) {
            // Legacy-Node (kein SyncWord-Präfix)
            if (peerIdx < 0) {
                udpPeers.push_back(senderIP);
                udpPeerLegacy.push_back(true);
                udpPeerEnabled.push_back(true);
                udpPeerCall.push_back(strlen(f.nodeCall) > 0 ? String(f.nodeCall) : "");
                saveUdpPeers();
                Serial.printf("UDP Legacy-Peer automatisch eingetragen: %d.%d.%d.%d (%s)\n",
                    senderIP[0], senderIP[1], senderIP[2], senderIP[3], f.nodeCall);
            } else if (!(bool)udpPeerLegacy[peerIdx]) {
                udpPeerLegacy[peerIdx] = true;
                saveUdpPeers();
            }
        }

        // Rufzeichen zum bekannten Peer nachlernen (auch wenn Peer schon eingetragen war)
        if (peerIdx >= 0 && strlen(f.nodeCall) > 0) {
            while ((size_t)peerIdx >= udpPeerCall.size()) udpPeerCall.push_back("");
            if (udpPeerCall[peerIdx] != f.nodeCall) {
                udpPeerCall[peerIdx] = f.nodeCall;
                sendSettings();  // WebUI aktualisieren
            }
        }
        f.tx = false;
        f.timestamp = time(NULL);
        f.rssi = 0;
        f.snr = 100;
        f.frqError = 0;
        f.port = 1;
        return true;
    }
    return false;
}

void sendUDP(Frame &f) {
    uint8_t txBuffer[255];
    size_t txBufferLength;

    //Frame ergänzen
    strncpy(f.nodeCall, settings.mycall, sizeof(f.nodeCall));
    f.tx = true;
    f.port = 1;
    f.timestamp = time(NULL);

    //Senden
    if (strlen(f.nodeCall) == 0) {return;}
    // SyncWord als erstes Byte voranstellen, damit Empfänger fremde Netze ablehnen können
    txBuffer[0] = settings.loraSyncWord;
    txBufferLength = f.exportBinary(txBuffer + 1, sizeof(txBuffer) - 1) + 1;
    bool udpTX = false;
    for (size_t i = 0; i < udpPeers.size(); i++) {
        if (!(bool)udpPeerEnabled[i]) continue;  // Deaktivierte Peers überspringen
        udp.beginPacket(udpPeers[i], UDP_PORT);
        if ((bool)udpPeerLegacy[i]) {
            udp.write(txBuffer + 1, txBufferLength - 1);
        } else {
            udp.write(txBuffer, txBufferLength);
        }
        udp.endPacket();
        udp.flush();
        udpTX = true;
    }
    // Announces immer auch per Broadcast senden (damit neue Nodes erkannt werden)
    if (f.frameType == Frame::FrameTypes::ANNOUNCE_FRAME) {
        udp.beginPacket(settings.wifiBrodcast, UDP_PORT);
        udp.write(txBuffer, txBufferLength);
        udp.endPacket();
        udp.flush();
        udpTX = true;
    }
    //Frame monitoren
    if (udpTX) {
        f.monitorJSON();
    }
}
