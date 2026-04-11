#include <Arduino.h>
#include <ArduinoJson.h>

#ifdef HAS_WIFI
#include <EEPROM.h>
#include <nvs_flash.h>
#include <WiFi.h>
#endif

#include "settings.h"
#include "heapdbg.h"
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
std::vector<UdpPeerCallsign> udpPeerCall;

std::vector<WifiNetwork> wifiNetworks;
String apName = "rMesh";
String apPassword = "";
#endif

Preferences prefs;
bool loraReady = false;
bool batteryEnabled = true;
float batteryFullVoltage = 4.2f;
int8_t wifiTxPower = WIFI_MAX_TX_POWER_DBM;

#ifdef HAS_WIFI
// WiFi enable (only meaningful on boards with Ethernet as fallback)
bool wifiEnabled     = true;
// Ethernet settings (defaults for first boot)
bool ethEnabled      = true;
bool ethDhcp         = true;
IPAddress ethIP(10, 0, 0, 100);
IPAddress ethNetMask(255, 255, 255, 0);
IPAddress ethGateway(10, 0, 0, 1);
IPAddress ethDNS(10, 0, 0, 1);

// Per-interface service flags
bool wifiNodeComm = true;
bool wifiWebUI    = true;
bool ethNodeComm  = true;
bool ethWebUI     = true;
uint8_t primaryInterface = 0;  // 0=auto, 1=WiFi, 2=LAN
#endif
uint8_t displayBrightness = 200;
uint16_t cpuFrequency = 240;
bool oledEnabled = false;
char oledDisplayGroup[17] = {0};
uint16_t oledPageInterval = 5000;
uint8_t  oledPageMask     = 0xFF;
int8_t   oledButtonPin    = -1;

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
    logRaw("");
    logRaw("Settings:");
    logRaw("  myCall: %s", settings.mycall);
    logRaw("  position: %s", settings.position);
    logRaw("  version: %s", VERSION);
    logRaw("  updateChannel: %d", updateChannel);
    logRaw("");
#ifdef HAS_WIFI
    logRaw("  WiFi:");
    logRaw("    wifiEnabled: %s", wifiEnabled ? "true" : "false");
    logRaw("    apMode: %s", settings.apMode ? "true" : "false");
    logRaw("    apName: %s", apName.c_str());
    logRaw("    apPassword: %s", apPassword.isEmpty() ? "not set" : "set");
    if (wifiNetworks.empty()) {
        logRaw("    networks: none");
    } else {
        for (size_t i = 0; i < wifiNetworks.size(); i++) {
            logRaw("    network %zu: %s%s (pw: %s)", i + 1,
                wifiNetworks[i].ssid,
                wifiNetworks[i].favorite ? " [favorite]" : "",
                (wifiNetworks[i].password[0] != '\0') ? "set" : "none");
        }
    }
    logRaw("    dhcp: %s", settings.dhcpActive ? "true" : "false");
    if (!settings.dhcpActive) {
        logRaw("    ip: %d.%d.%d.%d", settings.wifiIP[0], settings.wifiIP[1], settings.wifiIP[2], settings.wifiIP[3]);
        logRaw("    netmask: %d.%d.%d.%d", settings.wifiNetMask[0], settings.wifiNetMask[1], settings.wifiNetMask[2], settings.wifiNetMask[3]);
        logRaw("    dns: %d.%d.%d.%d", settings.wifiDNS[0], settings.wifiDNS[1], settings.wifiDNS[2], settings.wifiDNS[3]);
        logRaw("    gateway: %d.%d.%d.%d", settings.wifiGateway[0], settings.wifiGateway[1], settings.wifiGateway[2], settings.wifiGateway[3]);
    }
    logRaw("    ntpServer: %s", settings.ntpServer);
    logRaw("    txPower: %d dBm", wifiTxPower);
    logRaw("    wifiNodeComm: %s", wifiNodeComm ? "true" : "false");
    logRaw("    wifiWebUI: %s", wifiWebUI ? "true" : "false");
    logRaw("");
#ifdef HAS_ETHERNET
    logRaw("  Ethernet:");
    logRaw("    ethEnabled: %s", ethEnabled ? "true" : "false");
    logRaw("    ethDhcp: %s", ethDhcp ? "true" : "false");
    if (!ethDhcp) {
        logRaw("    ethIP: %d.%d.%d.%d", ethIP[0], ethIP[1], ethIP[2], ethIP[3]);
        logRaw("    ethNetMask: %d.%d.%d.%d", ethNetMask[0], ethNetMask[1], ethNetMask[2], ethNetMask[3]);
        logRaw("    ethGateway: %d.%d.%d.%d", ethGateway[0], ethGateway[1], ethGateway[2], ethGateway[3]);
        logRaw("    ethDNS: %d.%d.%d.%d", ethDNS[0], ethDNS[1], ethDNS[2], ethDNS[3]);
    }
    logRaw("    ethNodeComm: %s", ethNodeComm ? "true" : "false");
    logRaw("    ethWebUI: %s", ethWebUI ? "true" : "false");
    logRaw("");
#endif
    logRaw("    primaryInterface: %d (%s)", primaryInterface,
           primaryInterface == 0 ? "auto" : (primaryInterface == 1 ? "WiFi" : "LAN"));
    if (udpPeers.empty()) {
        logRaw("    udpPeers: none");
    } else {
        for (size_t i = 0; i < udpPeers.size(); i++) {
            logRaw("    udpPeer %zu: %d.%d.%d.%d%s%s", i + 1,
                udpPeers[i][0], udpPeers[i][1], udpPeers[i][2], udpPeers[i][3],
                udpPeerLegacy[i] ? " [legacy]" : "",
                (bool)udpPeerEnabled[i] ? "" : " [disabled]");
        }
    }
    logRaw("");
#endif
    logRaw("  LoRa:");
    logRaw("    loraEnabled: %s", loraEnabled ? "true" : "false");
    logRaw("    loraReady: %s", loraReady ? "true" : "false");
    logRaw("    frequency: %.3f MHz", settings.loraFrequency);
    logRaw("    outputPower: %d dBm", settings.loraOutputPower);
    logRaw("    bandwidth: %.2f kHz", settings.loraBandwidth);
    logRaw("    syncWord: 0x%02X", settings.loraSyncWord);
    logRaw("    codingRate: %d", settings.loraCodingRate);
    logRaw("    spreadingFactor: %d", settings.loraSpreadingFactor);
    logRaw("    preambleLength: %d", settings.loraPreambleLength);
    logRaw("    repeat: %s", settings.loraRepeat ? "true" : "false");
    logRaw("");
    logRaw("  Mesh:");
    logRaw("    maxHopMessage: %d", extSettings.maxHopMessage);
    logRaw("    maxHopPosition: %d", extSettings.maxHopPosition);
    logRaw("    maxHopTelemetry: %d", extSettings.maxHopTelemetry);
    logRaw("    minSnr: %d dB", extSettings.minSnr);
    logRaw("");
    logRaw("  System:");
    logRaw("    serialDebug: %s", serialDebug ? "true" : "false");
    logRaw("    batteryEnabled: %s", batteryEnabled ? "true" : "false");
    logRaw("    batteryFullVoltage: %.2f V", batteryFullVoltage);
    logRaw("    displayBrightness: %d", displayBrightness);
    logRaw("    cpuFrequency: %d MHz", cpuFrequency);
    logRaw("");
#ifdef HAS_WIFI
    logRaw("  WiFi Status:");
    switch(WiFi.status()) {
    case 0: logRaw("    WL_IDLE_STATUS"); break;
    case 1: logRaw("    WL_NO_SSID_AVAIL"); break;
    case 2: logRaw("    WL_SCAN_COMPLETED"); break;
    case 3:
        logRaw("    WL_CONNECTED");
        logRaw("    IP: %d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
        logRaw("    Netmask: %d.%d.%d.%d", WiFi.subnetMask()[0], WiFi.subnetMask()[1], WiFi.subnetMask()[2], WiFi.subnetMask()[3]);
        logRaw("    Gateway: %d.%d.%d.%d", WiFi.gatewayIP()[0], WiFi.gatewayIP()[1], WiFi.gatewayIP()[2], WiFi.gatewayIP()[3]);
        logRaw("    DNS: %d.%d.%d.%d", WiFi.dnsIP()[0], WiFi.dnsIP()[1], WiFi.dnsIP()[2], WiFi.dnsIP()[3]);
        break;
    case 4: logRaw("    WL_CONNECT_FAILED"); break;
    case 5: logRaw("    WL_CONNECTION_LOST"); break;
    case 6: logRaw("    WL_DISCONNECTED"); break;
    case 255: logRaw("    WL_NO_SHIELD"); break;
    default: logRaw("    WL_AP_MODE");
    }
#else
    logRaw("  WiFi: not available (nRF52)");
#endif
    logPrintf(LOG_INFO, "Settings", "");
}

void sendSettings() {
#ifdef HAS_WIFI
    // Replaced: no longer serializes full JSON on heap (~17 KB malloc).
    // Sends lightweight notification; WebUI fetches /api/settings instead.
    notifySettingsChanged();
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
    heapDebugEnabled   = prefs.getBool("heapDebug", false);
    {
        String grp = prefs.getString("oledGroup", "");
        strlcpy(oledDisplayGroup, grp.c_str(), sizeof(oledDisplayGroup));
    }
    oledPageInterval = prefs.getUShort("oledPageIv", 5000);
    if (oledPageInterval < 1000) oledPageInterval = 1000;
    oledPageMask     = prefs.getUChar("oledPageMask", 0xFF);
    if (oledPageMask == 0) oledPageMask = 0xFF;
    oledButtonPin    = prefs.getChar("oledBtnPin", -1);

#ifdef HAS_WIFI
    // Ethernet & per-interface settings
    wifiEnabled  = prefs.getBool("wifiEnabled", true);
    ethEnabled   = prefs.getBool("ethEnabled", true);
    ethDhcp      = prefs.getBool("ethDhcp", true);
    {
        uint8_t ipBuf[4];
        if (prefs.getBytes("ethIP", ipBuf, 4) == 4)
            ethIP = IPAddress(ipBuf[0], ipBuf[1], ipBuf[2], ipBuf[3]);
        if (prefs.getBytes("ethNetMask", ipBuf, 4) == 4)
            ethNetMask = IPAddress(ipBuf[0], ipBuf[1], ipBuf[2], ipBuf[3]);
        if (prefs.getBytes("ethGateway", ipBuf, 4) == 4)
            ethGateway = IPAddress(ipBuf[0], ipBuf[1], ipBuf[2], ipBuf[3]);
        if (prefs.getBytes("ethDNS", ipBuf, 4) == 4)
            ethDNS = IPAddress(ipBuf[0], ipBuf[1], ipBuf[2], ipBuf[3]);
    }
    wifiNodeComm = prefs.getBool("wifiNodeComm", true);
    wifiWebUI    = prefs.getBool("wifiWebUI", true);
    ethNodeComm  = prefs.getBool("ethNodeComm", true);
    ethWebUI     = prefs.getBool("ethWebUI", true);
    primaryInterface = prefs.getUChar("primaryIf", 0);
    if (primaryInterface > 2) primaryInterface = 0;
#endif

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
                            udpPeerCall.push_back(UdpPeerCallsign());
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
                udpPeerCall.push_back(UdpPeerCallsign());
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

    // Clamp TX power to hardware and regulatory limits after loading
    if (settings.loraOutputPower > LORA_MAX_TX_POWER) {
        settings.loraOutputPower = LORA_MAX_TX_POWER;
    }
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
void saveEthSettings() {
    prefs.putBool("wifiEnabled",  wifiEnabled);
    prefs.putBool("ethEnabled",   ethEnabled);
    prefs.putBool("ethDhcp",      ethDhcp);
    uint8_t ipBuf[4];
    ipBuf[0] = ethIP[0]; ipBuf[1] = ethIP[1]; ipBuf[2] = ethIP[2]; ipBuf[3] = ethIP[3];
    prefs.putBytes("ethIP", ipBuf, 4);
    ipBuf[0] = ethNetMask[0]; ipBuf[1] = ethNetMask[1]; ipBuf[2] = ethNetMask[2]; ipBuf[3] = ethNetMask[3];
    prefs.putBytes("ethNetMask", ipBuf, 4);
    ipBuf[0] = ethGateway[0]; ipBuf[1] = ethGateway[1]; ipBuf[2] = ethGateway[2]; ipBuf[3] = ethGateway[3];
    prefs.putBytes("ethGateway", ipBuf, 4);
    ipBuf[0] = ethDNS[0]; ipBuf[1] = ethDNS[1]; ipBuf[2] = ethDNS[2]; ipBuf[3] = ethDNS[3];
    prefs.putBytes("ethDNS", ipBuf, 4);
    prefs.putBool("wifiNodeComm", wifiNodeComm);
    prefs.putBool("wifiWebUI",    wifiWebUI);
    prefs.putBool("ethNodeComm",  ethNodeComm);
    prefs.putBool("ethWebUI",     ethWebUI);
    prefs.putUChar("primaryIf",   primaryInterface);
}

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
    prefs.putUShort("oledPageIv", oledPageInterval);
    prefs.putUChar("oledPageMask", oledPageMask);
    prefs.putChar("oledBtnPin", oledButtonPin);
}

void saveSettings() {
    logPrintf(LOG_INFO, "Settings", "Saving settings...");

#ifdef HAS_WIFI
    // Sync wifiNetworks -> settings.wifiSSID (wifiNetworks is authoritative)
    if (!wifiNetworks.empty()) {
        int favIdx = 0;
        for (size_t i = 0; i < wifiNetworks.size(); i++) {
            if (wifiNetworks[i].favorite) { favIdx = (int)i; break; }
        }
        strlcpy(settings.wifiSSID,     wifiNetworks[favIdx].ssid,     sizeof(settings.wifiSSID));
        strlcpy(settings.wifiPassword, wifiNetworks[favIdx].password, sizeof(settings.wifiPassword));
    } else if (settings.wifiSSID[0] != '\0') {
        // Migration: legacy single-network -> wifiNetworks list
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
    prefs.putBool("heapDebug", heapDebugEnabled);
    saveOledSettings();
#ifdef HAS_WIFI
    saveWifiNetworks();
    saveUdpPeers();
    saveEthSettings();
#endif
    pendingLoraReinit = true;
}
