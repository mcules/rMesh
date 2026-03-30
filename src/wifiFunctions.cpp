#ifdef HAS_WIFI
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
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

#ifdef HELTEC_WIFI_LORA_32_V3
#include "display_HELTEC_WiFi_LoRa_32_V3.h"
#endif
#ifdef HELTEC_HT_TRACKER_V1_2
#include "display_HELTEC_HT-Tracker_V1_2.h"
#endif
#ifdef LILYGO_T3_LORA32_V1_6_1
#include "display_LILYGO_T3_LoRa32_V1_6_1.h"
#endif
#ifdef LILYGO_T_BEAM
#include "display_LILYGO_T-Beam.h"
#endif


uint64_t ledTimer = 0;
uint64_t reconnectTimer = 0;
byte wifiStatus = 0xff;
bool wiFiLED = false;
bool apModeKey = false;
bool pendingReconnectScan = false;

static void sendOtaLog(const char* event, const char* versionFrom, const char* versionTo, const char* errorMsg) {
    WiFiClientSecure logClient;
    logClient.setInsecure();
    HTTPClient logHttp;
    if (!logHttp.begin(logClient, "https://www.rMesh.de/ota_log.php")) return;
    logHttp.addHeader("Content-Type", "application/json");
    logHttp.setTimeout(5000);
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
    if (WiFi.status() != WL_CONNECTED) return;
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
    client.setTimeout(10);  // 10s TLS timeout
    HTTPClient http;
    http.setTimeout(10000);  // 10s HTTP timeout
    uint8_t activeChannel = force ? forceChannel : updateChannel;
    String latestUrl = "https://www.rMesh.de/latest.php?call=";
    latestUrl += settings.mycall;
    latestUrl += "&device=";
    latestUrl += PIO_ENV_NAME;
    latestUrl += "&version=";
    latestUrl += VERSION;
    latestUrl += "&channel=";
    latestUrl += (activeChannel == 1) ? "dev" : "release";
    if (!http.begin(client, latestUrl)) {
        sendUpdateStatus("Update-Server nicht erreichbar.");
        return;
    }
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
        spiffsClient.setTimeout(120000);
        spiffsResult = httpUpdate.updateSpiffs(spiffsClient, spiffsUrl);
        if (spiffsResult != HTTP_UPDATE_FAILED) break;
    }
    if (spiffsResult == HTTP_UPDATE_FAILED) {
        // LittleFS nicht verfügbar (z.B. kein Release-Asset) – Firmware-Update trotzdem fortsetzen
        String warnMsg = "LittleFS nicht aktualisiert: " + httpUpdate.getLastErrorString() + " – fahre mit Firmware fort";
        sendUpdateStatus(warnMsg.c_str());
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
        fwClient.setTimeout(120000);
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

#if defined(HELTEC_WIFI_LORA_32_V3) || defined(LILYGO_T3_LORA32_V1_6_1) || defined(LILYGO_T_BEAM) || defined(HELTEC_HT_TRACKER_V1_2)
    // Long press (>=2s): toggle AP/Client mode + reboot
    // Short press (<2s): toggle status display on/off
    //
    // Initialise buttonPressed to the current button state so that a
    // permanently LOW GPIO 0 (e.g. serial DTR holding it down) does not
    // register as a fresh press.  Only a HIGH→LOW transition counts.
    static bool firstCall = true;
    static bool buttonPressed = false;
    static uint32_t buttonPressTime = 0;
    static bool longPressHandled = false;
    if (firstCall) {
        firstCall = false;
        buttonPressed = getKeyApMode();   // seed with current state
        longPressHandled = buttonPressed; // suppress stale long-press
        buttonPressTime = millis();
        return;                           // skip first evaluation
    }

    bool currentButton = getKeyApMode();

    if (currentButton && !buttonPressed) {
        // Button just pressed (HIGH→LOW transition)
        buttonPressed = true;
        buttonPressTime = millis();
        longPressHandled = false;
    }

    if (currentButton && buttonPressed && !longPressHandled) {
        // Button still held – check for long press
        if (millis() - buttonPressTime >= 2000) {
            longPressHandled = true;
            settings.apMode = !settings.apMode;
            saveSettings();
            rebootTimer = millis(); rebootRequested = true;
            delay(500);
        }
    }

    if (!currentButton && buttonPressed) {
        // Button released
        buttonPressed = false;
        if (!longPressHandled && (millis() - buttonPressTime >= 50)) {
            // Short press – toggle status display (persisted setting)
            if (hasStatusDisplay()) {
                if (oledEnabled) {
                    disableStatusDisplay();
                } else {
                    enableStatusDisplay();
                }
            }
        }
    }

#else
    // Original behaviour: simple press toggles AP mode
    if (getKeyApMode() != apModeKey) {
        delay(100);
        apModeKey = getKeyApMode();
        if (apModeKey == 1) {
            settings.apMode = !settings.apMode;
            saveSettings();
            rebootTimer = millis(); rebootRequested = true;
            delay(500);
        }
    }
#endif

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
            // Not connected -> LED off + attempt reconnect
            setWiFiLED(false);
            if (millis() > reconnectTimer && WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
                reconnectTimer = millis() + 30000;
                if (wifiNetworks.size() > 1) {
                    // Multiple networks: scan and connect to best available
                    pendingReconnectScan = true;
                    WiFi.scanNetworks(true);
                } else {
                    WiFi.reconnect();
                }
            }
        }
    }
}

void onWiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
    MDNS.end();
    String mdnsName = String(settings.mycall) + "-rmesh";
    mdnsName.toLowerCase();
    if (MDNS.begin(mdnsName.c_str())) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("mDNS started: %s.local\n", mdnsName.c_str());
    } else {
        Serial.println("mDNS failed to start");
    }
}

void onWiFiScanDone(WiFiEvent_t event, WiFiEventInfo_t info) {
    int n = WiFi.scanComplete();

    // Multi-network reconnect: pick best available network from list
    if (pendingReconnectScan && n > 0) {
        pendingReconnectScan = false;
        int bestIdx = -1;
        int bestRSSI = -200;

        // Favorites first
        for (size_t i = 0; i < wifiNetworks.size(); i++) {
            if (!wifiNetworks[i].favorite) continue;
            for (int j = 0; j < n; j++) {
                if (strcmp(WiFi.SSID(j).c_str(), wifiNetworks[i].ssid) == 0 && WiFi.RSSI(j) > bestRSSI) {
                    bestRSSI = WiFi.RSSI(j);
                    bestIdx = (int)i;
                }
            }
        }
        // If no favorite found, try any network from list
        if (bestIdx < 0) {
            bestRSSI = -200;
            for (size_t i = 0; i < wifiNetworks.size(); i++) {
                for (int j = 0; j < n; j++) {
                    if (strcmp(WiFi.SSID(j).c_str(), wifiNetworks[i].ssid) == 0 && WiFi.RSSI(j) > bestRSSI) {
                        bestRSSI = WiFi.RSSI(j);
                        bestIdx = (int)i;
                    }
                }
            }
        }
        if (bestIdx >= 0) {
            Serial.printf("Reconnecting to %s (RSSI: %d)\n", wifiNetworks[bestIdx].ssid, bestRSSI);
            WiFi.disconnect();
            if (!settings.dhcpActive) {
                WiFi.config(settings.wifiIP, settings.wifiGateway, settings.wifiNetMask, settings.wifiDNS);
            }
            WiFi.begin(wifiNetworks[bestIdx].ssid, wifiNetworks[bestIdx].password);
        }
    } else {
        pendingReconnectScan = false;
    }

    // Send scan results to WebUI
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
}


void wifiInit() {
    WiFi.mode(WIFI_STA);
    if (settings.apMode) {
        // Access Point mode
        WiFi.mode(WIFI_AP);
        esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
        WiFi.softAPConfig(IPAddress(192,168,1,1), IPAddress(192,168,1,1), IPAddress(255,255,255,0));
        const char* ssid = apName.isEmpty() ? "rMesh" : apName.c_str();
        // WiFi AP password must be >= 8 chars; empty or short = open network
        if (apPassword.length() >= 8) {
            WiFi.softAP(ssid, apPassword.c_str());
        } else {
            WiFi.softAP(ssid);
        }
    } else {
        // Station mode: connect to favorite or first network from list
        WiFi.disconnect();
        WiFi.mode(WIFI_STA);
        if (!settings.dhcpActive) {
            WiFi.config(settings.wifiIP, settings.wifiGateway, settings.wifiNetMask, settings.wifiDNS);
        }
        const char* connectSsid = nullptr;
        const char* connectPw   = nullptr;
        if (!wifiNetworks.empty()) {
            int idx = 0;
            for (size_t i = 0; i < wifiNetworks.size(); i++) {
                if (wifiNetworks[i].favorite) { idx = (int)i; break; }
            }
            connectSsid = wifiNetworks[idx].ssid;
            connectPw   = wifiNetworks[idx].password;
        } else if (settings.wifiSSID[0] != '\0') {
            // Fallback to legacy single SSID
            connectSsid = settings.wifiSSID;
            connectPw   = settings.wifiPassword;
        }
        if (connectSsid && connectSsid[0] != '\0') {
            WiFi.begin(connectSsid, connectPw);
        }
        // Only auto-reconnect when single network; multi-network uses scan-based reconnect
        WiFi.setAutoReconnect(wifiNetworks.size() <= 1);
    }
    WiFi.setSleep(false);
    // Set hostname before connecting (mDNS is started later in onWiFiGotIP)
    String mdnsName = String(settings.mycall) + "-rmesh";
    mdnsName.toLowerCase();
    WiFi.setHostname(mdnsName.c_str());
    WiFi.setTxPower((wifi_power_t)(wifiTxPower * 4));
}


#endif // HAS_WIFI
