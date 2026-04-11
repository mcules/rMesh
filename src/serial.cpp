#include <Arduino.h>
#include <cstring>
#include <ArduinoJson.h>

#ifdef NRF52_PLATFORM
#include "platform_nrf52.h"
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#else
#include <LittleFS.h>
#include <nvs_flash.h>
#endif

#ifdef HAS_WIFI
#include <WiFi.h>
#include "wifiFunctions.h"
#include "auth.h"
#include "mbedtls/md.h"
#endif

#include "serial.h"
#ifdef HAS_ETHERNET
#include "ethFunctions.h"
#endif
#include "config.h"
#include "logging.h"
#include "settings.h"
#include "hal.h"
#include "main.h"
#include "frame.h"
#include "helperFunctions.h"
#include "peer.h"
#include "routing.h"
#include "ack.h"

bool serialDebug = false;
char serialRxBuffer[200] = {0};


void checkSerialRX() {
    if (Serial.available() > 0) {
        char rx = Serial.read();
        //Echo
        Serial.write(rx);
        if ((rx == 13) || (rx == 10)) {
            //Process
            if (strlen(serialRxBuffer) > 0 ) {

                //Copy parameters
                char parameter[200] = {0};
                char* pos = strchr(serialRxBuffer, ' ');
                if (pos != nullptr) {
                    pos++; // past the space
                    strncpy(parameter, pos, sizeof(parameter) - 1); // safe copy
                    logPrintf(LOG_INFO, "CLI", "%s", parameter);
                }

                //Convert buffer to lowercase
                for (int i = 0; serialRxBuffer[i] != '\0'; i++) {
                    serialRxBuffer[i] = tolower(serialRxBuffer[i]);
                }
                
                //Process commands

                //Test functions
                // BUG-S20: Hardcoded time-set removed
                // if (strncmp(serialRxBuffer, "t", 1) == 0) {
                //     struct tm tm;
                //     tm.tm_year = 2024 - 1900;
                //     tm.tm_mon = 1;
                //     tm.tm_mday = 14;
                //     tm.tm_hour = 4;
                //     tm.tm_min = 59;
                //     tm.tm_sec = 0;
                //     time_t t = mktime(&tm);
                //     struct timeval now = { .tv_sec = t };
                //     settimeofday(&now, NULL);
                //     Serial.println("Uhrzeit manuell auf 03:59:00 gesetzt!");
                // }

                //Help
                if (strncmp(serialRxBuffer, "h", 1) == 0) {
                    File file = LittleFS.open("/help.txt", "r");
                    if (file) {
                        while (file.available()) {
                            String line = file.readStringUntil('\n');
                            line.replace("\r", "");
                            logRaw("%s", line.c_str());
                        }
                        file.close();
                    } else {
                        logPrintf(LOG_ERROR, "CLI", "Error: /help.txt not found. Reflash filesystem?");
                    }
                }

                //Version
                if (strncmp(serialRxBuffer, "v", 1) == 0) {
                    //+ BOARD TYPE
                    logPrintf(LOG_INFO, "System", "");
                    logPrintf(LOG_INFO, "System", "%s", PIO_ENV_NAME);
                    logPrintf(LOG_INFO, "System", "%s %s", NAME, VERSION);
                    logPrintf(LOG_INFO, "System", "READY.");
                }

                //Settings
                if (strncmp(serialRxBuffer, "se", 2) == 0) {
                    showSettings();
                }

                //Reboot
                if (strncmp(serialRxBuffer, "reb", 3) == 0) {
                    logPrintf(LOG_INFO, "CLI", "Reboot...");
                    rebootTimer = millis(); rebootRequested = true;
                }

                // Hidden: send a remote COMMAND_MESSAGE frame to another node.
                // Syntax: "cmd <name> <NODECALL>"   (NODECALL is case-sensitive)
                //   names: reboot, version
                // Not listed in help.txt on purpose.
                if (strncmp(serialRxBuffer, "cmd ", 4) == 0) {
                    // parameter still holds the original-case remainder
                    char cmdName[16] = {0};
                    char targetCall[MAX_CALLSIGN_LENGTH + 1] = {0};
                    const char* sp = strchr(parameter, ' ');
                    if (sp != nullptr) {
                        size_t nameLen = (size_t)(sp - parameter);
                        if (nameLen > 0 && nameLen < sizeof(cmdName)) {
                            memcpy(cmdName, parameter, nameLen);
                            cmdName[nameLen] = '\0';
                            strncpy(targetCall, sp + 1, MAX_CALLSIGN_LENGTH);
                            targetCall[MAX_CALLSIGN_LENGTH] = '\0';
                            // trim trailing CR/LF/space just in case
                            for (int i = (int)strlen(targetCall) - 1; i >= 0; i--) {
                                if (targetCall[i] == '\r' || targetCall[i] == '\n' || targetCall[i] == ' ')
                                    targetCall[i] = '\0';
                                else break;
                            }
                        }
                    }

                    uint8_t cmdByte = 0;
                    if      (strcmp(cmdName, "version") == 0) cmdByte = 0xFF;
                    else if (strcmp(cmdName, "reboot")  == 0) cmdByte = 0xFE;

                    if (cmdByte != 0 && targetCall[0] != '\0') {
                        Frame f;
                        f.frameType = Frame::FrameTypes::MESSAGE_FRAME;
                        f.messageType = Frame::MessageTypes::COMMAND_MESSAGE;
                        strncpy(f.srcCall, settings.mycall, sizeof(f.srcCall));
                        strncpy(f.dstCall, targetCall, sizeof(f.dstCall));
                        f.message[0] = cmdByte;
                        f.messageLength = 1;
                        sendFrame(f);
                        logPrintf(LOG_INFO, "CLI", "cmd %s -> %s sent", cmdName, targetCall);
                    } else {
                        logPrintf(LOG_WARN, "CLI", "cmd: usage: cmd <name> <NODECALL>");
                    }
                }

                #ifdef HAS_WIFI
                //OTA Update
                if (strncmp(serialRxBuffer, "upd", 3) == 0 && strncmp(serialRxBuffer, "updf", 4) != 0) {
                    logPrintf(LOG_INFO, "CLI", "OTA update started...");
                    checkForUpdates();
                }

                // Set update channel: "uc 0" = Release, "uc 1" = Dev
                if (strncmp(serialRxBuffer, "uc", 2) == 0 && (serialRxBuffer[2] == ' ' || serialRxBuffer[2] == '\0')) {
                    if (strlen(parameter) > 0) {
                        updateChannel = (uint8_t)atoi(parameter);
                        saveSettings();
                    }
                    logPrintf(LOG_INFO, "Config", "updateChannel: %d (%s)", updateChannel, updateChannel == 1 ? "dev" : "release");
                }

                // Force-Install: "updf 0" = Release, "updf 1" = Dev
                if (strncmp(serialRxBuffer, "updf", 4) == 0) {
                    pendingForceChannel = (strlen(parameter) > 0) ? (uint8_t)atoi(parameter) : updateChannel;
                    pendingForceUpdate = true;
                    logPrintf(LOG_INFO, "CLI", "Force install started (channel: %s)...", pendingForceChannel == 1 ? "dev" : "release");
                }

                // ── wifi command group ────────────────────────────────────────
                if (strncmp(serialRxBuffer, "wifi", 4) == 0 && (serialRxBuffer[4] == ' ' || serialRxBuffer[4] == '\0')) {
                    const char* sub = serialRxBuffer + 5; // lowercased sub-command
                    if (serialRxBuffer[4] == '\0' || parameter[0] == '\0') {
                        logPrintf(LOG_INFO, "WiFi", "wifiEnabled: %s", wifiEnabled ? "true" : "false");
                    } else if (sub[0] == '0' || sub[0] == '1') {
                        wifiEnabled = (sub[0] == '1');
                        saveEthSettings();
                        logPrintf(LOG_INFO, "Config", "wifiEnabled: %s", wifiEnabled ? "true" : "false");
                    } else if (strncmp(sub, "tx ", 3) == 0) {
                        int8_t v = (int8_t)atoi(parameter + 3);
                        if (v < 2) v = 2;
                        if (v > 20) v = 20;
                        wifiTxPower = v;
                        WiFi.setTxPower((wifi_power_t)(wifiTxPower * 4));
                        saveSettings();
                        logPrintf(LOG_INFO, "Config", "wifiTxPower: %d dBm", wifiTxPower);
                    } else if (strncmp(sub, "nc", 2) == 0 && (sub[2] == ' ' || sub[2] == '\0')) {
                        if (sub[2] == ' ') {
                            wifiNodeComm = (sub[3] == '1');
                            saveEthSettings();
                        }
                        logPrintf(LOG_INFO, "Config", "wifiNodeComm: %s", wifiNodeComm ? "true" : "false");
                    } else if (strncmp(sub, "wu", 2) == 0 && (sub[2] == ' ' || sub[2] == '\0')) {
                        if (sub[2] == ' ') {
                            wifiWebUI = (sub[3] == '1');
                            saveEthSettings();
                        }
                        logPrintf(LOG_INFO, "Config", "wifiWebUI: %s", wifiWebUI ? "true" : "false");
                    } else if (strncmp(sub, "scan", 4) == 0) {
                        logPrintf(LOG_INFO, "WiFi", "WiFi scan.....");
                        pendingReconnectScan = false;
                        WiFi.scanNetworks(true);
                    } else if (strncmp(sub, "list", 4) == 0) {
                        if (wifiNetworks.empty()) {
                            logPrintf(LOG_INFO, "WiFi", "WiFi networks: none");
                        } else {
                            for (size_t i = 0; i < wifiNetworks.size(); i++) {
                                logPrintf(LOG_INFO, "WiFi", "WiFi %zu: %s%s (pw: %s)", i + 1,
                                    wifiNetworks[i].ssid,
                                    wifiNetworks[i].favorite ? " [favorite]" : "",
                                    (wifiNetworks[i].password[0] != '\0') ? "set" : "none");
                            }
                        }
                    } else if (strncmp(sub, "clear", 5) == 0) {
                        wifiNetworks.clear();
                        settings.wifiSSID[0] = '\0';
                        settings.wifiPassword[0] = '\0';
                        saveWifiNetworks();
                        logPrintf(LOG_INFO, "WiFi", "All WiFi networks deleted.");
                    } else if (strncmp(sub, "add ", 4) == 0) {
                        const char* rest = parameter + 4;
                        WifiNetwork net;
                        memset(&net, 0, sizeof(net));
                        net.favorite = wifiNetworks.empty();
                        const char* sp = strchr(rest, ' ');
                        if (sp) {
                            size_t ssidLen = sp - rest;
                            if (ssidLen >= sizeof(net.ssid)) ssidLen = sizeof(net.ssid) - 1;
                            strncpy(net.ssid, rest, ssidLen);
                            net.ssid[ssidLen] = '\0';
                            strlcpy(net.password, sp + 1, sizeof(net.password));
                        } else {
                            strlcpy(net.ssid, rest, sizeof(net.ssid));
                        }
                        wifiNetworks.push_back(net);
                        saveSettings();
                        wifiInit();
                        logPrintf(LOG_INFO, "WiFi", "WiFi %zu added: %s%s", wifiNetworks.size(),
                            net.ssid, net.favorite ? " [favorite]" : "");
                    } else if (strncmp(sub, "del ", 4) == 0) {
                        int idx = atoi(parameter + 4) - 1;
                        if (idx >= 0 && (size_t)idx < wifiNetworks.size()) {
                            logPrintf(LOG_INFO, "WiFi", "WiFi %d deleted: %s", idx + 1, wifiNetworks[idx].ssid);
                            wifiNetworks.erase(wifiNetworks.begin() + idx);
                            saveSettings();
                            wifiInit();
                        } else {
                            logPrintf(LOG_ERROR, "WiFi", "Error: index 1-%zu expected!", wifiNetworks.size());
                        }
                    } else if (strncmp(sub, "fav ", 4) == 0) {
                        int idx = atoi(parameter + 4) - 1;
                        if (idx >= 0 && (size_t)idx < wifiNetworks.size()) {
                            for (auto& n : wifiNetworks) n.favorite = false;
                            wifiNetworks[idx].favorite = true;
                            saveSettings();
                            wifiInit();
                            logPrintf(LOG_INFO, "WiFi", "WiFi %d set as favorite: %s", idx + 1, wifiNetworks[idx].ssid);
                        } else {
                            logPrintf(LOG_ERROR, "WiFi", "Error: index 1-%zu expected!", wifiNetworks.size());
                        }
                    } else if (strncmp(sub, "pw ", 3) == 0) {
                        const char* rest = parameter + 3;
                        char* sp = strchr((char*)rest, ' ');
                        if (sp) {
                            int idx = atoi(rest) - 1;
                            if (idx >= 0 && (size_t)idx < wifiNetworks.size()) {
                                strlcpy(wifiNetworks[idx].password, sp + 1, sizeof(wifiNetworks[idx].password));
                                saveSettings();
                                logPrintf(LOG_INFO, "WiFi", "WiFi %d password updated.", idx + 1);
                            } else {
                                logPrintf(LOG_ERROR, "WiFi", "Error: index 1-%zu expected!", wifiNetworks.size());
                            }
                        } else {
                            logPrintf(LOG_INFO, "WiFi", "Usage: wifi pw <N> <PASSWORD>");
                        }
                    } else if (strncmp(sub, "ap", 2) == 0 && (sub[2] == ' ' || sub[2] == '\0')) {
                        if (sub[2] == '\0') {
                            logPrintf(LOG_INFO, "WiFi", "AP Mode: %s  Name: %s  Password: %s",
                                      settings.apMode ? "on" : "off", apName.c_str(),
                                      apPassword.isEmpty() ? "not set" : "set");
                        } else {
                            const char* apsub = sub + 3;
                            if (apsub[0] == '0' || apsub[0] == '1') {
                                settings.apMode = (apsub[0] == '1');
                                saveSettings(); wifiInit();
                                logPrintf(LOG_INFO, "Config", "apMode: %s", settings.apMode ? "true" : "false");
                            } else if (strncmp(apsub, "name ", 5) == 0) {
                                apName = String(parameter + 8);
                                saveSettings();
                                if (settings.apMode) wifiInit();
                                logPrintf(LOG_INFO, "Config", "AP Name: %s", apName.c_str());
                            } else if (strncmp(apsub, "pw ", 3) == 0) {
                                const char* pw = parameter + 6;
                                if (strcmp(pw, "-") == 0) {
                                    apPassword = "";
                                    saveSettings();
                                    if (settings.apMode) wifiInit();
                                    logPrintf(LOG_INFO, "Config", "AP password cleared.");
                                } else if (strlen(pw) < 8) {
                                    logPrintf(LOG_ERROR, "WiFi", "Error: AP password must be at least 8 characters!");
                                } else {
                                    apPassword = String(pw);
                                    saveSettings();
                                    if (settings.apMode) wifiInit();
                                    logPrintf(LOG_INFO, "Config", "AP password set.");
                                }
                            }
                        }
                    } else if (strncmp(sub, "ip ", 3) == 0) {
                        IPAddress tempIP;
                        if (tempIP.fromString(parameter + 3)) {
                            settings.wifiIP = tempIP; saveSettings(); wifiInit();
                        } else { logPrintf(LOG_ERROR, "Config", "Error: Invalid IP format!"); }
                        logPrintf(LOG_INFO, "Config", "WiFi IP: %d.%d.%d.%d", settings.wifiIP[0], settings.wifiIP[1], settings.wifiIP[2], settings.wifiIP[3]);
                    } else if (strncmp(sub, "dns ", 4) == 0) {
                        IPAddress tempIP;
                        if (tempIP.fromString(parameter + 4)) {
                            settings.wifiDNS = tempIP; saveSettings(); wifiInit();
                        } else { logPrintf(LOG_ERROR, "Config", "Error: Invalid IP format!"); }
                        logPrintf(LOG_INFO, "Config", "WiFi DNS: %d.%d.%d.%d", settings.wifiDNS[0], settings.wifiDNS[1], settings.wifiDNS[2], settings.wifiDNS[3]);
                    } else if (strncmp(sub, "gateway ", 8) == 0) {
                        IPAddress tempIP;
                        if (tempIP.fromString(parameter + 8)) {
                            settings.wifiGateway = tempIP; saveSettings(); wifiInit();
                        } else { logPrintf(LOG_ERROR, "Config", "Error: Invalid IP format!"); }
                        logPrintf(LOG_INFO, "Config", "WiFi Gateway: %d.%d.%d.%d", settings.wifiGateway[0], settings.wifiGateway[1], settings.wifiGateway[2], settings.wifiGateway[3]);
                    } else if (strncmp(sub, "netmask ", 8) == 0) {
                        IPAddress tempIP;
                        if (tempIP.fromString(parameter + 8)) {
                            settings.wifiNetMask = tempIP; saveSettings(); wifiInit();
                        } else { logPrintf(LOG_ERROR, "Config", "Error: Invalid IP format!"); }
                        logPrintf(LOG_INFO, "Config", "WiFi Netmask: %d.%d.%d.%d", settings.wifiNetMask[0], settings.wifiNetMask[1], settings.wifiNetMask[2], settings.wifiNetMask[3]);
                    } else if (strncmp(sub, "dhcp", 4) == 0 && (sub[4] == ' ' || sub[4] == '\0')) {
                        if (sub[4] == ' ') {
                            settings.dhcpActive = (sub[5] == '1');
                            saveSettings(); wifiInit();
                        }
                        logPrintf(LOG_INFO, "Config", "WiFi DHCP: %s", settings.dhcpActive ? "true" : "false");
                    }
                }
                #endif // HAS_WIFI

                //Defaults
                if (strncmp(serialRxBuffer, "de", 2) == 0) {
                    std::memset(settings.mycall, 0xff, sizeof(settings.mycall));
                    #ifdef NRF52_PLATFORM
                    nvs_flash_erase_nrf52();
                    nvs_flash_init_nrf52();
                    #else
                    nvs_flash_erase();
                    nvs_flash_init();
                    #endif
                    rebootTimer = millis(); rebootRequested = true;
                }

                // ── lora command group ────────────────────────────────────────
                if (strncmp(serialRxBuffer, "lora", 4) == 0 && (serialRxBuffer[4] == ' ' || serialRxBuffer[4] == '\0')) {
                    const char* sub = serialRxBuffer + 5;
                    if (serialRxBuffer[4] == '\0' || parameter[0] == '\0') {
                        logPrintf(LOG_INFO, "LoRa", "loraEnabled: %s  loraReady: %s", loraEnabled ? "true" : "false", loraReady ? "true" : "false");
                    } else if (sub[0] == '0' || sub[0] == '1') {
                        loraEnabled = (sub[0] == '1');
                        saveSettings();
                        logPrintf(LOG_INFO, "Config", "loraEnabled: %s", loraEnabled ? "true" : "false");
                    } else if (strncmp(sub, "minsnr", 6) == 0 && (sub[6] == ' ' || sub[6] == '\0')) {
                        if (sub[6] == ' ') { extSettings.minSnr = (int8_t)atoi(parameter + 7); saveSettings(); }
                        logPrintf(LOG_INFO, "Config", "minSnr: %d dB", extSettings.minSnr);
                    } else if (strncmp(sub, "freq ", 5) == 0) {
                        if (strncmp(sub + 5, "433", 3) == 0) {
                            settings.loraFrequency = 434.850f; settings.loraBandwidth = 62.5f;
                            settings.loraSpreadingFactor = 7; settings.loraCodingRate = 6;
                            settings.loraOutputPower = 20; settings.loraPreambleLength = 10;
                            settings.loraSyncWord = syncWordForFrequency(settings.loraFrequency);
                            saveSettings();
                            logPrintf(LOG_INFO, "Config", "Preset 433 MHz (amateur radio) applied.");
                        } else if (strncmp(sub + 5, "868", 3) == 0) {
                            settings.loraFrequency = 869.525f; settings.loraBandwidth = 125.0f;
                            settings.loraSpreadingFactor = 7; settings.loraCodingRate = 5;
                            settings.loraOutputPower = 27; settings.loraPreambleLength = 10;
                            settings.loraSyncWord = syncWordForFrequency(settings.loraFrequency);
                            saveSettings();
                            logPrintf(LOG_INFO, "Config", "Preset 868 MHz (public, 500 mW) applied.");
                        } else {
                            logPrintf(LOG_INFO, "Config", "Current frequency: %.3f MHz  Usage: lora freq 433 | lora freq 868", settings.loraFrequency);
                        }
                    } else if (strncmp(sub, "op ", 3) == 0) {
                        int8_t txp = (int8_t)atoi(parameter + 3);
                        if (txp > LORA_MAX_TX_POWER) {
                            logPrintf(LOG_WARN, "Config", "TX Power %d dBm exceeds board max (%d dBm).", txp, LORA_MAX_TX_POWER);
                        }
                        if (isPublicBand(settings.loraFrequency) && txp > PUBLIC_MAX_TX_POWER) txp = PUBLIC_MAX_TX_POWER;
                        settings.loraOutputPower = txp; saveSettings();
                        logPrintf(LOG_INFO, "Config", "TX Power: %d dBm", settings.loraOutputPower);
                    } else if (strncmp(sub, "bw ", 3) == 0) {
                        settings.loraBandwidth = atof(parameter + 3); saveSettings();
                        logPrintf(LOG_INFO, "Config", "Bandwidth: %.2f kHz", settings.loraBandwidth);
                    } else if (strncmp(sub, "sf ", 3) == 0) {
                        settings.loraSpreadingFactor = (uint8_t)atoi(parameter + 3); saveSettings();
                        logPrintf(LOG_INFO, "Config", "Spreading Factor: %d", settings.loraSpreadingFactor);
                    } else if (strncmp(sub, "cr ", 3) == 0) {
                        settings.loraCodingRate = (uint8_t)atoi(parameter + 3); saveSettings();
                        logPrintf(LOG_INFO, "Config", "Coding Rate: %d", settings.loraCodingRate);
                    } else if (strncmp(sub, "pl ", 3) == 0) {
                        settings.loraPreambleLength = (int16_t)atoi(parameter + 3); saveSettings();
                        logPrintf(LOG_INFO, "Config", "Preamble Length: %d", settings.loraPreambleLength);
                    }
                }

                // Callsign
                if (strncmp(serialRxBuffer, "call", 4) == 0) {
                    if (strlen(parameter) > 0) {
                        strncpy(settings.mycall, parameter, sizeof(settings.mycall) - 1);
                        settings.mycall[sizeof(settings.mycall) - 1] = '\0';
                        for (size_t i = 0; settings.mycall[i]; i++) settings.mycall[i] = toupper(settings.mycall[i]);
                        saveSettings();
                    }
                    logPrintf(LOG_INFO, "Config", "Callsign: %s", settings.mycall);
                }

                // Position
                if (strncmp(serialRxBuffer, "pos", 3) == 0) {
                    if (strlen(parameter) > 0) {
                        strncpy(settings.position, parameter, sizeof(settings.position) - 1);
                        settings.position[sizeof(settings.position) - 1] = '\0';
                        saveSettings();
                    }
                    logPrintf(LOG_INFO, "Config", "Position: %s", settings.position);
                }

                // ── net command group ────────────────────────────────────────
                if (strncmp(serialRxBuffer, "net ", 4) == 0) {
                    const char* sub = serialRxBuffer + 4;
                    if (strncmp(sub, "ntp", 3) == 0 && (sub[3] == ' ' || sub[3] == '\0')) {
                        if (sub[3] == ' ') {
                            strncpy(settings.ntpServer, parameter + 4, sizeof(settings.ntpServer) - 1);
                            settings.ntpServer[sizeof(settings.ntpServer) - 1] = '\0';
                            saveSettings();
                        }
                        logPrintf(LOG_INFO, "Config", "NTP: %s", settings.ntpServer);
                    }
#ifdef HAS_WIFI
                    else if (strncmp(sub, "pri", 3) == 0 && (sub[3] == ' ' || sub[3] == '\0')) {
                        if (sub[3] == ' ') {
                            uint8_t v = (uint8_t)atoi(parameter + 4);
                            if (v <= 2) { primaryInterface = v; saveEthSettings(); }
                        }
                        logPrintf(LOG_INFO, "Config", "primaryInterface: %d (%s)", primaryInterface,
                                  primaryInterface == 0 ? "auto" : (primaryInterface == 1 ? "WiFi" : "LAN"));
                    }
#endif
                }

                // Sync Word (hexadecimal, e.g. "sw 2B")
                if (strncmp(serialRxBuffer, "sw", 2) == 0) {
                    if (strlen(parameter) > 0) {
                        settings.loraSyncWord = (uint8_t)strtol(parameter, nullptr, 16);
                        saveSettings();
                    }
                    logPrintf(LOG_INFO, "Config", "SyncWord: %02X", settings.loraSyncWord);
                }

                // Repeat / Relay
                // "rep 1" or "rep 0"
                if (strncmp(serialRxBuffer, "rep", 3) == 0) {
                    if (strlen(parameter) > 0) {
                        bool value = (parameter[0] == '1' || parameter[0] == 'e' || parameter[0] == 't');
                        settings.loraRepeat = value;
                        saveSettings();
                    }
                    logPrintf(LOG_INFO, "Config", "Repeat: %s", settings.loraRepeat ? "true" : "false");
                }

                // Max Hop Message
                if (strncmp(serialRxBuffer, "mhm", 3) == 0) {
                    if (strlen(parameter) > 0) {
                        extSettings.maxHopMessage = (uint8_t)atoi(parameter);
                        saveSettings();
                    }
                    logPrintf(LOG_INFO, "Config", "MaxHopMessage: %d", extSettings.maxHopMessage);
                }

                // Max Hop Position
                if (strncmp(serialRxBuffer, "mhp", 3) == 0) {
                    if (strlen(parameter) > 0) {
                        extSettings.maxHopPosition = (uint8_t)atoi(parameter);
                        saveSettings();
                    }
                    logPrintf(LOG_INFO, "Config", "MaxHopPosition: %d", extSettings.maxHopPosition);
                }

                // Max Hop Telemetry
                if (strncmp(serialRxBuffer, "mht", 3) == 0) {
                    if (strlen(parameter) > 0) {
                        extSettings.maxHopTelemetry = (uint8_t)atoi(parameter);
                        saveSettings();
                    }
                    logPrintf(LOG_INFO, "Config", "MaxHopTelemetry: %d", extSettings.maxHopTelemetry);
                }

                #ifdef HAS_WIFI
                // WebUI password
                // "webpw <password>" → Set password (stored as SHA256)
                // "webpw -"          → Clear password
                if (strncmp(serialRxBuffer, "webp", 4) == 0) {
                    if (strlen(parameter) > 0) {
                        if (strcmp(parameter, "-") == 0) {
                            savePasswordHash("");
                            logPrintf(LOG_INFO, "Config", "WebUI password cleared.");
                        } else {
                            uint8_t hash[32];
                            mbedtls_md_context_t ctx;
                            mbedtls_md_init(&ctx);
                            mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
                            mbedtls_md_starts(&ctx);
                            mbedtls_md_update(&ctx, (const uint8_t*)parameter, strlen(parameter));
                            mbedtls_md_finish(&ctx, hash);
                            mbedtls_md_free(&ctx);
                            char hexHash[65];
                            for (int i = 0; i < 32; i++) sprintf(hexHash + 2 * i, "%02x", hash[i]);
                            hexHash[64] = '\0';
                            savePasswordHash(hexHash);
                            logPrintf(LOG_INFO, "Config", "WebUI password set.");
                        }
                    } else {
                        logPrintf(LOG_INFO, "Config", "WebUI password: %s", webPasswordHash.isEmpty() ? "not set" : "set");
                    }
                }

                // UDP peer management
                // "udp"                  → list all peers
                // "udp add <IP>"         → add peer
                // "udp add <IP> legacy"  → add legacy peer
                // "udp del <N>"          → delete peer N (1-based)
                // "udp <N> <IP>"         → set IP of peer N
                // "udp <N> legacy"       → toggle legacy flag of peer N
                // "udp <N> enabled"      → toggle enabled flag of peer N
                // "udp clear"            → delete all peers
                if (strncmp(serialRxBuffer, "udp", 3) == 0) {
                    if (parameter == nullptr || parameter[0] == '\0') {
                        // List all peers
                        if (udpPeers.empty()) {
                            logPrintf(LOG_INFO, "UDP", "UDP Peers: none");
                        } else {
                            for (size_t i = 0; i < udpPeers.size(); i++) {
                                logPrintf(LOG_INFO, "UDP", "UDP-Peer %zu: %d.%d.%d.%d%s%s", i + 1,
                                    udpPeers[i][0], udpPeers[i][1], udpPeers[i][2], udpPeers[i][3],
                                    udpPeerLegacy[i] ? " [legacy]" : "",
                                    (bool)udpPeerEnabled[i] ? "" : " [disabled]");
                            }
                        }
                    } else if (strncmp(parameter, "clear", 5) == 0) {
                        udpPeers.clear();
                        udpPeerLegacy.clear();
                        saveUdpPeers();
                        logPrintf(LOG_INFO, "UDP", "All UDP peers deleted.");
                    } else if (strncmp(parameter, "add ", 4) == 0) {
                        const char* rest = parameter + 4;
                        // Extract IP (until space or end)
                        char ipStr[20];
                        const char* sp = strchr(rest, ' ');
                        if (sp) {
                            size_t ipLen = sp - rest;
                            if (ipLen < sizeof(ipStr)) {
                                strncpy(ipStr, rest, ipLen);
                                ipStr[ipLen] = '\0';
                            } else { ipStr[0] = '\0'; }
                        } else {
                            strncpy(ipStr, rest, sizeof(ipStr) - 1);
                            ipStr[sizeof(ipStr)-1] = '\0';
                        }
                        IPAddress tempIP;
                        if (tempIP.fromString(ipStr)) {
                            bool legacy = (sp && strstr(sp, "legacy") != nullptr);
                            udpPeers.push_back(tempIP);
                            udpPeerLegacy.push_back(legacy);
                            udpPeerEnabled.push_back(true);
                            saveUdpPeers();
                            logPrintf(LOG_INFO, "UDP", "UDP peer %zu added: %d.%d.%d.%d%s",
                                udpPeers.size(), tempIP[0], tempIP[1], tempIP[2], tempIP[3],
                                legacy ? " [legacy]" : "");
                        } else {
                            logPrintf(LOG_ERROR, "UDP", "Error: Invalid IP format!");
                        }
                    } else if (strncmp(parameter, "del ", 4) == 0) {
                        int idx = atoi(parameter + 4) - 1;
                        if (idx >= 0 && (size_t)idx < udpPeers.size()) {
                            logPrintf(LOG_INFO, "UDP", "UDP peer %d deleted: %d.%d.%d.%d", idx + 1,
                                udpPeers[idx][0], udpPeers[idx][1], udpPeers[idx][2], udpPeers[idx][3]);
                            udpPeers.erase(udpPeers.begin() + idx);
                            udpPeerLegacy.erase(udpPeerLegacy.begin() + idx);
                            udpPeerEnabled.erase(udpPeerEnabled.begin() + idx);
                            saveUdpPeers();
                        } else {
                            logPrintf(LOG_ERROR, "UDP", "Error: Index 1-%zu expected!", udpPeers.size());
                        }
                    } else {
                        // "udp <N> <IP>", "udp <N> legacy" or "udp <N> enabled"
                        char* spacePos = strchr(parameter, ' ');
                        if (spacePos != nullptr) {
                            int idx = atoi(parameter) - 1;
                            spacePos++;
                            if (idx >= 0 && (size_t)idx < udpPeers.size()) {
                                if (strncmp(spacePos, "legacy", 6) == 0) {
                                    udpPeerLegacy[idx] = !udpPeerLegacy[idx];
                                    saveUdpPeers();
                                    logPrintf(LOG_INFO, "UDP", "UDP peer %d legacy: %s", idx + 1, (bool)udpPeerLegacy[idx] ? "on" : "off");
                                } else if (strncmp(spacePos, "enabled", 7) == 0) {
                                    udpPeerEnabled[idx] = !(bool)udpPeerEnabled[idx];
                                    saveUdpPeers();
                                    logPrintf(LOG_INFO, "UDP", "UDP peer %d enabled: %s", idx + 1, (bool)udpPeerEnabled[idx] ? "on" : "off");
                                } else {
                                    IPAddress tempIP;
                                    if (tempIP.fromString(spacePos)) {
                                        udpPeers[idx] = tempIP;
                                        saveUdpPeers();
                                        logPrintf(LOG_INFO, "UDP", "UDP-Peer %d: %d.%d.%d.%d", idx + 1, tempIP[0], tempIP[1], tempIP[2], tempIP[3]);
                                    } else {
                                        logPrintf(LOG_ERROR, "UDP", "Error: Invalid IP format!");
                                    }
                                }
                            } else {
                                logPrintf(LOG_ERROR, "UDP", "Error: Index 1-%zu expected!", udpPeers.size());
                            }
                        } else {
                            logPrintf(LOG_INFO, "UDP", "Commands: udp | udp add <IP> [legacy] | udp del <N> | udp <N> <IP> | udp <N> legacy | udp <N> enabled | udp clear");
                        }
                    }
                }
                // ── lan command group (Ethernet) ─────────────────────────────
#ifdef HAS_ETHERNET
                if (strncmp(serialRxBuffer, "lan", 3) == 0 && (serialRxBuffer[3] == ' ' || serialRxBuffer[3] == '\0')) {
                    const char* sub = serialRxBuffer + 4;
                    if (serialRxBuffer[3] == '\0' || parameter[0] == '\0') {
                        logPrintf(LOG_INFO, "ETH", "ethEnabled: %s  ethConnected: %s", ethEnabled ? "true" : "false", ethConnected ? "true" : "false");
                    } else if (sub[0] == '0' || sub[0] == '1') {
                        ethEnabled = (sub[0] == '1');
                        saveEthSettings();
                        logPrintf(LOG_INFO, "Config", "ethEnabled: %s", ethEnabled ? "true" : "false");
                    } else if (strncmp(sub, "nc", 2) == 0 && (sub[2] == ' ' || sub[2] == '\0')) {
                        if (sub[2] == ' ') { ethNodeComm = (sub[3] == '1'); saveEthSettings(); }
                        logPrintf(LOG_INFO, "Config", "ethNodeComm: %s", ethNodeComm ? "true" : "false");
                    } else if (strncmp(sub, "wu", 2) == 0 && (sub[2] == ' ' || sub[2] == '\0')) {
                        if (sub[2] == ' ') { ethWebUI = (sub[3] == '1'); saveEthSettings(); }
                        logPrintf(LOG_INFO, "Config", "ethWebUI: %s", ethWebUI ? "true" : "false");
                    } else if (strncmp(sub, "ip ", 3) == 0) {
                        IPAddress tempIP;
                        if (tempIP.fromString(parameter + 3)) { ethIP = tempIP; saveEthSettings(); }
                        else { logPrintf(LOG_ERROR, "Config", "Error: Invalid IP format!"); }
                        logPrintf(LOG_INFO, "Config", "LAN IP: %d.%d.%d.%d", ethIP[0], ethIP[1], ethIP[2], ethIP[3]);
                    } else if (strncmp(sub, "dns ", 4) == 0) {
                        IPAddress tempIP;
                        if (tempIP.fromString(parameter + 4)) { ethDNS = tempIP; saveEthSettings(); }
                        else { logPrintf(LOG_ERROR, "Config", "Error: Invalid IP format!"); }
                        logPrintf(LOG_INFO, "Config", "LAN DNS: %d.%d.%d.%d", ethDNS[0], ethDNS[1], ethDNS[2], ethDNS[3]);
                    } else if (strncmp(sub, "gateway ", 8) == 0) {
                        IPAddress tempIP;
                        if (tempIP.fromString(parameter + 8)) { ethGateway = tempIP; saveEthSettings(); }
                        else { logPrintf(LOG_ERROR, "Config", "Error: Invalid IP format!"); }
                        logPrintf(LOG_INFO, "Config", "LAN Gateway: %d.%d.%d.%d", ethGateway[0], ethGateway[1], ethGateway[2], ethGateway[3]);
                    } else if (strncmp(sub, "netmask ", 8) == 0) {
                        IPAddress tempIP;
                        if (tempIP.fromString(parameter + 8)) { ethNetMask = tempIP; saveEthSettings(); }
                        else { logPrintf(LOG_ERROR, "Config", "Error: Invalid IP format!"); }
                        logPrintf(LOG_INFO, "Config", "LAN Netmask: %d.%d.%d.%d", ethNetMask[0], ethNetMask[1], ethNetMask[2], ethNetMask[3]);
                    } else if (strncmp(sub, "dhcp", 4) == 0 && (sub[4] == ' ' || sub[4] == '\0')) {
                        if (sub[4] == ' ') { ethDhcp = (sub[5] == '1'); saveEthSettings(); }
                        logPrintf(LOG_INFO, "Config", "LAN DHCP: %s", ethDhcp ? "true" : "false");
                    }
                }
#endif
                #endif // HAS_WIFI (webpw + udp)

                // Debug mode toggle
                // "dbg 1" = enable, "dbg 0" = disable
                // NOTE: must NOT use "debug" — conflicts with "de" (factory defaults)
                if (strncmp(serialRxBuffer, "dbg", 3) == 0 && (serialRxBuffer[3] == ' ' || serialRxBuffer[3] == '\0')) {
                    if (strlen(parameter) > 0) {
                        serialDebug = (parameter[0] == '1' || parameter[0] == 'e' || parameter[0] == 't');
                        #ifndef NRF52_PLATFORM
                        Serial.setDebugOutput(serialDebug);
                        #endif
                        saveSettings();
                    }
                    logPrintf(LOG_INFO, "Debug", "serialDebug: %s", serialDebug ? "true" : "false");
                }

                // Send direct message: "msg <CALL> <TEXT>"
                if (strncmp(serialRxBuffer, "msg", 3) == 0 && serialRxBuffer[3] == ' ') {
                    char* sp = strchr(parameter, ' ');
                    if (sp != nullptr) {
                        *sp = '\0';
                        char dst[MAX_CALLSIGN_LENGTH + 1] = {0};
                        strncpy(dst, parameter, MAX_CALLSIGN_LENGTH);
                        for (size_t ci = 0; dst[ci]; ci++) dst[ci] = toupper(dst[ci]);
                        sendMessage(dst, sp + 1);
                        logPrintf(LOG_INFO, "Msg", "Message sent to %s", dst);
                    } else {
                        logPrintf(LOG_INFO, "Msg", "Usage: msg <CALL> <TEXT>");
                    }
                }

                // Send group message: "xgrp <GROUP> <TEXT>"
                // NOTE: must NOT use "grp" — conflicts with "g" (gateway)
                if (strncmp(serialRxBuffer, "xgrp", 4) == 0 && serialRxBuffer[4] == ' ') {
                    char* sp = strchr(parameter, ' ');
                    if (sp != nullptr) {
                        *sp = '\0';
                        char dst[MAX_CALLSIGN_LENGTH + 1] = {0};
                        strncpy(dst, parameter, MAX_CALLSIGN_LENGTH);
                        for (size_t ci = 0; dst[ci]; ci++) dst[ci] = toupper(dst[ci]);
                        sendGroup(dst, sp + 1);
                        logPrintf(LOG_INFO, "Msg", "Group message sent to %s", dst);
                    } else {
                        logPrintf(LOG_INFO, "Msg", "Usage: xgrp <GROUP> <TEXT>");
                    }
                }

                // Send trace: "xtrace <CALL>"
                // NOTE: must NOT use "trace" — conflicts with "t" (time set)
                if (strncmp(serialRxBuffer, "xtrace", 6) == 0 && serialRxBuffer[6] == ' ') {
                    if (strlen(parameter) > 0) {
                        char dst[MAX_CALLSIGN_LENGTH + 1] = {0};
                        strncpy(dst, parameter, MAX_CALLSIGN_LENGTH);
                        for (size_t ci = 0; dst[ci]; ci++) dst[ci] = toupper(dst[ci]);
                        char timeStr[16];
                        getFormattedTime("%H:%M:%S", timeStr, sizeof(timeStr));
                        sendMessage(dst, timeStr, Frame::MessageTypes::TRACE_MESSAGE);
                        logPrintf(LOG_INFO, "Msg", "Trace sent to %s", dst);
                    } else {
                        logPrintf(LOG_INFO, "Msg", "Usage: xtrace <CALL>");
                    }
                }

                // Trigger manual announce
                if (strncmp(serialRxBuffer, "announce", 8) == 0) {
                    announceTimer = 0;
                    logPrintf(LOG_INFO, "CLI", "Announce triggered.");
                }

                // Query: peer list as JSON
                if (strncmp(serialRxBuffer, "peers", 5) == 0 && (serialRxBuffer[5] == '\0' || serialRxBuffer[5] == ' ')) {
                    JsonDocument doc;
                    doc["query"] = "peers";
                    JsonArray arr = doc["data"].to<JsonArray>();
                    for (size_t i = 0; i < peerList.size(); i++) {
                        JsonObject p = arr.add<JsonObject>();
                        p["call"] = peerList[i].nodeCall;
                        p["rssi"] = peerList[i].rssi;
                        p["snr"] = peerList[i].snr;
                        p["available"] = peerList[i].available;
                        p["port"] = peerList[i].port;
                    }
                    logJson(doc);
                }

                // Query: routing table as JSON
                if (strncmp(serialRxBuffer, "routes", 6) == 0 && (serialRxBuffer[6] == '\0' || serialRxBuffer[6] == ' ')) {
                    JsonDocument doc;
                    doc["query"] = "routes";
                    JsonArray arr = doc["data"].to<JsonArray>();
                    for (size_t i = 0; i < routingList.size(); i++) {
                        JsonObject r = arr.add<JsonObject>();
                        r["dst"] = routingList[i].srcCall;
                        r["via"] = routingList[i].viaCall;
                        r["hops"] = routingList[i].hopCount;
                    }
                    logJson(doc);
                }

                // Query: ACK list as JSON
                if (strncmp(serialRxBuffer, "acks", 4) == 0 && (serialRxBuffer[4] == '\0' || serialRxBuffer[4] == ' ')) {
                    JsonDocument doc;
                    doc["query"] = "acks";
                    JsonArray arr = doc["data"].to<JsonArray>();
                    for (int i = 0; i < MAX_STORED_ACK; i++) {
                        if (acks[i].id != 0) {
                            JsonObject a = arr.add<JsonObject>();
                            a["srcCall"] = acks[i].srcCall;
                            a["nodeCall"] = acks[i].nodeCall;
                            a["id"] = acks[i].id;
                        }
                    }
                    logJson(doc);
                }

                // Query: TX buffer status as JSON
                // NOTE: must NOT use "txbuf" — conflicts with "t" (time set)
                if (strncmp(serialRxBuffer, "xtxbuf", 6) == 0 && (serialRxBuffer[6] == '\0' || serialRxBuffer[6] == ' ')) {
                    JsonDocument doc;
                    doc["query"] = "txbuf";
                    doc["count"] = txBuffer.size();
                    JsonArray arr = doc["data"].to<JsonArray>();
                    for (size_t i = 0; i < txBuffer.size(); i++) {
                        JsonObject t = arr.add<JsonObject>();
                        t["frameType"] = txBuffer[i].frameType;
                        t["viaCall"] = txBuffer[i].viaCall;
                        t["id"] = txBuffer[i].id;
                        t["retry"] = txBuffer[i].retry;
                    }
                    logJson(doc);
                }

            }
            //Clear buffer
            serialRxBuffer[0] = '\0';
        } else {
            //RX byte into buffer
            size_t len = strlen(serialRxBuffer);
            if (len < sizeof(serialRxBuffer) - 1) {
                serialRxBuffer[len] = rx;
                serialRxBuffer[len + 1] = '\0';
            }

        }
    }
    
}

