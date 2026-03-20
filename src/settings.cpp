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
    uint8_t count = sizeof(extSettings.udpPeer) / sizeof(extSettings.udpPeer[0]);
    for (uint8_t i = 0; i < count; i++) {
        Serial.printf("UDP Peer %i: %d.%d.%d.%d\n", i, extSettings.udpPeer[i][0], extSettings.udpPeer[i][1], extSettings.udpPeer[i][2], extSettings.udpPeer[i][3]);
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
    uint8_t count = sizeof(extSettings.udpPeer) / sizeof(extSettings.udpPeer[0]);
    for (uint8_t i = 0; i < count; i++) {
        JsonObject peer = doc["settings"]["udpPeers"].add<JsonObject>();
        peer["ip"][0] = extSettings.udpPeer[i][0];
        peer["ip"][1] = extSettings.udpPeer[i][1];
        peer["ip"][2] = extSettings.udpPeer[i][2];
        peer["ip"][3] = extSettings.udpPeer[i][3];
    }
    doc["settings"]["maxHopMessage"] = extSettings.maxHopMessage;
    doc["settings"]["maxHopPosition"] = extSettings.maxHopPosition;
    doc["settings"]["maxHopTelemetry"] = extSettings.maxHopTelemetry;
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
    prefs.getBytes("extSettings", &extSettings, sizeof(extSettings));
    size_t storedLen = prefs.getBytesLength("config");
    size_t extSettingsLen = prefs.getBytesLength("extSettings");

    //IP-Adressen fixen 
    settings.wifiIP       = IPAddress(settings.wifiIP[0], settings.wifiIP[1], settings.wifiIP[2], settings.wifiIP[3]);
    settings.wifiNetMask  = IPAddress(settings.wifiNetMask[0], settings.wifiNetMask[1], settings.wifiNetMask[2], settings.wifiNetMask[3]);
    settings.wifiGateway  = IPAddress(settings.wifiGateway[0], settings.wifiGateway[1], settings.wifiGateway[2], settings.wifiGateway[3]);
    settings.wifiDNS      = IPAddress(settings.wifiDNS[0], settings.wifiDNS[1], settings.wifiDNS[2], settings.wifiDNS[3]);
    settings.wifiBrodcast = IPAddress(settings.wifiBrodcast[0], settings.wifiBrodcast[1], settings.wifiBrodcast[2], settings.wifiBrodcast[3]);
    uint8_t count = sizeof(extSettings.udpPeer) / sizeof(extSettings.udpPeer[0]);
    for (uint8_t i = 0; i < count; i++) {
        extSettings.udpPeer[i] = IPAddress(extSettings.udpPeer[i][0], extSettings.udpPeer[i][1], extSettings.udpPeer[i][2], extSettings.udpPeer[i][3]);
    }

    //Defaults für ext. Settings
    if (extSettingsLen != sizeof(extSettings)) {
        Serial.println("Lade Default-extSettings");
        for (uint8_t i = 0; i < count; i++) {
            extSettings.udpPeer[i] = IPAddress(extSettings.udpPeer[i][0], extSettings.udpPeer[i][1], extSettings.udpPeer[i][2], extSettings.udpPeer[i][3]);
        }
        extSettings.maxHopMessage = 15;
        extSettings.maxHopPosition = 1;
        extSettings.maxHopTelemetry = 3;
        prefs.putBytes("extSettings", &extSettings, sizeof(extSettings));
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
        // SyncWord immer aus Frequenz ableiten (verhindert Netz-Crossover)
        settings.loraSyncWord = syncWordForFrequency(settings.loraFrequency);
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

void saveSettings() {
    //Einstellungen im EEPROM speichern
    Serial.println("Speichere Einstellungen...");
    prefs.putBytes("config", &settings, sizeof(settings));
    prefs.putBytes("extSettings", &extSettings, sizeof(extSettings));
    sendSettings();
    initHal();
}

