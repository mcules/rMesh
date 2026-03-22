#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>

#include "wifiFunctions.h"
#include "settings.h"
#include "hal.h"
#include "config.h"
#include "webFunctions.h"
#include "udp.h"
#include "main.h"
#include "esp_wifi.h"


uint64_t ledTimer = 0;
uint64_t reconnectTimer = 0;
byte wifiStatus = 0xff;
bool wiFiLED = false;
bool apModeKey = false;

static void sendOtaLog(const char* event, const char* versionFrom, const char* versionTo, const char* errorMsg) {
    WiFiClientSecure logClient;
    logClient.setInsecure();
    HTTPClient logHttp;
    if (!logHttp.begin(logClient, "https://www.rMesh.de/ota_log.php")) return;
    logHttp.addHeader("Content-Type", "application/json");
    JsonDocument doc;
    doc["call"]         = settings.mycall;
    doc["device"]       = PIO_ENV_NAME;
    doc["event"]        = event;
    doc["version_from"] = versionFrom;
    doc["version_to"]   = versionTo;
    doc["error"]        = errorMsg;
    String body;
    serializeJson(doc, body);
    logHttp.POST(body);
    logHttp.end();
}

static void sendUpdateStatus(const char* msg) {
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"updateStatus\":\"%s\"}", msg);
    wsBroadcast(buf, strlen(buf));
    Serial.printf("[OTA] %s\n", msg);
}

void checkForUpdates(bool force, uint8_t forceChannel) {
    if (strcmp(VERSION, "unknown") == 0) {
        sendUpdateStatus("Kein Update: Dev-Build (unknown).");
        return;
    }
    // Manuell gebaute/geflashte Version (git describe: "v1.0.25a-3-gb480c38"):
    // Muster: -<Ziffern>-g<Hex> – kein automatisches Update außer bei force=true
    bool isGitDescribe = false;
    const char* dashG = strstr(VERSION, "-g");
    if (dashG != nullptr && dashG > VERSION) {
        // Prüfe ob vor "-g" Ziffern und ein weiteres "-" stehen (Commit-Zähler)
        const char* p = dashG - 1;
        while (p > VERSION && isdigit((unsigned char)*p)) p--;
        if (*p == '-' && p < dashG - 1 && isxdigit((unsigned char)*(dashG + 2))) {
            isGitDescribe = true;
        }
    }
    if (!force && isGitDescribe) {
        sendUpdateStatus("Kein automatisches Update: lokaler Dev-Build.");
        return;
    }

    // Version prüfen
    sendUpdateStatus("Suche nach Updates...");
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    uint8_t activeChannel = force ? forceChannel : updateChannel;
    String latestUrl = "https://www.rMesh.de/latest.php?call=";
    latestUrl += settings.mycall;
    latestUrl += "&device=";
    latestUrl += PIO_ENV_NAME;
    latestUrl += "&version=";
    latestUrl += VERSION;
    latestUrl += "&channel=";
    latestUrl += (activeChannel == 1) ? "dev" : "release";
    http.begin(client, latestUrl);
    if (http.GET() != 200) {
        http.end();
        sendUpdateStatus("Update-Server nicht erreichbar.");
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, http.getStream()) != DeserializationError::Ok) {
        http.end();
        sendUpdateStatus("Antwort des Update-Servers ungültig.");
        return;
    }
    const char* latestTag = doc["version"];
    if (!latestTag) {
        http.end();
        sendUpdateStatus("Kein Update gefunden.");
        return;
    }
    // Gleiche Version (bei force trotzdem installieren)
    if (!force && strcmp(latestTag, VERSION) == 0) {
        http.end();
        sendUpdateStatus("Bereits aktuell.");
        return;
    }
    // Aktuelle Version ist ein Dev-Build ahead des Tags (git describe: "v1.0.25a-3-gb480c38")
    // → installierte Version ist neuer, kein Update nötig (bei force trotzdem installieren)
    if (!force && String(VERSION).startsWith(String(latestTag) + "-")) {
        http.end();
        sendUpdateStatus("Bereits aktuell (Dev-Build).");
        return;
    }
    http.end();

    // Neues Update gefunden
    String newVersion = latestTag;
    {
        char msg[64];
        snprintf(msg, sizeof(msg), "Update %s wird installiert...", newVersion.c_str());
        sendUpdateStatus(msg);
    }
    String callParam = "&call=";
    callParam += settings.mycall;
    callParam += "&device=";
    callParam += PIO_ENV_NAME;
    callParam += "&tag=";
    callParam += newVersion;
    httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

    // LittleFS – bis zu 3 Versuche
    String spiffsUrl = "https://www.rMesh.de/update.php?file=" PIO_ENV_NAME "_littlefs.bin" + callParam;
    t_httpUpdate_return spiffsResult = HTTP_UPDATE_FAILED;
    for (int attempt = 1; attempt <= 3; attempt++) {
        if (attempt > 1) {
            char retryMsg[64];
            snprintf(retryMsg, sizeof(retryMsg), "LittleFS-Update Versuch %d/3...", attempt);
            sendUpdateStatus(retryMsg);
            delay(3000);
        }
        WiFiClientSecure spiffsClient;
        spiffsClient.setInsecure();
        spiffsClient.setTimeout(30000);
        spiffsResult = httpUpdate.updateSpiffs(spiffsClient, spiffsUrl);
        if (spiffsResult != HTTP_UPDATE_FAILED) break;
    }
    if (spiffsResult == HTTP_UPDATE_FAILED) {
        String errMsg = "Update fehlgeschlagen (LittleFS): " + httpUpdate.getLastErrorString();
        sendUpdateStatus(errMsg.c_str());
        sendOtaLog("update_failed", VERSION, newVersion.c_str(),
            ("LittleFS: " + httpUpdate.getLastErrorString()).c_str());
        return;
    }

    // Firmware – bei Erfolg startet der Node neu, onEnd feuert kurz davor
    httpUpdate.onEnd([newVersion]() {
        sendOtaLog("update_success", VERSION, newVersion.c_str(), "");
    });

    // Firmware – bis zu 3 Versuche
    String fwUrl = "https://www.rMesh.de/update.php?file=" PIO_ENV_NAME "_firmware.bin" + callParam;
    t_httpUpdate_return fwResult = HTTP_UPDATE_FAILED;
    for (int attempt = 1; attempt <= 3; attempt++) {
        if (attempt > 1) {
            char retryMsg[64];
            snprintf(retryMsg, sizeof(retryMsg), "Firmware-Update Versuch %d/3...", attempt);
            sendUpdateStatus(retryMsg);
            delay(3000);
        }
        WiFiClientSecure fwClient;
        fwClient.setInsecure();
        fwClient.setTimeout(30000);
        fwResult = httpUpdate.update(fwClient, fwUrl);
        if (fwResult != HTTP_UPDATE_FAILED) break;
    }
    // Nur erreicht wenn fehlgeschlagen (Erfolg = Neustart)
    if (fwResult == HTTP_UPDATE_FAILED) {
        String errMsg = "Update fehlgeschlagen (Firmware): " + httpUpdate.getLastErrorString();
        sendUpdateStatus(errMsg.c_str());
        sendOtaLog("update_failed", VERSION, newVersion.c_str(),
            ("Firmware: " + httpUpdate.getLastErrorString()).c_str());
    }
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
            //Nicht Verbunden -> LED aus + Reconnect versuchen
            setWiFiLED(false);
            if (millis() > reconnectTimer) {
                reconnectTimer = millis() + 30000;
                WiFi.reconnect();
            }
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
        WiFi.setAutoReconnect(true);
    }
    WiFi.setSleep(false);
    WiFi.setHostname(settings.mycall);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);  
    WiFi.onEvent(onWiFiScanDone, ARDUINO_EVENT_WIFI_SCAN_DONE);  
}

