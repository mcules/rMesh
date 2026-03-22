#include <EEPROM.h>
#include <nvs_flash.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Preferences.h>

#include "settings.h"
#include "config.h"
#include "webFunctions.h"
#include "hal.h"

Settings settings;
ExtSettings extSettings;
uint8_t updateChannel = 0;
std::vector<IPAddress> udpPeers;
std::vector<bool> udpPeerLegacy;
std::vector<bool> udpPeerEnabled;
std::vector<String> udpPeerCall;

Preferences prefs;
bool loraReady = false;

void showSettings() {
    //Einstellungen als Debug-Ausgabe
    Serial.println();
    Serial.println("Einstellungen:");
    Serial.printf("WiFi SSID: %s\n", settings.wifiSSID);
    Serial.printf("WiFi Password: %s\n", settings.wifiPassword);
    Serial.printf("AP-Mode: %s\n", settings.apMode ? "true" : "false");
    Serial.printf("DHCP: %s\n", settings.dhcpActive ? "true" : "false");
    if (!settings.dhcpActive) {
        Serial.printf("IP: %d.%d.%d.%d\n", settings.wifiIP[0], settings.wifiIP[1], settings.wifiIP[2], settings.wifiIP[3]);
        Serial.printf("NetMask: %d.%d.%d.%d\n", settings.wifiNetMask[0], settings.wifiNetMask[1], settings.wifiNetMask[2], settings.wifiNetMask[3]);
        Serial.printf("DNS: %d.%d.%d.%d\n", settings.wifiDNS[0], settings.wifiDNS[1], settings.wifiDNS[2], settings.wifiDNS[3]);
        Serial.printf("Gateway: %d.%d.%d.%d\n", settings.wifiGateway[0], settings.wifiGateway[1], settings.wifiGateway[2], settings.wifiGateway[3]);
        //Serial.printf("Brodcast: %d.%d.%d.%d\n", settings.wifiBrodcast[0], settings.wifiBrodcast[1], settings.wifiBrodcast[2], settings.wifiBrodcast[3]);
    }
    Serial.printf("NTP-Server: %s\n", settings.ntpServer);
    if (udpPeers.empty()) {
        Serial.println("UDP Peers: keine");
    } else {
        for (size_t i = 0; i < udpPeers.size(); i++) {
            Serial.printf("UDP Peer %zu: %d.%d.%d.%d%s%s\n", i + 1,
                udpPeers[i][0], udpPeers[i][1], udpPeers[i][2], udpPeers[i][3],
                udpPeerLegacy[i] ? " [legacy]" : "",
                (bool)udpPeerEnabled[i] ? "" : " [deaktiviert]");
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
    Serial.printf("maxHopMessage: %d\n", extSettings.maxHopMessage);
    Serial.printf("maxHopPosition: %d\n", extSettings.maxHopPosition);
    Serial.printf("maxHopTelemetry: %d\n", extSettings.maxHopTelemetry);
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
        Serial.printf("NetMask: %d.%d.%d.%d\n", WiFi.subnetMask()[0], WiFi.subnetMask()[1], WiFi.subnetMask()[2], WiFi.subnetMask()[3]); 
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
    //Einstellungen über Websocket senden
    JsonDocument doc;
    doc["settings"]["mycall"] = settings.mycall;
    doc["settings"]["position"] = settings.position;
    doc["settings"]["ntp"] = settings.ntpServer;
    doc["settings"]["dhcpActive"] = settings.dhcpActive;
    doc["settings"]["wifiSSID"] = settings.wifiSSID;
    doc["settings"]["wifiPassword"] = settings.wifiPassword;
    doc["settings"]["apMode"] = settings.apMode;
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
    doc["settings"]["updateChannel"] = updateChannel;
    char* jsonBuffer = (char*)malloc(4096);
    size_t len = serializeJson(doc, jsonBuffer, 4096);
    wsBroadcast(jsonBuffer, len);
    free(jsonBuffer);
    jsonBuffer = nullptr;

}


void loadSettings() {
    //Einstellungen aus EEPROM lesen
    Serial.println("Lade Einstellungen...");
    prefs.begin("custom_settings", false);
    loadPasswordHash();
    prefs.getBytes("config", &settings, sizeof(settings));
    updateChannel = prefs.getUChar("updateChannel", 0);
    prefs.getBytes("extSettings", &extSettings, sizeof(extSettings));
    size_t storedLen = prefs.getBytesLength("config");
    size_t extSettingsLen = prefs.getBytesLength("extSettings");

    //IP-Adressen fixen
    settings.wifiIP       = IPAddress(settings.wifiIP[0], settings.wifiIP[1], settings.wifiIP[2], settings.wifiIP[3]);
    settings.wifiNetMask  = IPAddress(settings.wifiNetMask[0], settings.wifiNetMask[1], settings.wifiNetMask[2], settings.wifiNetMask[3]);
    settings.wifiGateway  = IPAddress(settings.wifiGateway[0], settings.wifiGateway[1], settings.wifiGateway[2], settings.wifiGateway[3]);
    settings.wifiDNS      = IPAddress(settings.wifiDNS[0], settings.wifiDNS[1], settings.wifiDNS[2], settings.wifiDNS[3]);
    settings.wifiBrodcast = IPAddress(settings.wifiBrodcast[0], settings.wifiBrodcast[1], settings.wifiBrodcast[2], settings.wifiBrodcast[3]);

    //Defaults für ext. Settings / Migration alter UDP-Peer-Daten
    if (extSettingsLen != sizeof(extSettings)) {
        // Altes Format: 3 maxHop-Bytes + 5×16 IP-Strings + 5 legacy-Flags = 88 Bytes
        const size_t OLD_EXT_SIZE = 3 + 5 * 16 + 5;
        size_t existingPeers = prefs.getBytesLength("udpPeers");
        if (extSettingsLen == OLD_EXT_SIZE && existingPeers == 0) {
            Serial.println("Migriere UDP-Peers aus altem ExtSettings-Format...");
            uint8_t* oldBuf = new uint8_t[OLD_EXT_SIZE];
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
                        Serial.printf("  Peer migriert: %s%s\n", ip, legacy ? " [legacy]" : "");
                    }
                }
            }
            delete[] oldBuf;
            if (!udpPeers.empty()) {
                saveUdpPeers();
                Serial.printf("%u UDP-Peer(s) erfolgreich migriert.\n", (unsigned)udpPeers.size());
            }
        } else {
            Serial.println("Lade Default-extSettings");
            extSettings.maxHopMessage = 15;
            extSettings.maxHopPosition = 1;
            extSettings.maxHopTelemetry = 3;
        }
        prefs.putBytes("extSettings", &extSettings, sizeof(extSettings));
    }

    // Dynamische UDP-Peers laden
    udpPeers.clear();
    udpPeerLegacy.clear();
    udpPeerEnabled.clear();
    udpPeerCall.clear();
    size_t peersLen = prefs.getBytesLength("udpPeers");
    if (peersLen >= 1) {
        uint8_t* buf = new uint8_t[peersLen];
        prefs.getBytes("udpPeers", buf, peersLen);
        uint8_t peerCount = buf[0];
        // Altes Format: 5 Bytes/Peer (ohne enabled), neues Format: 6 Bytes/Peer
        bool newFormat = (peersLen == 1 + (size_t)peerCount * 6);
        size_t stride = newFormat ? 6 : 5;
        for (uint8_t i = 0; i < peerCount && 1 + (size_t)i * stride + 4 < peersLen; i++) {
            udpPeers.push_back(IPAddress(buf[1+i*stride], buf[2+i*stride], buf[3+i*stride], buf[4+i*stride]));
            udpPeerLegacy.push_back(buf[5+i*stride] != 0);
            udpPeerEnabled.push_back(newFormat ? buf[6+i*stride] != 0 : true);
            udpPeerCall.push_back("");
        }
        delete[] buf;
        Serial.printf("%u UDP-Peer(s) geladen.\n", (unsigned)udpPeers.size());
    }

    //Defaults laden
    if (storedLen != sizeof(settings)) {
        Serial.println("Lade Default-Settings");
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
        // Keine Default-Frequenz – HF bleibt deaktiviert bis der User
        // explizit ein Band auswählt (433 MHz AFU oder 868 MHz Public).
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

    // Band-spezifische Korrekturen nach dem Laden
    if (loraConfigured(settings.loraFrequency)) {
        // TX-Power auf regulatorisches Maximum begrenzen (Public-Band: 27 dBm)
        if (isPublicBand(settings.loraFrequency) && settings.loraOutputPower > PUBLIC_MAX_TX_POWER) {
            settings.loraOutputPower = PUBLIC_MAX_TX_POWER;
        }
    }

    //MAX Nachrichtenlänge berechnen
    settings.loraMaxMessageLength = 255 - (4 * (MAX_CALLSIGN_LENGTH + 1)) - 8;

    //Hardware neu initialisieren
    initHal();
}

void saveUdpPeers() {
    uint8_t count = (uint8_t)udpPeers.size();
    size_t bufLen = 1 + (size_t)count * 6;
    uint8_t* buf = new uint8_t[bufLen];
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

void saveSettings() {
    //Einstellungen im EEPROM speichern
    Serial.println("Speichere Einstellungen...");
    prefs.putBytes("config", &settings, sizeof(settings));
    prefs.putBytes("extSettings", &extSettings, sizeof(extSettings));
    prefs.putUChar("updateChannel", updateChannel);
    saveUdpPeers();  // speichert Peers + ruft sendSettings()
    initHal();
}

