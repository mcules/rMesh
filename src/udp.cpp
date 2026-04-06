#ifdef HAS_WIFI
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "udp.h"
#include "frame.h"
#include "settings.h"
#include "webFunctions.h"
#include "logging.h"


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
        // First byte is the sender's SyncWord – only accept same networks.
        // Old firmware without SyncWord prefix is treated as 433 MHz network (AMATEUR_SYNCWORD).
        if (len < 1) return false;
        bool hasSyncword = (packetBuffer[0] == AMATEUR_SYNCWORD || packetBuffer[0] == PUBLIC_SYNCWORD);
        uint8_t pktSyncword = hasSyncword ? packetBuffer[0] : AMATEUR_SYNCWORD;
        if (pktSyncword != settings.loraSyncWord) return false;

        f.importBinary(hasSyncword ? packetBuffer + 1 : packetBuffer,
                       hasSyncword ? len - 1 : len);

        // Look up sender IP in peer vector
        IPAddress senderIP = udp.remoteIP();
        int peerIdx = -1;
        for (size_t i = 0; i < udpPeers.size(); i++) {
            if (udpPeers[i] == senderIP) { peerIdx = (int)i; break; }
        }

        // Limit UDP peer list to prevent unbounded growth
        static const size_t MAX_UDP_PEERS = 50;

        if (f.frameType == Frame::FrameTypes::ANNOUNCE_FRAME && peerIdx < 0) {
            // Create new peer from broadcast announce
            if (udpPeers.size() < MAX_UDP_PEERS) {
                udpPeers.push_back(senderIP);
                udpPeerLegacy.push_back(!hasSyncword);
                udpPeerEnabled.push_back(true);
                udpPeerCall.push_back(UdpPeerCallsign(f.nodeCall));
                saveUdpPeers();
                logPrintf(LOG_INFO, "UDP", "UDP peer registered from announce: %d.%d.%d.%d (%s)",
                    senderIP[0], senderIP[1], senderIP[2], senderIP[3], f.nodeCall);
            }
        } else if (!hasSyncword) {
            // Legacy node (no SyncWord prefix)
            if (peerIdx < 0 && udpPeers.size() < MAX_UDP_PEERS) {
                udpPeers.push_back(senderIP);
                udpPeerLegacy.push_back(true);
                udpPeerEnabled.push_back(true);
                udpPeerCall.push_back(UdpPeerCallsign(f.nodeCall));
                saveUdpPeers();
                logPrintf(LOG_INFO, "UDP", "UDP legacy peer auto-registered: %d.%d.%d.%d (%s)",
                    senderIP[0], senderIP[1], senderIP[2], senderIP[3], f.nodeCall);
            } else if (!(bool)udpPeerLegacy[peerIdx]) {
                udpPeerLegacy[peerIdx] = true;
                saveUdpPeers();
            }
        }

        // Learn callsign for known peer (even if peer was already registered)
        if (peerIdx >= 0 && strlen(f.nodeCall) > 0) {
            while ((size_t)peerIdx >= udpPeerCall.size()) udpPeerCall.push_back(UdpPeerCallsign());
            if (udpPeerCall[peerIdx] != f.nodeCall) {
                udpPeerCall[peerIdx] = f.nodeCall;
                sendSettings();  // Update WebUI
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

    //Populate frame
    strncpy(f.nodeCall, settings.mycall, sizeof(f.nodeCall));
    f.tx = true;
    f.port = 1;
    f.timestamp = time(NULL);

    //Transmit
    if (strlen(f.nodeCall) == 0) {return;}
    // Prepend SyncWord as first byte so receivers can reject foreign networks
    txBuffer[0] = settings.loraSyncWord;
    txBufferLength = f.exportBinary(txBuffer + 1, sizeof(txBuffer) - 1) + 1;
    bool udpTX = false;
    for (size_t i = 0; i < udpPeers.size(); i++) {
        if (!(bool)udpPeerEnabled[i]) continue;  // Skip disabled peers
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
    // Always send announces via broadcast too (so new nodes are discovered)
    if (f.frameType == Frame::FrameTypes::ANNOUNCE_FRAME) {
        udp.beginPacket(settings.wifiBrodcast, UDP_PORT);
        udp.write(txBuffer, txBufferLength);
        udp.endPacket();
        udp.flush();
        udpTX = true;
    }
    //Monitor frame
    if (udpTX) {
        f.monitorJSON();
    }
}

#endif // HAS_WIFI
