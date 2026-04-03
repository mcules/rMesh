#include <Arduino.h>
#include <ArduinoJson.h>

#ifdef HAS_WIFI
#include <EEPROM.h>
#include <nvs_flash.h>
#include <WiFi.h>
#endif

#include "settings.h"
#include "config.h"
#include "version.h"
#include "webFunctions.h"
#include "hal.h"
#include "serial.h"
#include "main.h"
#include "logging.h"

Settings settings;
ExtSettings extSettings;
uint8_t updateChannel = 0;
bool loraEnabled = true;

#ifdef HAS_WIFI
std::vector<IPAddress> udpPeers;
std::vector<bool> udpPeerLegacy;
std::vector<bool> udpPeerEnabled;
std::vector<String> udpPeerCall;

std::vector<WifiNetwork> wifiNetworks;
String apName = "rMesh";
String apPassword = "";
#endif

Preferences prefs;
bool loraReady = false;
bool batteryEnabled = true;
float batteryFullVoltage = 4.2f;
int8_t wifiTxPower = WIFI_MAX_TX_POWER_DBM;
uint8_t displayBrightness = 200;
uint16_t cpuFrequency = 240;
bool oledEnabled = false;
char oledDisplayGroup[17] = {0};

// webPasswordHash is defined in auth.cpp for both WiFi and non-WiFi builds

char groupNames[MAX_CHANNELS + 1][MAX_GROUP_NAME_LEN] = {0};  // index 1-10

void saveGroupNames() {
    for (int i = 3; i <= MAX_CHANNELS; i++) {
        char key[10];
        snprintf(key, sizeof(key), "grpName%d", i);
        prefs.putString(key, groupNames[i]);
    }
}

void loadGroupNames() {
    memset(groupNames, 0, sizeof(groupNames));
    for (int i = 3; i <= MAX_CHANNELS; i++) {
        char key[10];
        snprintf(key, sizeof(key), "grpName%d", i);
        String name = prefs.getString(key, "");
        strlcpy(groupNames[i], name.c_str(), MAX_GROUP_NAME_LEN);
    }
}

void showSettings() {
    logPrintf(LOG_INFO, "Settings", "");
    logPrintf(LOG_INFO, "Settings", "Settings:");
#ifdef HAS_WIFI
    logPrintf(LOG_INFO, "Settings", "AP Mode: %s", settings.apMode ? "true" : "false");
    logPrintf(LOG_INFO, "Settings", "AP Name: %s", apName.c_str());
    logPrintf(LOG_INFO, "Settings", "AP Password set: %s", apPassword.isEmpty() ? "false" : "true");
    if (wifiNetworks.empty()) {
        logPrintf(LOG_INFO, "Settings", "WiFi Networks: none");
    } else {
        for (size_t i = 0; i < wifiNetworks.size(); i++) {
            logPrintf(LOG_INFO, "Settings", "WiFi %zu: %s%s (pw: %s)", i + 1,
                wifiNetworks[i].ssid,
                wifiNetworks[i].favorite ? " [favorite]" : "",
                (wifiNetworks[i].password[0] != '\0') ? "set" : "none");
        }
    }
    logPrintf(LOG_INFO, "Settings", "DHCP: %s", settings.dhcpActive ? "true" : "false");
    if (!settings.dhcpActive) {
        logPrintf(LOG_INFO, "Settings", "IP: %d.%d.%d.%d", settings.wifiIP[0], settings.wifiIP[1], settings.wifiIP[2], settings.wifiIP[3]);
        logPrintf(LOG_INFO, "Settings", "Netmask: %d.%d.%d.%d", settings.wifiNetMask[0], settings.wifiNetMask[1], settings.wifiNetMask[2], settings.wifiNetMask[3]);
        logPrintf(LOG_INFO, "Settings", "DNS: %d.%d.%d.%d", settings.wifiDNS[0], settings.wifiDNS[1], settings.wifiDNS[2], settings.wifiDNS[3]);
        logPrintf(LOG_INFO, "Settings", "Gateway: %d.%d.%d.%d", settings.wifiGateway[0], settings.wifiGateway[1], settings.wifiGateway[2], settings.wifiGateway[3]);
    }
    logPrintf(LOG_INFO, "Settings", "NTP Server: %s", settings.ntpServer);
    if (udpPeers.empty()) {
        logPrintf(LOG_INFO, "Settings", "UDP Peers: none");
    } else {
        for (size_t i = 0; i < udpPeers.size(); i++) {
            logPrintf(LOG_INFO, "Settings", "UDP Peer %zu: %d.%d.%d.%d%s%s", i + 1,
                udpPeers[i][0], udpPeers[i][1], udpPeers[i][2], udpPeers[i][3],
                udpPeerLegacy[i] ? " [legacy]" : "",
                (bool)udpPeerEnabled[i] ? "" : " [disabled]");
        }
    }
#endif
    logPrintf(LOG_INFO, "Settings", "");
    logPrintf(LOG_INFO, "Settings", "myCall: %s", settings.mycall);
    logPrintf(LOG_INFO, "Settings", "position: %s", settings.position);
    logPrintf(LOG_INFO, "Settings", "loraFrequency: %f", settings.loraFrequency);
    logPrintf(LOG_INFO, "Settings", "loraOutputPower: %d", settings.loraOutputPower);
    logPrintf(LOG_INFO, "Settings", "loraBandwidth: %f", settings.loraBandwidth);
    logPrintf(LOG_INFO, "Settings", "loraSyncWord: %X", settings.loraSyncWord);
    logPrintf(LOG_INFO, "Settings", "loraCodingRate: %d", settings.loraCodingRate);
    logPrintf(LOG_INFO, "Settings", "loraSpreadingFactor: %d", settings.loraSpreadingFactor);
    logPrintf(LOG_INFO, "Settings", "loraPreambleLength: %d", settings.loraPreambleLength);
    logPrintf(LOG_INFO, "Settings", "loraRepeat: %d", settings.loraRepeat);
    logPrintf(LOG_INFO, "Settings", "version: %s", VERSION);
    logPrintf(LOG_INFO, "Settings", "updateChannel: %d", updateChannel);
    logPrintf(LOG_INFO, "Settings", "maxHopMessage: %d", extSettings.maxHopMessage);
    logPrintf(LOG_INFO, "Settings", "maxHopPosition: %d", extSettings.maxHopPosition);
    logPrintf(LOG_INFO, "Settings", "maxHopTelemetry: %d", extSettings.maxHopTelemetry);
    logPrintf(LOG_INFO, "Settings", "minSnr: %d dB", extSettings.minSnr);
    logPrintf(LOG_INFO, "Settings", "");
#ifdef HAS_WIFI
    logPrintf(LOG_INFO, "Settings", "WiFi Status:");
    switch(WiFi.status()) {
    case 0: logPrintf(LOG_INFO, "Settings", "WL_IDLE_STATUS"); break;
    case 1: logPrintf(LOG_INFO, "Settings", "WL_NO_SSID_AVAIL"); break;
    case 2: logPrintf(LOG_INFO, "Settings", "WL_SCAN_COMPLETED"); break;
    case 3:
        logPrintf(LOG_INFO, "Settings", "WL_CONNECTED");
        logPrintf(LOG_INFO, "Settings", "IP: %d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
        logPrintf(LOG_INFO, "Settings", "Netmask: %d.%d.%d.%d", WiFi.subnetMask()[0], WiFi.subnetMask()[1], WiFi.subnetMask()[2], WiFi.subnetMask()[3]);
        logPrintf(LOG_INFO, "Settings", "Gateway: %d.%d.%d.%d", WiFi.gatewayIP()[0], WiFi.gatewayIP()[1], WiFi.gatewayIP()[2], WiFi.gatewayIP()[3]);
        logPrintf(LOG_INFO, "Settings", "DNS: %d.%d.%d.%d", WiFi.dnsIP()[0], WiFi.dnsIP()[1], WiFi.dnsIP()[2], WiFi.dnsIP()[3]);
        break;
    case 4: logPrintf(LOG_INFO, "Settings", "WL_CONNECT_FAILED"); break;
    case 5: logPrintf(LOG_INFO, "Settings", "WL_CONNECTION_LOST"); break;
    case 6: logPrintf(LOG_INFO, "Settings", "WL_DISCONNECTED"); break;
    case 255: logPrintf(LOG_INFO, "Settings", "WL_NO_SHIELD"); break;
    default: logPrintf(LOG_INFO, "Settings", "WL_AP_MODE");
    }
#else
    logPrintf(LOG_INFO, "Settings", "WiFi: not available (nRF52)");
#endif
    logPrintf(LOG_INFO, "Settings", "");
}

void sendSettings() {
#ifdef HAS_WIFI
    // Send settings via WebSocket
    JsonDocument doc;
    doc["settings"]["mycall"] = settings.mycall;
    doc["settings"]["position"] = settings.position;
    doc["settings"]["ntp"] = settings.ntpServer;
    doc["settings"]["dhcpActive"] = settings.dhcpActive;
    doc["settings"]["wifiSSID"] = settings.wifiSSID;
    doc["settings"]["wifiPassword"] = (settings.wifiPassword[0] != '\0') ? "***" : "";
    doc["settings"]["apMode"] = settings.apMode;
    doc["settings"]["apName"] = apName;
    doc["settings"]["apPassword"] = apPassword;
    for (size_t i = 0; i < wifiNetworks.size(); i++) {
        JsonObject net = doc["settings"]["wifiNetworks"].add<JsonObject>();
        net["ssid"]     = wifiNetworks[i].ssid;
        net["password"] = (wifiNetworks[i].password[0] != '\0') ? "***" : "";
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
    doc["settings"]["wifiTxPower"]        = wifiTxPower;
    doc["settings"]["wifiMaxTxPower"]     = WIFI_MAX_TX_POWER_DBM;
    doc["settings"]["displayBrightness"]  = displayBrightness;
    doc["settings"]["cpuFrequency"]       = cpuFrequency;
    doc["settings"]["oledEnabled"]        = oledEnabled;
    doc["settings"]["serialDebug"]        = serialDebug;
    doc["settings"]["oledDisplayGroup"]   = oledDisplayGroup;
    if (!webPasswordHash.isEmpty()) {
        doc["settings"]["otaToken"] = webPasswordHash;
    }
    for (int i = 3; i <= MAX_CHANNELS; i++) {
        doc["settings"]["groupNames"][String(i)] = groupNames[i];
    }
    size_t bufSize = 4096 + wifiNetworks.size() * 160 + udpPeers.size() * 100;
    char* jsonBuffer = (char*)malloc(bufSize);
    if (jsonBuffer == nullptr) {
        logPrintf(LOG_ERROR, "Settings", "sendSettings: malloc failed");
        return;
    }
    size_t len = serializeJson(doc, jsonBuffer, bufSize);
    wsBroadcast(jsonBuffer, len);
    free(jsonBuffer);
    jsonBuffer = nullptr;
#else
    // On non-WiFi boards, settings are only shown via serial
    (void)0;
#endif
}

void loadSettings() {
    logPrintf(LOG_INFO, "Settings", "Loading settings...");
    prefs.begin("custom_settings", false);
#ifdef HAS_WIFI
    loadPasswordHash();
#endif
    prefs.getBytes("config", &settings, sizeof(settings));
    uint8_t defaultChannel = (strstr(VERSION, "-dev") != nullptr) ? 1 : 0;
    updateChannel      = prefs.getUChar("updateChannel", defaultChannel);
    loraEnabled        = prefs.getBool("loraEnabled", true);
    batteryEnabled     = prefs.getBool("batEnabled", true);
    batteryFullVoltage = prefs.getFloat("batFullV", 4.2f);
    wifiTxPower        = prefs.getChar("wifiTxPow", WIFI_MAX_TX_POWER_DBM);
    if (wifiTxPower < 2) wifiTxPower = 2;
    if (wifiTxPower > WIFI_MAX_TX_POWER_DBM) wifiTxPower = WIFI_MAX_TX_POWER_DBM;
    displayBrightness  = prefs.getUChar("dispBrightW", 200);
    cpuFrequency       = prefs.getUChar("cpuFreq", 240);
    if (cpuFrequency != 80 && cpuFrequency != 160 && cpuFrequency != 240) cpuFrequency = 240;
    oledEnabled        = prefs.getBool("oledEnabled", false);
    serialDebug        = prefs.getBool("serialDebug", false);
    {
        String grp = prefs.getString("oledGroup", "");
        strlcpy(oledDisplayGroup, grp.c_str(), sizeof(oledDisplayGroup));
    }
    loadGroupNames();
    prefs.getBytes("extSettings", &extSettings, sizeof(extSettings));
    size_t storedLen = prefs.getBytesLength("config");
    size_t extSettingsLen = prefs.getBytesLength("extSettings");

#ifdef HAS_WIFI
    // Fix IP addresses
    settings.wifiIP       = IPAddress(settings.wifiIP[0], settings.wifiIP[1], settings.wifiIP[2], settings.wifiIP[3]);
    settings.wifiNetMask  = IPAddress(settings.wifiNetMask[0], settings.wifiNetMask[1], settings.wifiNetMask[2], settings.wifiNetMask[3]);
    settings.wifiGateway  = IPAddress(settings.wifiGateway[0], settings.wifiGateway[1], settings.wifiGateway[2], settings.wifiGateway[3]);
    settings.wifiDNS      = IPAddress(settings.wifiDNS[0], settings.wifiDNS[1], settings.wifiDNS[2], settings.wifiDNS[3]);
    settings.wifiBrodcast = IPAddress(settings.wifiBrodcast[0], settings.wifiBrodcast[1], settings.wifiBrodcast[2], settings.wifiBrodcast[3]);
#endif

    // Load defaults for extended settings / migrate old UDP peer data
    if (extSettingsLen != sizeof(extSettings)) {
#ifdef HAS_WIFI
        const size_t OLD_EXT_SIZE = 3 + 5 * 16 + 5;
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
            uint8_t tmp[PREV_EXT_SIZE];
            prefs.getBytes("extSettings", tmp, PREV_EXT_SIZE);
            extSettings.maxHopMessage   = tmp[0];
            extSettings.maxHopPosition  = tmp[1];
            extSettings.maxHopTelemetry = tmp[2];
            extSettings.minSnr          = -30;
        } else
#endif
        {
            extSettings.maxHopMessage = 15;
            extSettings.maxHopPosition = 1;
            extSettings.maxHopTelemetry = 3;
            extSettings.minSnr = -30;
        }
        prefs.putBytes("extSettings", &extSettings, sizeof(extSettings));
    }

#ifdef HAS_WIFI
    // Load AP settings
    apName     = prefs.getString("apName",     "rMesh");
    apPassword = prefs.getString("apPassword", "");

    // Load WiFi network list
    wifiNetworks.clear();
    {
        size_t wifiNetLen = prefs.getBytesLength("wifiNetworks");
        const size_t WNET_STRIDE = WIFI_NETWORK_SSID_LEN + WIFI_NETWORK_PW_LEN + 1;
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
#endif

    // Load defaults
    if (storedLen != sizeof(settings)) {
        strcpy(settings.wifiSSID, "");
        strcpy(settings.wifiPassword, "");
        strcpy(settings.ntpServer, "de.pool.ntp.org");
        strcpy(settings.mycall, "");
        strcpy(settings.position, "");
        settings.apMode = true;
        settings.dhcpActive = true;
#ifdef HAS_WIFI
        settings.wifiIP = IPAddress(192,168,33,60);
        settings.wifiNetMask = IPAddress(255,255,255,0);
        settings.wifiGateway = IPAddress(192,168,33,4);
        settings.wifiDNS = IPAddress(192,168,33,4);
        settings.wifiBrodcast = IPAddress(255,255,255,255);
#endif
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
        if (isPublicBand(settings.loraFrequency) && settings.loraOutputPower > PUBLIC_MAX_TX_POWER) {
            settings.loraOutputPower = PUBLIC_MAX_TX_POWER;
        }
    }

    // Calculate maximum message length
    settings.loraMaxMessageLength = 255 - (4 * (MAX_CALLSIGN_LENGTH + 1)) - 8;

#ifdef HAS_WIFI
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
#endif

    pendingLoraReinit = true;
}

#ifdef HAS_WIFI
void saveWifiNetworks() {
    // Limit stored networks to prevent NVS overflow
    const size_t MAX_WIFI_NETWORKS = 20;
    if (wifiNetworks.size() > MAX_WIFI_NETWORKS) {
        wifiNetworks.resize(MAX_WIFI_NETWORKS);
    }
    uint8_t count = (uint8_t)wifiNetworks.size();
    const size_t WNET_STRIDE = WIFI_NETWORK_SSID_LEN + WIFI_NETWORK_PW_LEN + 1;
    size_t bufLen = 1 + (size_t)count * WNET_STRIDE;
    uint8_t* buf = new uint8_t[bufLen];
    if (buf == nullptr) {
        logPrintf(LOG_ERROR, "Settings", "saveWifiNetworks: allocation failed");
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
        logPrintf(LOG_ERROR, "Settings", "saveUdpPeers: allocation failed");
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
#endif

void saveOledSettings() {
    prefs.putBool("oledEnabled", oledEnabled);
    prefs.putString("oledGroup", oledDisplayGroup);
}

void saveSettings() {
    logPrintf(LOG_INFO, "Settings", "Saving settings...");

#ifdef HAS_WIFI
    // Sync settings.wifiSSID <-> wifiNetworks
    if (!wifiNetworks.empty()) {
        int favIdx = 0;
        for (size_t i = 0; i < wifiNetworks.size(); i++) {
            if (wifiNetworks[i].favorite) { favIdx = (int)i; break; }
        }
        if (settings.wifiSSID[0] != '\0' && strcmp(wifiNetworks[favIdx].ssid, settings.wifiSSID) != 0) {
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
            strlcpy(settings.wifiSSID,     wifiNetworks[favIdx].ssid,     sizeof(settings.wifiSSID));
            strlcpy(settings.wifiPassword, wifiNetworks[favIdx].password, sizeof(settings.wifiPassword));
        }
    } else if (settings.wifiSSID[0] != '\0') {
        WifiNetwork net;
        memset(&net, 0, sizeof(net));
        strlcpy(net.ssid,     settings.wifiSSID,    sizeof(net.ssid));
        strlcpy(net.password, settings.wifiPassword, sizeof(net.password));
        net.favorite = true;
        wifiNetworks.push_back(net);
    }
#endif

    size_t written = prefs.putBytes("config", &settings, sizeof(settings));
    if (written != sizeof(settings)) {
        logPrintf(LOG_ERROR, "Settings", "saveSettings: putBytes(\"config\") wrote %u of %u bytes",
                       (unsigned)written, (unsigned)sizeof(settings));
    }
    prefs.putBytes("extSettings", &extSettings, sizeof(extSettings));
    prefs.putUChar("updateChannel", updateChannel);
    prefs.putBool("loraEnabled", loraEnabled);
    prefs.putBool("batEnabled", batteryEnabled);
    prefs.putFloat("batFullV", batteryFullVoltage);
    prefs.putChar("wifiTxPow", wifiTxPower);
    prefs.putUChar("dispBrightW", displayBrightness);
    prefs.putUChar("cpuFreq", cpuFrequency);
    prefs.putBool("serialDebug", serialDebug);
    saveOledSettings();
#ifdef HAS_WIFI
    saveWifiNetworks();
    saveUdpPeers();
#endif
    pendingLoraReinit = true;
}
