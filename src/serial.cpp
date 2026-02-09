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
                if (strncmp(serialRxBuffer, "r", 1) == 0) {
                    Serial.println("Reboot...");
                    rebootTimer = 0;
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
                if (strncmp(serialRxBuffer, "p", 1) == 0) {
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

