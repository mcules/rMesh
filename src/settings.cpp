#include <EEPROM.h>
#include <nvs_flash.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Preferences.h>

#include "settings.h"
#include "config.h"
#include "version.h"
#include "webFunctions.h"
#include "hal.h"

Settings settings;
ExtSettings extSettings;
uint8_t updateChannel = 0;
bool loraEnabled = true;
std::vector<IPAddress> udpPeers;
std::vector<bool> udpPeerLegacy;
std::vector<bool> udpPeerEnabled;
std::vector<String> udpPeerCall;

std::vector<WifiNetwork> wifiNetworks;
String apName = "rMesh";
String apPassword = "";

Preferences prefs;
bool loraReady = false;
bool batteryEnabled = true;
float batteryFullVoltage = 4.2f;
bool oledEnabled = false;
char oledDisplayGroup[17] = {0};

void showSettings() {
    // Print settings as debug output
    Serial.println();
    Serial.println("Settings:");
    Serial.printf("AP Mode: %s\n", settings.apMode ? "true" : "false");
    Serial.printf("AP Name: %s\n", apName.c_str());
    Serial.printf("AP Password set: %s\n", apPassword.isEmpty() ? "false" : "true");
    if (wifiNetworks.empty()) {
        Serial.println("WiFi Networks: none");
    } else {
        for (size_t i = 0; i < wifiNetworks.size(); i++) {
            Serial.printf("WiFi %zu: %s%s (pw: %s)\n", i + 1,
                wifiNetworks[i].ssid,
                wifiNetworks[i].favorite ? " [favorite]" : "",
                (wifiNetworks[i].password[0] != '\0') ? "set" : "none");
        }
    }
    Serial.printf("DHCP: %s\n", settings.dhcpActive ? "true" : "false");
    if (!settings.dhcpActive) {
        Serial.printf("IP: %d.%d.%d.%d\n", settings.wifiIP[0], settings.wifiIP[1], settings.wifiIP[2], settings.wifiIP[3]);
        Serial.printf("Netmask: %d.%d.%d.%d\n", settings.wifiNetMask[0], settings.wifiNetMask[1], settings.wifiNetMask[2], settings.wifiNetMask[3]);
        Serial.printf("DNS: %d.%d.%d.%d\n", settings.wifiDNS[0], settings.wifiDNS[1], settings.wifiDNS[2], settings.wifiDNS[3]);
        Serial.printf("Gateway: %d.%d.%d.%d\n", settings.wifiGateway[0], settings.wifiGateway[1], settings.wifiGateway[2], settings.wifiGateway[3]);
        //Serial.printf("Broadcast: %d.%d.%d.%d\n", settings.wifiBrodcast[0], settings.wifiBrodcast[1], settings.wifiBrodcast[2], settings.wifiBrodcast[3]);
    }
    Serial.printf("NTP Server: %s\n", settings.ntpServer);
    if (udpPeers.empty()) {
        Serial.println("UDP Peers: none");
    } else {
        for (size_t i = 0; i < udpPeers.size(); i++) {
            Serial.printf("UDP Peer %zu: %d.%d.%d.%d%s%s\n", i + 1,
                udpPeers[i][0], udpPeers[i][1], udpPeers[i][2], udpPeers[i][3],
                udpPeerLegacy[i] ? " [legacy]" : "",
                (bool)udpPeerEnabled[i] ? "" : " [disabled]");
        }
    }
    Serial.println();
    Serial.printf("myCall: %s\n", settings.mycall);
    Serial.printf("position: %s\n", settings.position);
    Serial.printf("loraFrequency: %f\n", settings.loraFrequency);
    Serial.printf("loraOutputPower: %d\n", settings.loraOutputPower);
    Serial.printf("loraBandwidth: %f\n", settings.loraBandwidth);
    Serial.printf("loraSyncWord: %X\n", settings.loraSyncWord);
    Serial.printf("loraCodingRate: %d\n", settings.loraCodingRate);
    Serial.printf("loraSpreadingFactor: %d\n", settings.loraSpreadingFactor);
    Serial.printf("loraPreambleLength: %d\n", settings.loraPreambleLength);
    Serial.printf("loraRepeat: %d\n", settings.loraRepeat);
    Serial.printf("version: %s\n", VERSION);
    Serial.printf("updateChannel: %d\n", updateChannel);
    Serial.printf("maxHopMessage: %d\n", extSettings.maxHopMessage);
    Serial.printf("maxHopPosition: %d\n", extSettings.maxHopPosition);
    Serial.printf("maxHopTelemetry: %d\n", extSettings.maxHopTelemetry);
    Serial.printf("minSnr: %d dB\n", extSettings.minSnr);
    Serial.println();
    Serial.println("WiFi Status:");
    switch(WiFi.status()) {
    case 0:
        Serial.println("WL_IDLE_STATUS");
        break;
    case 1:
        Serial.println("WL_NO_SSID_AVAIL");
        break;
    case 2:
        Serial.println("WL_SCAN_COMPLETED");
        break;
    case 3:
        Serial.println("WL_CONNECTED");
        Serial.printf("IP: %d.%d.%d.%d\n", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
        Serial.printf("Netmask: %d.%d.%d.%d\n", WiFi.subnetMask()[0], WiFi.subnetMask()[1], WiFi.subnetMask()[2], WiFi.subnetMask()[3]);
        Serial.printf("Gateway: %d.%d.%d.%d\n", WiFi.gatewayIP()[0], WiFi.gatewayIP()[1], WiFi.gatewayIP()[2], WiFi.gatewayIP()[3]);
        Serial.printf("DNS: %d.%d.%d.%d\n", WiFi.dnsIP()[0], WiFi.dnsIP()[1], WiFi.dnsIP()[2], WiFi.dnsIP()[3]);
        break;
    case 4:
        Serial.println("WL_CONNECT_FAILED");
        break;
    case 5:
        Serial.println("WL_CONNECTION_LOST");
        break;
    case 6:
        Serial.println("WL_DISCONNECTED");
        break;
    case 255:
        Serial.println("WL_NO_SHIELD");
        break;
    default:
        Serial.println("WL_AP_MODE");
    }
    Serial.println();
}

void sendSettings() {
    // Send settings via WebSocket
    JsonDocument doc;
    doc["settings"]["mycall"] = settings.mycall;
    doc["settings"]["position"] = settings.position;
    doc["settings"]["ntp"] = settings.ntpServer;
    doc["settings"]["dhcpActive"] = settings.dhcpActive;
    doc["settings"]["wifiSSID"] = settings.wifiSSID;
    doc["settings"]["wifiPassword"] = settings.wifiPassword;
    doc["settings"]["apMode"] = settings.apMode;
    doc["settings"]["apName"] = apName;
    doc["settings"]["apPassword"] = apPassword;
    for (size_t i = 0; i < wifiNetworks.size(); i++) {
        JsonObject net = doc["settings"]["wifiNetworks"].add<JsonObject>();
        net["ssid"]     = wifiNetworks[i].ssid;
        net["password"] = wifiNetworks[i].password;
        net["favorite"] = wifiNetworks[i].favorite;
    }
    doc["settings"]["wifiIP"][0] = settings.wifiIP[0];
    doc["settings"]["wifiIP"][1] = settings.wifiIP[1];
    doc["settings"]["wifiIP"][2] = settings.wifiIP[2];
    doc["settings"]["wifiIP"][3] = settings.wifiIP[3];
    doc["settings"]["wifiNetMask"][0] = settings.wifiNetMask[0];
    doc["settings"]["wifiNetMask"][1] = settings.wifiNetMask[1];
    doc["settings"]["wifiNetMask"][2] = settings.wifiNetMask[2];
    doc["settings"]["wifiNetMask"][3] = settings.wifiNetMask[3];
    doc["settings"]["wifiGateway"][0] = settings.wifiGateway[0];
    doc["settings"]["wifiGateway"][1] = settings.wifiGateway[1];
    doc["settings"]["wifiGateway"][2] = settings.wifiGateway[2];
    doc["settings"]["wifiGateway"][3] = settings.wifiGateway[3];
    doc["settings"]["wifiDNS"][0] = settings.wifiDNS[0];
    doc["settings"]["wifiDNS"][1] = settings.wifiDNS[1];
    doc["settings"]["wifiDNS"][2] = settings.wifiDNS[2];
    doc["settings"]["wifiDNS"][3] = settings.wifiDNS[3];
    doc["settings"]["wifiBrodcast"][0] = settings.wifiBrodcast[0];
    doc["settings"]["wifiBrodcast"][1] = settings.wifiBrodcast[1];
    doc["settings"]["wifiBrodcast"][2] = settings.wifiBrodcast[2];
    doc["settings"]["wifiBrodcast"][3] = settings.wifiBrodcast[3];
    if (WiFi.status() == WL_CONNECTED) {
        doc["settings"]["currentIP"][0] = WiFi.localIP()[0];
        doc["settings"]["currentIP"][1] = WiFi.localIP()[1];
        doc["settings"]["currentIP"][2] = WiFi.localIP()[2];
        doc["settings"]["currentIP"][3] = WiFi.localIP()[3];
        doc["settings"]["currentNetMask"][0] = WiFi.subnetMask()[0];
        doc["settings"]["currentNetMask"][1] = WiFi.subnetMask()[1];
        doc["settings"]["currentNetMask"][2] = WiFi.subnetMask()[2];
        doc["settings"]["currentNetMask"][3] = WiFi.subnetMask()[3];
        doc["settings"]["currentGateway"][0] = WiFi.gatewayIP()[0];
        doc["settings"]["currentGateway"][1] = WiFi.gatewayIP()[1];
        doc["settings"]["currentGateway"][2] = WiFi.gatewayIP()[2];
        doc["settings"]["currentGateway"][3] = WiFi.gatewayIP()[3];
        doc["settings"]["currentDNS"][0] = WiFi.dnsIP()[0];
        doc["settings"]["currentDNS"][1] = WiFi.dnsIP()[1];
        doc["settings"]["currentDNS"][2] = WiFi.dnsIP()[2];
        doc["settings"]["currentDNS"][3] = WiFi.dnsIP()[3];
    }
    doc["settings"]["loraFrequency"] = settings.loraFrequency;
    doc["settings"]["loraOutputPower"] = settings.loraOutputPower;
    doc["settings"]["loraBandwidth"] = settings.loraBandwidth;
    doc["settings"]["loraSyncWord"] = settings.loraSyncWord;
    doc["settings"]["loraCodingRate"] = settings.loraCodingRate;
    doc["settings"]["loraSpreadingFactor"] = settings.loraSpreadingFactor;
    doc["settings"]["loraPreambleLength"] = settings.loraPreambleLength;
    doc["settings"]["version"] = VERSION;
    doc["settings"]["name"] = NAME;
    doc["settings"]["hardware"] = PIO_ENV_NAME;
    doc["settings"]["loraRepeat"] = settings.loraRepeat;
    doc["settings"]["loraMaxMessageLength"] = settings.loraMaxMessageLength;
    doc["settings"]["webPasswordSet"] = !webPasswordHash.isEmpty();
    {
        uint64_t mac = ESP.getEfuseMac();
        char chipId[13];
        snprintf(chipId, sizeof(chipId), "%02X%02X%02X%02X%02X%02X",
            (uint8_t)(mac >> 40), (uint8_t)(mac >> 32), (uint8_t)(mac >> 24),
            (uint8_t)(mac >> 16), (uint8_t)(mac >> 8), (uint8_t)(mac));
        doc["settings"]["chipId"] = chipId;
    }
    for (size_t i = 0; i < udpPeers.size(); i++) {
        JsonObject peer = doc["settings"]["udpPeers"].add<JsonObject>();
        peer["ip"][0] = udpPeers[i][0];
        peer["ip"][1] = udpPeers[i][1];
        peer["ip"][2] = udpPeers[i][2];
        peer["ip"][3] = udpPeers[i][3];
        peer["legacy"]  = (bool)udpPeerLegacy[i];
        peer["enabled"] = (bool)udpPeerEnabled[i];
        peer["call"]    = (i < udpPeerCall.size()) ? udpPeerCall[i].c_str() : "";
    }
    doc["settings"]["maxHopMessage"] = extSettings.maxHopMessage;
    doc["settings"]["maxHopPosition"] = extSettings.maxHopPosition;
    doc["settings"]["maxHopTelemetry"] = extSettings.maxHopTelemetry;
    doc["settings"]["minSnr"] = extSettings.minSnr;
    doc["settings"]["updateChannel"]      = updateChannel;
    doc["settings"]["loraEnabled"]        = loraEnabled;
    #ifdef HAS_BATTERY_ADC
    doc["settings"]["hasBattery"]         = true;
    #else
    doc["settings"]["hasBattery"]         = false;
    #endif
    doc["settings"]["batteryEnabled"]     = batteryEnabled;
    doc["settings"]["batteryFullVoltage"] = batteryFullVoltage;
    doc["settings"]["oledEnabled"]        = oledEnabled;
    doc["settings"]["oledDisplayGroup"]   = oledDisplayGroup;
    size_t bufSize = 4096 + wifiNetworks.size() * 160;
    char* jsonBuffer = (char*)malloc(bufSize);
    if (jsonBuffer == nullptr) {
        Serial.println("[OOM] sendSettings: malloc failed");
        return;
    }
    size_t len = serializeJson(doc, jsonBuffer, bufSize);
    wsBroadcast(jsonBuffer, len);
    free(jsonBuffer);
    jsonBuffer = nullptr;
}

void loadSettings() {
    // Read settings from EEPROM
    Serial.println("Loading settings...");
    prefs.begin("custom_settings", false);
    loadPasswordHash();
    prefs.getBytes("config", &settings, sizeof(settings));
    uint8_t defaultChannel = (strstr(VERSION, "-dev") != nullptr) ? 1 : 0;
    updateChannel      = prefs.getUChar("updateChannel", defaultChannel);
    loraEnabled        = prefs.getBool("loraEnabled", true);
    batteryEnabled     = prefs.getBool("batEnabled", true);
    batteryFullVoltage = prefs.getFloat("batFullV", 4.2f);
    oledEnabled        = prefs.getBool("oledEnabled", false);
    {
        String grp = prefs.getString("oledGroup", "");
        strlcpy(oledDisplayGroup, grp.c_str(), sizeof(oledDisplayGroup));
    }
    prefs.getBytes("extSettings", &extSettings, sizeof(extSettings));
    size_t storedLen = prefs.getBytesLength("config");
    size_t extSettingsLen = prefs.getBytesLength("extSettings");

    // Fix IP addresses
    settings.wifiIP       = IPAddress(settings.wifiIP[0], settings.wifiIP[1], settings.wifiIP[2], settings.wifiIP[3]);
    settings.wifiNetMask  = IPAddress(settings.wifiNetMask[0], settings.wifiNetMask[1], settings.wifiNetMask[2], settings.wifiNetMask[3]);
    settings.wifiGateway  = IPAddress(settings.wifiGateway[0], settings.wifiGateway[1], settings.wifiGateway[2], settings.wifiGateway[3]);
    settings.wifiDNS      = IPAddress(settings.wifiDNS[0], settings.wifiDNS[1], settings.wifiDNS[2], settings.wifiDNS[3]);
    settings.wifiBrodcast = IPAddress(settings.wifiBrodcast[0], settings.wifiBrodcast[1], settings.wifiBrodcast[2], settings.wifiBrodcast[3]);

    // Load defaults for extended settings / migrate old UDP peer data
    if (extSettingsLen != sizeof(extSettings)) {
        // Old format: 3 maxHop bytes + 5×16 IP strings + 5 legacy flags = 88 bytes
        const size_t OLD_EXT_SIZE = 3 + 5 * 16 + 5;
        // Previous format: 3 maxHop bytes only (before minSnr was added)
        const size_t PREV_EXT_SIZE = 3;
        size_t existingPeers = prefs.getBytesLength("udpPeers");
        if (extSettingsLen == OLD_EXT_SIZE && existingPeers == 0) {
            uint8_t* oldBuf = new uint8_t[OLD_EXT_SIZE];
            if (oldBuf != nullptr) {
                prefs.getBytes("extSettings", oldBuf, OLD_EXT_SIZE);
                extSettings.maxHopMessage   = oldBuf[0];
                extSettings.maxHopPosition  = oldBuf[1];
                extSettings.maxHopTelemetry = oldBuf[2];
                for (int i = 0; i < 5; i++) {
                    const char* ip = (const char*)(oldBuf + 3 + i * 16);
                    if (ip[0] != '\0' && strcmp(ip, "0.0.0.0") != 0) {
                        IPAddress addr;
                        if (addr.fromString(ip)) {
                            bool legacy = oldBuf[3 + 5 * 16 + i] != 0;
                            udpPeers.push_back(addr);
                            udpPeerLegacy.push_back(legacy);
                            udpPeerEnabled.push_back(true);
                            udpPeerCall.push_back("");
                        }
                    }
                }
                delete[] oldBuf;
                if (!udpPeers.empty()) {
                    saveUdpPeers();
                }
            }
        } else if (extSettingsLen == PREV_EXT_SIZE) {
            // Migrate from 3-byte format: preserve maxHop values, add default minSnr
            uint8_t tmp[PREV_EXT_SIZE];
            prefs.getBytes("extSettings", tmp, PREV_EXT_SIZE);
            extSettings.maxHopMessage   = tmp[0];
            extSettings.maxHopPosition  = tmp[1];
            extSettings.maxHopTelemetry = tmp[2];
            extSettings.minSnr          = -30;
        } else {
            extSettings.maxHopMessage = 15;
            extSettings.maxHopPosition = 1;
            extSettings.maxHopTelemetry = 3;
            extSettings.minSnr = -20;
        }
        prefs.putBytes("extSettings", &extSettings, sizeof(extSettings));
    }

    // Load AP settings
    apName     = prefs.getString("apName",     "rMesh");
    apPassword = prefs.getString("apPassword", "");

    // Load WiFi network list
    wifiNetworks.clear();
    {
        size_t wifiNetLen = prefs.getBytesLength("wifiNetworks");
        const size_t WNET_STRIDE = WIFI_NETWORK_SSID_LEN + WIFI_NETWORK_PW_LEN + 1; // 129 bytes
        if (wifiNetLen >= 1) {
            uint8_t* buf = new uint8_t[wifiNetLen];
            if (buf != nullptr) {
                prefs.getBytes("wifiNetworks", buf, wifiNetLen);
                uint8_t count = buf[0];
                for (uint8_t i = 0; i < count && 1 + (size_t)i * WNET_STRIDE + WNET_STRIDE <= wifiNetLen; i++) {
                    WifiNetwork net;
                    const uint8_t* entry = buf + 1 + i * WNET_STRIDE;
                    memset(&net, 0, sizeof(net));
                    strlcpy(net.ssid,     (const char*)(entry),                              WIFI_NETWORK_SSID_LEN);
                    strlcpy(net.password, (const char*)(entry + WIFI_NETWORK_SSID_LEN),      WIFI_NETWORK_PW_LEN);
                    net.favorite = entry[WIFI_NETWORK_SSID_LEN + WIFI_NETWORK_PW_LEN] != 0;
                    wifiNetworks.push_back(net);
                }
                delete[] buf;
            }
        }
    }

    // Load dynamic UDP peers
    udpPeers.clear();
    udpPeerLegacy.clear();
    udpPeerEnabled.clear();
    udpPeerCall.clear();
    size_t peersLen = prefs.getBytesLength("udpPeers");
    if (peersLen >= 1) {
        uint8_t* buf = new uint8_t[peersLen];
        if (buf != nullptr) {
            prefs.getBytes("udpPeers", buf, peersLen);
            uint8_t peerCount = buf[0];
            // Old format: 5 bytes per peer (without enabled), new format: 6 bytes per peer
            bool newFormat = (peersLen == 1 + (size_t)peerCount * 6);
            size_t stride = newFormat ? 6 : 5;
            for (uint8_t i = 0; i < peerCount && 1 + (size_t)i * stride + 4 < peersLen; i++) {
                udpPeers.push_back(IPAddress(buf[1+i*stride], buf[2+i*stride], buf[3+i*stride], buf[4+i*stride]));
                udpPeerLegacy.push_back(buf[5+i*stride] != 0);
                udpPeerEnabled.push_back(newFormat ? buf[6+i*stride] != 0 : true);
                udpPeerCall.push_back("");
            }
            delete[] buf;
        }
    }

    // Load defaults
    if (storedLen != sizeof(settings)) {
        strcpy(settings.wifiSSID, "");
        strcpy(settings.wifiPassword, "");
        strcpy(settings.ntpServer, "de.pool.ntp.org");
        strcpy(settings.mycall, "");
        strcpy(settings.position, "");
        settings.apMode = true;
        settings.dhcpActive = true;
        settings.wifiIP = IPAddress(192,168,33,60);
        settings.wifiNetMask = IPAddress(255,255,255,0);
        settings.wifiGateway = IPAddress(192,168,33,4);
        settings.wifiDNS = IPAddress(192,168,33,4);
        settings.wifiBrodcast = IPAddress(255,255,255,255);
        // No default frequency – RF remains disabled until the user
        // explicitly selects a band (433 MHz amateur radio or 868 MHz public).
        settings.loraFrequency = 0.0;
        settings.loraOutputPower = LORA_DEFAULT_TX_POWER;
        settings.loraBandwidth = 62.5;
        settings.loraSyncWord = AMATEUR_SYNCWORD;
        settings.loraCodingRate = 6;
        settings.loraSpreadingFactor = 7;
        settings.loraPreambleLength = 10;
        settings.loraRepeat = true;
        prefs.putBytes("config", &settings, sizeof(settings));
    }

    // Band-specific corrections after loading
    if (loraConfigured(settings.loraFrequency)) {
        // Enforce regulatory maximum for EU public band
        if (isPublicBand(settings.loraFrequency) && settings.loraOutputPower > PUBLIC_MAX_TX_POWER) {
            settings.loraOutputPower = PUBLIC_MAX_TX_POWER;
        }
    }

    // Calculate maximum message length
    settings.loraMaxMessageLength = 255 - (4 * (MAX_CALLSIGN_LENGTH + 1)) - 8;

    // Migrate legacy single WiFi to network list
    if (wifiNetworks.empty() && settings.wifiSSID[0] != '\0') {
        WifiNetwork net;
        memset(&net, 0, sizeof(net));
        strlcpy(net.ssid,     settings.wifiSSID,     sizeof(net.ssid));
        strlcpy(net.password, settings.wifiPassword,  sizeof(net.password));
        net.favorite = true;
        wifiNetworks.push_back(net);
        saveWifiNetworks();
    }

    // Reinitialize hardware
    initHal();
}

void saveWifiNetworks() {
    uint8_t count = (uint8_t)wifiNetworks.size();
    const size_t WNET_STRIDE = WIFI_NETWORK_SSID_LEN + WIFI_NETWORK_PW_LEN + 1;
    size_t bufLen = 1 + (size_t)count * WNET_STRIDE;
    uint8_t* buf = new uint8_t[bufLen];
    if (buf == nullptr) {
        Serial.println("[OOM] saveWifiNetworks: allocation failed");
        return;
    }
    memset(buf, 0, bufLen);
    buf[0] = count;
    for (uint8_t i = 0; i < count; i++) {
        uint8_t* entry = buf + 1 + i * WNET_STRIDE;
        strlcpy((char*)entry,                              wifiNetworks[i].ssid,     WIFI_NETWORK_SSID_LEN);
        strlcpy((char*)(entry + WIFI_NETWORK_SSID_LEN),    wifiNetworks[i].password, WIFI_NETWORK_PW_LEN);
        entry[WIFI_NETWORK_SSID_LEN + WIFI_NETWORK_PW_LEN] = wifiNetworks[i].favorite ? 1 : 0;
    }
    prefs.putBytes("wifiNetworks", buf, bufLen);
    delete[] buf;
    prefs.putString("apName",     apName);
    prefs.putString("apPassword", apPassword);
    sendSettings();
}

void saveUdpPeers() {
    uint8_t count = (uint8_t)udpPeers.size();
    size_t bufLen = 1 + (size_t)count * 6;
    uint8_t* buf = new uint8_t[bufLen];
    if (buf == nullptr) {
        Serial.println("[OOM] saveUdpPeers: allocation failed");
        return;
    }
    buf[0] = count;
    for (uint8_t i = 0; i < count; i++) {
        buf[1+i*6] = udpPeers[i][0];
        buf[2+i*6] = udpPeers[i][1];
        buf[3+i*6] = udpPeers[i][2];
        buf[4+i*6] = udpPeers[i][3];
        buf[5+i*6] = udpPeerLegacy[i] ? 1 : 0;
        buf[6+i*6] = udpPeerEnabled[i] ? 1 : 0;
    }
    prefs.putBytes("udpPeers", buf, bufLen);
    delete[] buf;
    sendSettings();
}

void saveOledSettings() {
    prefs.putBool("oledEnabled", oledEnabled);
    prefs.putString("oledGroup", oledDisplayGroup);
}

void saveSettings() {
    Serial.println("Saving settings...");

    // Sync settings.wifiSSID ↔ wifiNetworks (for display-device compatibility)
    if (!wifiNetworks.empty()) {
        // Check if display device changed wifiSSID (differs from current favorite)
        int favIdx = 0;
        for (size_t i = 0; i < wifiNetworks.size(); i++) {
            if (wifiNetworks[i].favorite) { favIdx = (int)i; break; }
        }
        if (settings.wifiSSID[0] != '\0' && strcmp(wifiNetworks[favIdx].ssid, settings.wifiSSID) != 0) {
            // Display changed SSID -> sync into wifiNetworks
            bool found = false;
            for (size_t i = 0; i < wifiNetworks.size(); i++) {
                if (strcmp(wifiNetworks[i].ssid, settings.wifiSSID) == 0) {
                    strlcpy(wifiNetworks[i].password, settings.wifiPassword, sizeof(wifiNetworks[i].password));
                    for (auto& n : wifiNetworks) n.favorite = false;
                    wifiNetworks[i].favorite = true;
                    found = true;
                    break;
                }
            }
            if (!found) {
                WifiNetwork net;
                memset(&net, 0, sizeof(net));
                strlcpy(net.ssid,     settings.wifiSSID,    sizeof(net.ssid));
                strlcpy(net.password, settings.wifiPassword, sizeof(net.password));
                net.favorite = true;
                for (auto& n : wifiNetworks) n.favorite = false;
                wifiNetworks.push_back(net);
            }
        } else {
            // Normal save: sync wifiSSID from wifiNetworks favorite
            strlcpy(settings.wifiSSID,     wifiNetworks[favIdx].ssid,     sizeof(settings.wifiSSID));
            strlcpy(settings.wifiPassword, wifiNetworks[favIdx].password, sizeof(settings.wifiPassword));
        }
    } else if (settings.wifiSSID[0] != '\0') {
        // No networks in list, add current SSID as favorite
        WifiNetwork net;
        memset(&net, 0, sizeof(net));
        strlcpy(net.ssid,     settings.wifiSSID,    sizeof(net.ssid));
        strlcpy(net.password, settings.wifiPassword, sizeof(net.password));
        net.favorite = true;
        wifiNetworks.push_back(net);
    }

    prefs.putBytes("config", &settings, sizeof(settings));
    prefs.putBytes("extSettings", &extSettings, sizeof(extSettings));
    prefs.putUChar("updateChannel", updateChannel);
    prefs.putBool("loraEnabled", loraEnabled);
    prefs.putBool("batEnabled", batteryEnabled);
    prefs.putFloat("batFullV", batteryFullVoltage);
    saveOledSettings();
    saveWifiNetworks();  // Saves WiFi networks + AP settings + calls sendSettings()
    saveUdpPeers();      // Saves peers (sendSettings() already called above, but ok)
    initHal();
}