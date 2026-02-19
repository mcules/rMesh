#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

#include "wifiFunctions.h"
#include "settings.h"
#include "hal.h"
#include "config.h"
#include "webFunctions.h"
#include "udp.h"
#include "main.h"
#include "esp_wifi.h"


uint64_t ledTimer = 0;
byte wifiStatus = 0xff;
bool wiFiLED = false;
bool apModeKey = false;

void checkForUpdates() {
    WiFiClient client;
    // LittleFS Update prüfen
    httpUpdate.updateSpiffs(client, "http://dh1nfj.de/rMesh/update.php?file=" PIO_ENV_NAME "/" VERSION "/littlefs.bin");
    //Firmware prüfen
    httpUpdate.update(client, "http://dh1nfj.de/rMesh/update.php?file=" PIO_ENV_NAME "/" VERSION "/firmware.bin");
}

void showWiFiStatus() {
    //AP-Mode umschalten
    if (getKeyApMode() != apModeKey) {
        delay(100);
        apModeKey = getKeyApMode();
        if (apModeKey == 1) {
            if (settings.apMode == false) {
                settings.apMode = true;
            } else {
                settings.apMode = false;
            }
            saveSettings();
            rebootTimer = 0;
            delay(500);
        }
    }

    //Status-LED
    if (settings.apMode) {
        //AP-Mode
        if (millis() > ledTimer) {
            wiFiLED = !wiFiLED;
            setWiFiLED (wiFiLED);
            ledTimer = millis() + 750;
        }
    } else {
        //CLient-Mode
        if (wifiStatus != WiFi.status()) {
            wifiStatus = WiFi.status();   
            if (WiFi.status() == WL_CONNECTED) { 
                initUDP();
                checkForUpdates();
            }
        } 

        if (WiFi.status() == WL_CONNECTED) {
        //Verbunden -> kurz blinken
            if (millis() > ledTimer) {
                if (wiFiLED == true) {
                    wiFiLED = false;
                    ledTimer = millis() + 950;
                } else {
                    wiFiLED = true;
                    ledTimer = millis() + 50;
                }
                setWiFiLED(wiFiLED);
            }
        } else {
            //Nicht Verbunden -> LED aus
            setWiFiLED(false);
        }
    }
}

void onWiFiScanDone(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println("scan fertig...");
    int n = WiFi.scanComplete();
    JsonDocument doc;
    for (int i = 0; i < n; ++i) {
        doc["wifiScan"][i]["ssid"] = WiFi.SSID(i).c_str();
        doc["wifiScan"][i]["rssi"] = WiFi.RSSI(i);
        doc["wifiScan"][i]["channel"] = WiFi.channel(i);
        switch (WiFi.encryptionType(i)){
            case WIFI_AUTH_OPEN: doc["wifiScan"][i]["encryption"] = "open"; break;
            case WIFI_AUTH_WEP: doc["wifiScan"][i]["encryption"] = "WEP"; break;
            case WIFI_AUTH_WPA_PSK: doc["wifiScan"][i]["encryption"] = "WPA"; break;
            case WIFI_AUTH_WPA2_PSK: doc["wifiScan"][i]["encryption"] = "WPA2"; break;
            case WIFI_AUTH_WPA_WPA2_PSK: doc["wifiScan"][i]["encryption"] = "WPA+WPA2"; break;
            case WIFI_AUTH_WPA2_ENTERPRISE: doc["wifiScan"][i]["encryption"] = "WPA2-EAP"; break;
            case WIFI_AUTH_WPA3_PSK: doc["wifiScan"][i]["encryption"] = "WPA3"; break;
            case WIFI_AUTH_WPA2_WPA3_PSK: doc["wifiScan"][i]["encryption"] = "WPA2+WPA3"; break;
            case WIFI_AUTH_WAPI_PSK: doc["wifiScan"][i]["encryption"] = "WAPI"; break;
            default: doc["wifiScan"][i]["encryption"] = "unknown";
        }
    }
    String jsonOutput;
    serializeJson(doc, jsonOutput);
    ws.textAll(jsonOutput);

    if (n == 0) {
        Serial.println("no networks found");
    } else {
        Serial.print(n);
        Serial.println(" networks found");
        Serial.println("Nr | SSID                             | RSSI | CH | Encryption");
        for (int i = 0; i < n; ++i) {
            // Print SSID and RSSI for each network found
            Serial.printf("%2d",i + 1);
            Serial.print(" | ");
            Serial.printf("%-32.32s", WiFi.SSID(i).c_str());
            Serial.print(" | ");
            Serial.printf("%4d", WiFi.RSSI(i));
            Serial.print(" | ");
            Serial.printf("%2d", WiFi.channel(i));
            Serial.print(" | ");
            switch (WiFi.encryptionType(i))
            {
            case WIFI_AUTH_OPEN:
                Serial.print("open");
                break;
            case WIFI_AUTH_WEP:
                Serial.print("WEP");
                break;
            case WIFI_AUTH_WPA_PSK:
                Serial.print("WPA");
                break;
            case WIFI_AUTH_WPA2_PSK:
                Serial.print("WPA2");
                break;
            case WIFI_AUTH_WPA_WPA2_PSK:
                Serial.print("WPA+WPA2");
                break;
            case WIFI_AUTH_WPA2_ENTERPRISE:
                Serial.print("WPA2-EAP");
                break;
            case WIFI_AUTH_WPA3_PSK:
                Serial.print("WPA3");
                break;
            case WIFI_AUTH_WPA2_WPA3_PSK:
                Serial.print("WPA2+WPA3");
                break;
            case WIFI_AUTH_WAPI_PSK:
                Serial.print("WAPI");
                break;
            default:
                Serial.print("unknown");
            }
            Serial.println();
        }
    }
    Serial.println("");
}


void wifiInit() {
    //Wifi Init
    WiFi.mode(WIFI_STA);
    if (settings.apMode) {
        //AP-Mode
        //Serial.println("Starte WiFi AP-Mode");
        WiFi.mode(WIFI_AP);
        esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
        WiFi.softAPConfig(IPAddress(192,168,1,1), IPAddress(192,168,1,1), IPAddress(255,255,255,0));
        WiFi.softAP("rMesh");
    } else {
        //Serial.println("Starte WiFi Client-Mode");
        WiFi.disconnect();
        WiFi.mode(WIFI_STA);
        if (!settings.dhcpActive) {
            //Feste IP
            WiFi.config(settings.wifiIP, settings.wifiGateway, settings.wifiNetMask, settings.wifiDNS);
        }
        WiFi.begin(settings.wifiSSID, settings.wifiPassword);
    }
    WiFi.setSleep(false);
    WiFi.setHostname(settings.mycall);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);  
    WiFi.onEvent(onWiFiScanDone, ARDUINO_EVENT_WIFI_SCAN_DONE);  
}

