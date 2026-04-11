#ifdef HAS_WIFI
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "udp.h"
#include "frame.h"
#include "settings.h"
#include "webFunctions.h"
#include "ethFunctions.h"
#include "logging.h"

#ifdef HAS_ETHERNET
#include <ETH.h>
#endif


WiFiUDP udp;

// ── Per-interface helpers ────────────────────────────────────────────────────

static bool inSubnet(IPAddress ip, IPAddress ifIP, IPAddress mask) {
    for (int i = 0; i < 4; i++) {
        if ((ip[i] & mask[i]) != (ifIP[i] & mask[i])) return false;
    }
    return true;
}

/** Returns true if node-communication (UDP) is allowed for the given remote IP. */
static bool nodeCommAllowed(IPAddress remoteIP) {
#ifdef HAS_ETHERNET
    // If both interfaces are up, classify by subnet
    if (ethConnected && WiFi.status() == WL_CONNECTED) {
        if (inSubnet(remoteIP, ETH.localIP(), ETH.subnetMask()))  return ethNodeComm;
        if (inSubnet(remoteIP, WiFi.localIP(), WiFi.subnetMask())) return wifiNodeComm;
        // Unknown subnet (e.g. routed traffic) — allow if at least one flag is set
        return wifiNodeComm || ethNodeComm;
    }
    if (ethConnected)                     return ethNodeComm;
#endif
    return wifiNodeComm;
}

void initUDP() {
    udp.begin(UDP_PORT);
    esp_log_level_set("NetworkUdp", ESP_LOG_NONE);
}


bool checkUDP(Frame &f) {
    if (!loraConfigured(settings.loraFrequency)) return false;
    bool anyUp = (WiFi.status() == WL_CONNECTED);
#ifdef HAS_ETHERNET
    anyUp = anyUp || ethConnected;
#endif
    if (!anyUp) return false;
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

        // Per-interface node-communication filter
        IPAddress senderIP = udp.remoteIP();
        if (!nodeCommAllowed(senderIP)) return false;

        f.importBinary(hasSyncword ? packetBuffer + 1 : packetBuffer,
                       hasSyncword ? len - 1 : len);

        // Look up sender IP in peer vector
        int peerIdx = -1;
        for (size_t i = 0; i < udpPeers.size(); i++) {
            if (udpPeers[i] == senderIP) { peerIdx = (int)i; break; }
        }

        // Ignore incoming traffic from disabled peers
        if (peerIdx >= 0 && !(bool)udpPeerEnabled[peerIdx]) return false;

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
#ifdef HAS_ETHERNET
        // Classify by interface: port 1 = WiFi, port 2 = LAN (Ethernet)
        if (ethConnected && inSubnet(senderIP, ETH.localIP(), ETH.subnetMask()))
            f.port = 2;
        else
            f.port = 1;
#else
        f.port = 1;
#endif
        return true;
    }
    return false;
}

/** Check whether a UDP peer's IP belongs to the interface indicated by port. */
static bool peerMatchesPort(IPAddress peerIP, uint8_t port) {
#ifdef HAS_ETHERNET
    if (port == 2) {
        // LAN: peer must be in ETH subnet
        if (!ethConnected) return false;
        return inSubnet(peerIP, ETH.localIP(), ETH.subnetMask());
    }
    if (port == 1 && ethConnected && WiFi.status() == WL_CONNECTED) {
        // WiFi: peer must NOT be in ETH subnet (i.e. reachable via WiFi)
        return !inSubnet(peerIP, ETH.localIP(), ETH.subnetMask());
    }
#endif
    // Single interface or port 1 without ETH — all peers match
    (void)peerIP;
    return (port == 1);
}

void sendUDP(Frame &f) {
    if (!loraConfigured(settings.loraFrequency)) return;

    uint8_t txBuffer[255];
    size_t txBufferLength;

    //Populate frame
    strncpy(f.nodeCall, settings.mycall, sizeof(f.nodeCall));
    f.tx = true;
    // Keep f.port as-is (1 = WiFi, 2 = LAN) — set by caller / TX buffer
    f.timestamp = time(NULL);

    //Transmit
    if (strlen(f.nodeCall) == 0) {return;}
    // Prepend SyncWord as first byte so receivers can reject foreign networks
    txBuffer[0] = settings.loraSyncWord;
    txBufferLength = f.exportBinary(txBuffer + 1, sizeof(txBuffer) - 1) + 1;
    bool udpTX = false;

    // Unicast to known peers that belong to this interface
    for (size_t i = 0; i < udpPeers.size(); i++) {
        if (!(bool)udpPeerEnabled[i]) continue;
        if (!peerMatchesPort(udpPeers[i], f.port)) continue;
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

    // Broadcast announces on the matching interface only
    if (f.frameType == Frame::FrameTypes::ANNOUNCE_FRAME) {
        if (f.port == 1 && wifiNodeComm && WiFi.status() == WL_CONNECTED) {
            udp.beginPacket(settings.wifiBrodcast, UDP_PORT);
            udp.write(txBuffer, txBufferLength);
            udp.endPacket();
            udp.flush();
            udpTX = true;
        }
#ifdef HAS_ETHERNET
        if (f.port == 2 && ethNodeComm && ethConnected) {
            IPAddress ethBcast;
            for (int i = 0; i < 4; i++)
                ethBcast[i] = ETH.localIP()[i] | ~ETH.subnetMask()[i];
            udp.beginPacket(ethBcast, UDP_PORT);
            udp.write(txBuffer, txBufferLength);
            udp.endPacket();
            udp.flush();
            udpTX = true;
        }
#endif
    }

    //Monitor frame
    if (udpTX) {
        f.monitorJSON();
    }
}

#endif // HAS_WIFI
