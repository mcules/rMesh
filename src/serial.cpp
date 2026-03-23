#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <cstring>
#include <nvs_flash.h>

#include "frame.h"
#include "serial.h"
#include "config.h"
#include "settings.h"
#include "hal.h"
#include "settings.h"
#include "main.h"
#include "wifiFunctions.h"
#include "helperFunctions.h"
#include "routing.h"
#include "auth.h"
#include "mbedtls/md.h"

//String serialRxBuffer;

char serialRxBuffer[200] = {0}; 


void checkSerialRX() {
    if (Serial.available() > 0) {
        char rx = Serial.read();
        //Echo
        Serial.write(rx);
        if ((rx == 13) || (rx == 10)) {
            //Auswerten
            if (strlen(serialRxBuffer) > 0 ) {

                //Parameter kopieren
                char parameter[200] = {0};
                char* pos = strchr(serialRxBuffer, ' ');
                if (pos != nullptr) {
                    pos++; // hinter das Leerzeichen
                    strncpy(parameter, pos, sizeof(parameter) - 1); // sicheren Copy
                    Serial.println(parameter);
                }

                //Puffer nach Kleinbuchstaben
                for (int i = 0; serialRxBuffer[i] != '\0'; i++) {
                    serialRxBuffer[i] = tolower(serialRxBuffer[i]);
                }
                
                //Befehle auswerten

                //Testfunktionen
                if (strncmp(serialRxBuffer, "t", 1) == 0) {
                    struct tm tm;
                    tm.tm_year = 2024 - 1900; // Jahr seit 1900
                    tm.tm_mon = 1;            // Februar (0-11, also 1 = Februar)
                    tm.tm_mday = 14;          // Tag
                    tm.tm_hour = 4;           // Stunde
                    tm.tm_min = 59;           // Minute
                    tm.tm_sec = 0;            // Sekunde

                    time_t t = mktime(&tm);
                    struct timeval now = { .tv_sec = t };
                    settimeofday(&now, NULL);

                    Serial.println("Uhrzeit manuell auf 03:59:00 gesetzt!");         
                }

                //Hilfe
                if (strncmp(serialRxBuffer, "h", 1) == 0) {
                    File file = LittleFS.open("/help.txt", "r");
                    if (file) {
                        while (file.available()) {
                            String line = file.readStringUntil('\n');
                            line.replace("\r", "");
                            Serial.println(line);
                        }
                        file.close();
                    } else {
                        Serial.println("Fehler: /help.txt nicht gefunden. Filesystem neu flashen?");
                    }
                }

                //Version
                if (strncmp(serialRxBuffer, "v", 1) == 0) {
                    //+ BOARD TYPE
                    Serial.printf("\n\n\n%s\n%s %s\nREADY.\n", PIO_ENV_NAME, NAME, VERSION);   
                }

                //Settings
                if (strncmp(serialRxBuffer, "se", 2) == 0) {
                    showSettings();
                }

                //Reboot
                if (strncmp(serialRxBuffer, "reb", 3) == 0) {
                    Serial.println("Reboot...");
                    rebootTimer = 0;
                }

                //OTA Update
                if (strncmp(serialRxBuffer, "upd", 3) == 0) {
                    Serial.println("OTA Update gestartet...");
                    checkForUpdates();
                }

                // Update-Kanal setzen: "uc 0" = Release, "uc 1" = Dev
                if (strncmp(serialRxBuffer, "uc", 2) == 0 && (serialRxBuffer[2] == ' ' || serialRxBuffer[2] == '\0')) {
                    if (strlen(parameter) > 0) {
                        updateChannel = (uint8_t)atoi(parameter);
                        saveSettings();
                    }
                    Serial.printf("updateChannel: %d (%s)\n", updateChannel, updateChannel == 1 ? "dev" : "release");
                }

                // Force-Install: "updf 0" = Release, "updf 1" = Dev
                if (strncmp(serialRxBuffer, "updf", 4) == 0) {
                    pendingForceChannel = (strlen(parameter) > 0) ? (uint8_t)atoi(parameter) : updateChannel;
                    pendingForceUpdate = true;
                    Serial.printf("Force-Install gestartet (Kanal: %s)...\n", pendingForceChannel == 1 ? "dev" : "release");
                }

                //Wifi Scannen
                if (strncmp(serialRxBuffer, "sc", 2) == 0) {
                    Serial.println("WiFi scan.....");
                    WiFi.scanNetworks(true);
                }

                //Wifi SSID
                if (strncmp(serialRxBuffer, "ss", 2) == 0) {
                    if (strlen(parameter) > 0) {
                        strncpy(settings.wifiSSID, parameter, sizeof(settings.wifiSSID) - 1);
                        settings.wifiSSID[sizeof(settings.wifiSSID) - 1] = '\0'; 
                        settings.apMode = false;
                        saveSettings();
                        wifiInit();
                    }
                    Serial.printf("WiFi SSID: %s\n", settings.wifiSSID);
                }    
                
                //Wifi Password
                if (strncmp(serialRxBuffer, "pa", 2) == 0) {
                    if (strlen(parameter) > 0) {
                        strncpy(settings.wifiPassword, parameter, sizeof(settings.wifiPassword) - 1);
                        settings.wifiPassword[sizeof(settings.wifiPassword) - 1] = '\0'; 
                        settings.apMode = false;
                        saveSettings();
                        wifiInit();
                    }
                    Serial.printf("WiFi Passwort: %s\n", settings.wifiPassword);
                }            
                
                //IP-Adresse
                if (strncmp(serialRxBuffer, "i", 1) == 0) {
                    if (strlen(parameter) > 0) {
                        IPAddress tempIP;
                        if (tempIP.fromString(parameter)) {
                            settings.wifiIP = tempIP;
                            saveSettings();
                            wifiInit();
                        } else {
                            Serial.println("Fehler: Ungültiges IP-Format!");
                        }                    
                    }
                    Serial.printf("IP: %d.%d.%d.%d\n", settings.wifiIP[0], settings.wifiIP[1], settings.wifiIP[2], settings.wifiIP[3]);
                }       
                
                //Gateway
                if (strncmp(serialRxBuffer, "g", 1) == 0) {
                    if (strlen(parameter) > 0) {
                        IPAddress tempIP;
                        if (tempIP.fromString(parameter)) {
                            settings.wifiGateway = tempIP;
                            saveSettings();
                            wifiInit();
                        } else {
                            Serial.println("Fehler: Ungültiges IP-Format!");
                        }                    
                    }
                    Serial.printf("Gateway: %d.%d.%d.%d\n", settings.wifiGateway[0], settings.wifiGateway[1], settings.wifiGateway[2], settings.wifiGateway[3]);
                }

                //DNS-Adresse
                if (strncmp(serialRxBuffer, "dn", 2) == 0) {
                    if (strlen(parameter) > 0) {
                        IPAddress tempIP;
                        if (tempIP.fromString(parameter)) {
                            settings.wifiDNS = tempIP;
                            saveSettings();
                            wifiInit();
                        } else {
                            Serial.println("Fehler: Ungültiges IP-Format!");
                        }                    
                    }
                    Serial.printf("DNS: %d.%d.%d.%d\n", settings.wifiDNS[0], settings.wifiDNS[1], settings.wifiDNS[2], settings.wifiDNS[3]);
                }

                //Netmask
                if (strncmp(serialRxBuffer, "ne", 2) == 0) {
                    if (strlen(parameter) > 0) {
                        IPAddress tempIP;
                        if (tempIP.fromString(parameter)) {
                            settings.wifiNetMask = tempIP;
                            saveSettings();
                            wifiInit();
                        } else {
                            Serial.println("Fehler: Ungültiges IP-Format!");
                        }                    
                    }
                    Serial.printf("Netmask: %d.%d.%d.%d\n", settings.wifiNetMask[0], settings.wifiNetMask[1], settings.wifiNetMask[2], settings.wifiNetMask[3]);
                }

                //AP-Mode
                if (strncmp(serialRxBuffer, "a", 1) == 0) {
                    if (strlen(parameter) > 0) {
                        bool value = false;
                        if (parameter[0] == '1') value = true;
                        if (parameter[0] == 'e') value = true;
                        if (parameter[0] == 't') value = true;
                        settings.apMode = value;
                        saveSettings();
                        wifiInit();
                    }
                    Serial.printf("AP-Mode: %s\n", settings.apMode ? "true" : "false");
                }

                //DHCP-Mode
                if (strncmp(serialRxBuffer, "dh", 2) == 0) {
                    if (strlen(parameter) > 0) {
                        bool value = false;
                        if (parameter[0] == '1') value = true;
                        if (parameter[0] == 'e') value = true;
                        if (parameter[0] == 't') value = true;
                        settings.dhcpActive = value;
                        saveSettings();
                        wifiInit();
                    }
                    Serial.printf("DHCP: %s\n", settings.dhcpActive ? "true" : "false");
                } 

                //Defaults
                if (strncmp(serialRxBuffer, "de", 2) == 0) {
                    std::memset(settings.mycall, 0xff, sizeof(settings.mycall));
                    nvs_flash_erase(); // Löscht die gesamte NVS-Partition
                    nvs_flash_init();  // Initialisiert sie neu
                    rebootTimer = 0;
                }

                // Frequenz-Preset
                // "freq 433" → 433-MHz-Amateurfunk-Preset
                // "freq 868" → 868-MHz-Public-Preset (Sub-Band P, 500 mW)
                if (strncmp(serialRxBuffer, "fr", 2) == 0) {
                    if (strncmp(parameter, "433", 3) == 0) {
                        settings.loraFrequency       = 434.850f;
                        settings.loraBandwidth       = 62.5f;
                        settings.loraSpreadingFactor = 7;
                        settings.loraCodingRate      = 6;
                        settings.loraOutputPower     = 20;
                        settings.loraPreambleLength  = 10;
                        settings.loraSyncWord        = syncWordForFrequency(settings.loraFrequency);
                        saveSettings();
                        Serial.println("Preset 433 MHz (Amateurfunk) gesetzt.");
                    } else if (strncmp(parameter, "868", 3) == 0) {
                        settings.loraFrequency       = 869.525f;
                        settings.loraBandwidth       = 125.0f;
                        settings.loraSpreadingFactor = 7;
                        settings.loraCodingRate      = 5;
                        settings.loraOutputPower     = 27;
                        settings.loraPreambleLength  = 10;
                        settings.loraSyncWord        = syncWordForFrequency(settings.loraFrequency);
                        saveSettings();
                        Serial.println("Preset 868 MHz (Public, 500 mW) gesetzt.");
                    } else {
                        Serial.printf("Aktuelle Frequenz: %.3f MHz\n", settings.loraFrequency);
                        Serial.println("Verwendung: freq 433  oder  freq 868");
                    }
                }


                // Callsign
                // "call DL1ABC-1" → Callsign setzen
                if (strncmp(serialRxBuffer, "call", 4) == 0) {
                    if (strlen(parameter) > 0) {
                        strncpy(settings.mycall, parameter, sizeof(settings.mycall) - 1);
                        settings.mycall[sizeof(settings.mycall) - 1] = '\0';
                        for (size_t i = 0; settings.mycall[i]; i++) settings.mycall[i] = toupper(settings.mycall[i]);
                        saveSettings();
                    }
                    Serial.printf("Callsign: %s\n", settings.mycall);
                }

                // Position
                // "pos JN48mw" oder "pos 48.1234,11.5678"
                if (strncmp(serialRxBuffer, "pos", 3) == 0) {
                    if (strlen(parameter) > 0) {
                        strncpy(settings.position, parameter, sizeof(settings.position) - 1);
                        settings.position[sizeof(settings.position) - 1] = '\0';
                        saveSettings();
                    }
                    Serial.printf("Position: %s\n", settings.position);
                }

                // NTP-Server
                if (strncmp(serialRxBuffer, "ntp", 3) == 0) {
                    if (strlen(parameter) > 0) {
                        strncpy(settings.ntpServer, parameter, sizeof(settings.ntpServer) - 1);
                        settings.ntpServer[sizeof(settings.ntpServer) - 1] = '\0';
                        saveSettings();
                    }
                    Serial.printf("NTP: %s\n", settings.ntpServer);
                }

                // TX-Power (Output Power in dBm)
                // "op 20" → TX-Power auf 20 dBm setzen
                if (strncmp(serialRxBuffer, "op", 2) == 0) {
                    if (strlen(parameter) > 0) {
                        int8_t txp = (int8_t)atoi(parameter);
                        if (isPublicBand(settings.loraFrequency) && txp > PUBLIC_MAX_TX_POWER) { txp = PUBLIC_MAX_TX_POWER; }
                        settings.loraOutputPower = txp;
                        saveSettings();
                    }
                    Serial.printf("TX Power: %d dBm\n", settings.loraOutputPower);
                }

                // Bandwidth in kHz
                // "bw 62.5"
                if (strncmp(serialRxBuffer, "bw", 2) == 0) {
                    if (strlen(parameter) > 0) {
                        settings.loraBandwidth = atof(parameter);
                        saveSettings();
                    }
                    Serial.printf("Bandwidth: %.2f kHz\n", settings.loraBandwidth);
                }

                // Spreading Factor (6–12)
                if (strncmp(serialRxBuffer, "sf", 2) == 0) {
                    if (strlen(parameter) > 0) {
                        settings.loraSpreadingFactor = (uint8_t)atoi(parameter);
                        saveSettings();
                    }
                    Serial.printf("Spreading Factor: %d\n", settings.loraSpreadingFactor);
                }

                // Coding Rate (5–8)
                if (strncmp(serialRxBuffer, "cr", 2) == 0) {
                    if (strlen(parameter) > 0) {
                        settings.loraCodingRate = (uint8_t)atoi(parameter);
                        saveSettings();
                    }
                    Serial.printf("Coding Rate: %d\n", settings.loraCodingRate);
                }

                // Preamble Length
                // "pl 10"
                if (strncmp(serialRxBuffer, "pl", 2) == 0) {
                    if (strlen(parameter) > 0) {
                        settings.loraPreambleLength = (int16_t)atoi(parameter);
                        saveSettings();
                    }
                    Serial.printf("Preamble Length: %d\n", settings.loraPreambleLength);
                }

                // Sync Word (hexadezimal, z.B. "sw 2B")
                if (strncmp(serialRxBuffer, "sw", 2) == 0) {
                    if (strlen(parameter) > 0) {
                        settings.loraSyncWord = (uint8_t)strtol(parameter, nullptr, 16);
                        saveSettings();
                    }
                    Serial.printf("SyncWord: %02X\n", settings.loraSyncWord);
                }

                // Repeat / Relay
                // "rep 1" oder "rep 0"
                if (strncmp(serialRxBuffer, "rep", 3) == 0) {
                    if (strlen(parameter) > 0) {
                        bool value = (parameter[0] == '1' || parameter[0] == 'e' || parameter[0] == 't');
                        settings.loraRepeat = value;
                        saveSettings();
                    }
                    Serial.printf("Repeat: %s\n", settings.loraRepeat ? "true" : "false");
                }

                // Max Hop Message
                if (strncmp(serialRxBuffer, "mhm", 3) == 0) {
                    if (strlen(parameter) > 0) {
                        extSettings.maxHopMessage = (uint8_t)atoi(parameter);
                        saveSettings();
                    }
                    Serial.printf("MaxHopMessage: %d\n", extSettings.maxHopMessage);
                }

                // Max Hop Position
                if (strncmp(serialRxBuffer, "mhp", 3) == 0) {
                    if (strlen(parameter) > 0) {
                        extSettings.maxHopPosition = (uint8_t)atoi(parameter);
                        saveSettings();
                    }
                    Serial.printf("MaxHopPosition: %d\n", extSettings.maxHopPosition);
                }

                // Max Hop Telemetry
                if (strncmp(serialRxBuffer, "mht", 3) == 0) {
                    if (strlen(parameter) > 0) {
                        extSettings.maxHopTelemetry = (uint8_t)atoi(parameter);
                        saveSettings();
                    }
                    Serial.printf("MaxHopTelemetry: %d\n", extSettings.maxHopTelemetry);
                }

                // WebUI-Passwort
                // "webpw <passwort>" → Passwort setzen (wird als SHA256 gespeichert)
                // "webpw -"          → Passwort loeschen
                if (strncmp(serialRxBuffer, "webp", 4) == 0) {
                    if (strlen(parameter) > 0) {
                        if (strcmp(parameter, "-") == 0) {
                            savePasswordHash("");
                            Serial.println("WebUI-Passwort geloescht.");
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
                            savePasswordHash(String(hexHash));
                            Serial.println("WebUI-Passwort gesetzt.");
                        }
                    } else {
                        Serial.printf("WebUI-Passwort: %s\n", webPasswordHash.isEmpty() ? "nicht gesetzt" : "gesetzt");
                    }
                }

                // UDP-Peer-Verwaltung
                // "udp"                  → alle Peers auflisten
                // "udp add <IP>"         → Peer anhängen
                // "udp add <IP> legacy"  → Legacy-Peer anhängen
                // "udp del <N>"          → Peer N löschen (1-basiert)
                // "udp <N> <IP>"         → IP von Peer N setzen
                // "udp <N> legacy"       → Legacy-Flag von Peer N umschalten
                // "udp <N> enabled"      → Enabled-Flag von Peer N umschalten
                // "udp clear"            → alle Peers löschen
                if (strncmp(serialRxBuffer, "udp", 3) == 0) {
                    if (parameter == nullptr || parameter[0] == '\0') {
                        // Alle Peers auflisten
                        if (udpPeers.empty()) {
                            Serial.println("UDP Peers: keine");
                        } else {
                            for (size_t i = 0; i < udpPeers.size(); i++) {
                                Serial.printf("UDP-Peer %zu: %d.%d.%d.%d%s\n", i + 1,
                                    udpPeers[i][0], udpPeers[i][1], udpPeers[i][2], udpPeers[i][3],
                                    udpPeerLegacy[i] ? " [legacy]" : "");
                            }
                        }
                    } else if (strncmp(parameter, "clear", 5) == 0) {
                        udpPeers.clear();
                        udpPeerLegacy.clear();
                        saveUdpPeers();
                        Serial.println("Alle UDP-Peers gelöscht.");
                    } else if (strncmp(parameter, "add ", 4) == 0) {
                        const char* rest = parameter + 4;
                        // IP extrahieren (bis Leerzeichen oder Ende)
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
                            saveUdpPeers();
                            Serial.printf("UDP-Peer %zu hinzugefügt: %d.%d.%d.%d%s\n",
                                udpPeers.size(), tempIP[0], tempIP[1], tempIP[2], tempIP[3],
                                legacy ? " [legacy]" : "");
                        } else {
                            Serial.println("Fehler: Ungültiges IP-Format!");
                        }
                    } else if (strncmp(parameter, "del ", 4) == 0) {
                        int idx = atoi(parameter + 4) - 1;
                        if (idx >= 0 && (size_t)idx < udpPeers.size()) {
                            Serial.printf("UDP-Peer %d gelöscht: %d.%d.%d.%d\n", idx + 1,
                                udpPeers[idx][0], udpPeers[idx][1], udpPeers[idx][2], udpPeers[idx][3]);
                            udpPeers.erase(udpPeers.begin() + idx);
                            udpPeerLegacy.erase(udpPeerLegacy.begin() + idx);
                            udpPeerEnabled.erase(udpPeerEnabled.begin() + idx);
                            saveUdpPeers();
                        } else {
                            Serial.printf("Fehler: Index 1–%zu erwartet!\n", udpPeers.size());
                        }
                    } else {
                        // "udp <N> <IP>", "udp <N> legacy" oder "udp <N> enabled"
                        char* spacePos = strchr(parameter, ' ');
                        if (spacePos != nullptr) {
                            int idx = atoi(parameter) - 1;
                            spacePos++;
                            if (idx >= 0 && (size_t)idx < udpPeers.size()) {
                                if (strncmp(spacePos, "legacy", 6) == 0) {
                                    udpPeerLegacy[idx] = !udpPeerLegacy[idx];
                                    saveUdpPeers();
                                    Serial.printf("UDP-Peer %d legacy: %s\n", idx + 1, (bool)udpPeerLegacy[idx] ? "an" : "aus");
                                } else if (strncmp(spacePos, "enabled", 7) == 0) {
                                    udpPeerEnabled[idx] = !(bool)udpPeerEnabled[idx];
                                    saveUdpPeers();
                                    Serial.printf("UDP-Peer %d enabled: %s\n", idx + 1, (bool)udpPeerEnabled[idx] ? "an" : "aus");
                                } else {
                                    IPAddress tempIP;
                                    if (tempIP.fromString(spacePos)) {
                                        udpPeers[idx] = tempIP;
                                        saveUdpPeers();
                                        Serial.printf("UDP-Peer %d: %d.%d.%d.%d\n", idx + 1, tempIP[0], tempIP[1], tempIP[2], tempIP[3]);
                                    } else {
                                        Serial.println("Fehler: Ungültiges IP-Format!");
                                    }
                                }
                            } else {
                                Serial.printf("Fehler: Index 1–%zu erwartet!\n", udpPeers.size());
                            }
                        } else {
                            Serial.println("Befehle: udp | udp add <IP> [legacy] | udp del <N> | udp <N> <IP> | udp <N> legacy | udp <N> enabled | udp clear");
                        }
                    }
                }

            }
            //Puffer löschen
            serialRxBuffer[0] = '\0';
        } else {
            //RX-Byte in Puffer
            size_t len = strlen(serialRxBuffer);
            if (len < sizeof(serialRxBuffer) - 1) {
                serialRxBuffer[len] = rx;
                serialRxBuffer[len + 1] = '\0';
            }

        }
    }
    
}

