#ifdef HAS_WIFI
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "heapdbg.h"
#include <HTTPUpdate.h>

#include "wifiFunctions.h"
#include "serial.h"
#include "statusDisplay.h"
#include "settings.h"
#include "logging.h"
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
#ifdef ESP32_E22_V1
#include "display_ESP32_E22_V1.h"
#endif


uint32_t ledTimer = 0;
uint32_t reconnectTimer = 0;
byte wifiStatus = 0xff;
bool wiFiLED = false;
bool apModeKey = false;
bool pendingReconnectScan = false;
static volatile bool pendingWifiReconnect = false;
static volatile int  pendingReconnectIdx  = -1;
static volatile bool pendingScanBroadcast = false;
static void processDeferredScanActions();

static void sendOtaLog(const char* event, const char* versionFrom, const char* versionTo, const char* errorMsg) {
    WiFiClient logClient;
    HTTPClient logHttp;
    if (!logHttp.begin(logClient, "http://www.rMesh.de:8082/ota_log.php")) return;
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
    logPrintf(LOG_INFO, "OTA", "%s", msg);
}

void checkForUpdates(bool force, uint8_t forceChannel) {
    HEAP_SCOPE("checkForUpdates");
    if (WiFi.status() != WL_CONNECTED) return;
    if (strcmp(VERSION, "unknown") == 0) {
        sendUpdateStatus("No update: dev build (unknown).");
        return;
    }
    // Manually built/flashed version (git describe: "v1.0.25a-3-gb480c38"):
    // Pattern: -<digits>-g<hex> – no automatic update unless force=true
    bool isGitDescribe = false;
    const char* dashG = strstr(VERSION, "-g");
    if (dashG != nullptr && dashG > VERSION) {
        // Check if there are digits and another "-" before "-g" (commit counter)
        const char* p = dashG - 1;
        while (p > VERSION && isdigit((unsigned char)*p)) p--;
        if (*p == '-' && p < dashG - 1 && isxdigit((unsigned char)*(dashG + 2))) {
            isGitDescribe = true;
        }
    }
    if (!force && isGitDescribe) {
        sendUpdateStatus("No automatic update: local dev build.");
        return;
    }
    // Nightly builds should never auto-update
    if (!force && strncmp(VERSION, "nightly-", 8) == 0) {
        sendUpdateStatus("No automatic update: nightly build.");
        return;
    }

    // Check version
    sendUpdateStatus("Checking for updates...");
    WiFiClient client;
    HTTPClient http;
    http.setTimeout(10000);  // 10s HTTP timeout
    uint8_t activeChannel = force ? forceChannel : updateChannel;
    char latestUrl[256];
    snprintf(latestUrl, sizeof(latestUrl),
             "http://www.rMesh.de:8082/latest.php?call=%s&device=%s&version=%s&channel=%s",
             settings.mycall, PIO_ENV_NAME, VERSION,
             (activeChannel == 1) ? "dev" : "release");
    if (!http.begin(client, latestUrl)) {
        sendUpdateStatus("Update server unreachable.");
        return;
    }
    if (http.GET() != 200) {
        http.end();
        sendUpdateStatus("Update server unreachable.");
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, http.getStream()) != DeserializationError::Ok) {
        http.end();
        sendUpdateStatus("Invalid update server response.");
        return;
    }
    const char* latestTag = doc["version"];
    if (!latestTag) {
        http.end();
        sendUpdateStatus("No update found.");
        return;
    }
    // Same version (install anyway if force is set)
    if (!force && strcmp(latestTag, VERSION) == 0) {
        http.end();
        sendUpdateStatus("Already up to date.");
        return;
    }
    // Current version is a dev build ahead of the tag (git describe: "v1.0.25a-3-gb480c38")
    // → installed version is newer, no update needed (install anyway if force is set)
    // Check if VERSION starts with "latestTag-" (dev build ahead of tag)
    size_t tagLen = strlen(latestTag);
    if (!force && strncmp(VERSION, latestTag, tagLen) == 0 && VERSION[tagLen] == '-') {
        http.end();
        sendUpdateStatus("Already up to date (dev build).");
        return;
    }
    http.end();

    // New update found
    char newVersion[32];
    strlcpy(newVersion, latestTag, sizeof(newVersion));
    {
        char msg[64];
        snprintf(msg, sizeof(msg), "Installing update %s...", newVersion);
        sendUpdateStatus(msg);
    }
    char callParam[128];
    snprintf(callParam, sizeof(callParam), "&call=%s&device=%s&tag=%s",
             settings.mycall, PIO_ENV_NAME, newVersion);
    httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    showStatusDisplayFlashing("Filesystem");

    // LittleFS – up to 3 attempts
    char spiffsUrl[256];
    snprintf(spiffsUrl, sizeof(spiffsUrl),
             "http://www.rMesh.de:8082/update.php?file=%s_littlefs.bin%s",
             PIO_ENV_NAME, callParam);
    t_httpUpdate_return spiffsResult = HTTP_UPDATE_FAILED;
    for (int attempt = 1; attempt <= 3; attempt++) {
        if (attempt > 1) {
            char retryMsg[64];
            snprintf(retryMsg, sizeof(retryMsg), "LittleFS update attempt %d/3...", attempt);
            sendUpdateStatus(retryMsg);
            delay(3000);
        }
        WiFiClient spiffsClient;
        spiffsClient.setTimeout(120000);
        spiffsResult = httpUpdate.updateSpiffs(spiffsClient, spiffsUrl);
        if (spiffsResult != HTTP_UPDATE_FAILED) break;
    }
    if (spiffsResult == HTTP_UPDATE_FAILED) {
        char warnMsg[192];
        snprintf(warnMsg, sizeof(warnMsg),
                 "LittleFS not updated: %s – continuing with firmware",
                 httpUpdate.getLastErrorString().c_str());
        sendUpdateStatus(warnMsg);
    }

    // Firmware – on success the node reboots, onEnd fires shortly before
    // Capture newVersion by copy (it's a local char[])
    char capturedVersion[32];
    strlcpy(capturedVersion, newVersion, sizeof(capturedVersion));
    httpUpdate.onEnd([capturedVersion]() {
        sendOtaLog("update_success", VERSION, capturedVersion, "");
    });

    showStatusDisplayFlashing("Firmware");

    // Firmware – up to 3 attempts
    char fwUrl[256];
    snprintf(fwUrl, sizeof(fwUrl),
             "http://www.rMesh.de:8082/update.php?file=%s_firmware.bin%s",
             PIO_ENV_NAME, callParam);
    t_httpUpdate_return fwResult = HTTP_UPDATE_FAILED;
    for (int attempt = 1; attempt <= 3; attempt++) {
        if (attempt > 1) {
            char retryMsg[64];
            snprintf(retryMsg, sizeof(retryMsg), "Firmware update attempt %d/3...", attempt);
            sendUpdateStatus(retryMsg);
            delay(3000);
        }
        WiFiClient fwClient;
        fwClient.setTimeout(120000);
        fwResult = httpUpdate.update(fwClient, fwUrl);
        if (fwResult != HTTP_UPDATE_FAILED) break;
    }
    // Only reached if failed (success = reboot)
    if (fwResult == HTTP_UPDATE_FAILED) {
        char errMsg[192];
        snprintf(errMsg, sizeof(errMsg), "Update failed (firmware): %s",
                 httpUpdate.getLastErrorString().c_str());
        sendUpdateStatus(errMsg);
        char logDetail[128];
        snprintf(logDetail, sizeof(logDetail), "Firmware: %s",
                 httpUpdate.getLastErrorString().c_str());
        sendOtaLog("update_failed", VERSION, newVersion, logDetail);
    }
}

void showWiFiStatus() {
    if (!wifiEnabled) return;

    // Process deferred WiFi scan actions from event callback
    processDeferredScanActions();

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

    //Status LED
    if (settings.apMode) {
        //AP mode
        if ((int32_t)(millis() - ledTimer) >= 0) {
            wiFiLED = !wiFiLED;
            setWiFiLED (wiFiLED);
            ledTimer = millis() + 750;
        }
    } else {
        //Client mode
        if (wifiStatus != WiFi.status()) {
            uint8_t newStatus = WiFi.status();
            logPrintf(LOG_DEBUG, "WiFi", "status_change from=%d to=%d rssi=%d heap=%u uptime=%lu",
                    wifiStatus, newStatus, WiFi.RSSI(), ESP.getFreeHeap(), millis() / 1000);
            wifiStatus = newStatus;
            if (newStatus == WL_CONNECTED) {
                initUDP();
                pendingManualUpdate = true;
            }
        }

        if (WiFi.status() == WL_CONNECTED) {
        //Connected -> short blink
            if ((int32_t)(millis() - ledTimer) >= 0) {
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
            if ((int32_t)(millis() - reconnectTimer) >= 0 && WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
                reconnectTimer = millis() + 30000;
                logPrintf(LOG_DEBUG, "WiFi", "reconnect_attempt status=%d networks=%d heap=%u",
                        WiFi.status(), (int)wifiNetworks.size(), ESP.getFreeHeap());
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

// WiFi diagnostics counters
uint32_t wifiDisconnectCount = 0;
uint8_t  lastWifiDisconnectReason = 0;
uint32_t lastWifiDisconnectTime = 0;

void onWiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    uint8_t reason = info.wifi_sta_disconnected.reason;
    wifiDisconnectCount++;
    lastWifiDisconnectReason = reason;
    lastWifiDisconnectTime = (uint32_t)time(nullptr);
    logPrintf(LOG_DEBUG, "WiFi", "disconnected reason=%d rssi=%d heap=%u uptime=%lu",
        reason, WiFi.RSSI(), ESP.getFreeHeap(), millis() / 1000);
}

void onWiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
    HEAP_SCOPE("onWiFiGotIP_mDNS");
    MDNS.end();
    String mdnsName = String(settings.mycall) + "-rmesh";
    mdnsName.toLowerCase();
    if (MDNS.begin(mdnsName.c_str())) {
        MDNS.addService("http", "tcp", 80);
        logPrintf(LOG_INFO, "mDNS", "started: %s.local", mdnsName.c_str());
    } else {
        logPrintf(LOG_ERROR, "mDNS", "failed to start");
    }
#ifdef HAS_ETHERNET
    // Set WiFi as default outbound route if configured as primary
    if (primaryInterface == 1) {
        WiFi.STA.setDefault();
        logPrintf(LOG_INFO, "WiFi", "Set as primary interface (outbound route)");
    }
#endif
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
            // Defer WiFi reconnect to loop context to avoid deadlock
            pendingReconnectIdx = bestIdx;
            pendingWifiReconnect = true;
        }
    } else {
        pendingReconnectScan = false;
    }

    // Defer scan broadcast to loop context
    pendingScanBroadcast = true;
}

/**
 * Called from showWiFiStatus() (loop context) to process deferred scan results.
 */
static void processDeferredScanActions() {
    if (pendingWifiReconnect) {
        pendingWifiReconnect = false;
        int idx = pendingReconnectIdx;
        // Copy credentials before use — wifiNetworks can be modified concurrently
        // by the WebSocket settings handler on the async server task.
        char ssid[WIFI_NETWORK_SSID_LEN] = {0};
        char password[WIFI_NETWORK_PW_LEN] = {0};
        bool valid = false;
        if (idx >= 0 && idx < (int)wifiNetworks.size()) {
            strlcpy(ssid, wifiNetworks[idx].ssid, sizeof(ssid));
            strlcpy(password, wifiNetworks[idx].password, sizeof(password));
            valid = (ssid[0] != '\0');
        }
        if (valid) {
            logPrintf(LOG_INFO, "WiFi", "Reconnecting to %s", ssid);
            WiFi.disconnect();
            if (!settings.dhcpActive) {
                WiFi.config(settings.wifiIP, settings.wifiGateway, settings.wifiNetMask, settings.wifiDNS);
            }
            WiFi.begin(ssid, password);
        }
    }
    if (pendingScanBroadcast) {
        HEAP_SCOPE("scanBroadcast");
        pendingScanBroadcast = false;
        int n = WiFi.scanComplete();
        if (n > 0) {
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
            // Use a fixed heap buffer sized from measureJson to avoid the
            // dynamic-growth reallocations of Arduino String, which badly
            // fragment the heap on repeated WiFi scans.
            size_t jlen = measureJson(doc);
            if (jlen > 0 && jlen < 8192 && ESP.getMaxAllocHeap() > jlen + 1024) {
                char* jbuf = (char*)malloc(jlen + 1);
                if (jbuf) {
                    serializeJson(doc, jbuf, jlen + 1);
                    wsBroadcast(jbuf, jlen);
                    free(jbuf);
                }
            }
        }
        WiFi.scanDelete();
    }
}


void wifiInit() {
    if (!wifiEnabled) {
        WiFi.mode(WIFI_OFF);
        logPrintf(LOG_INFO, "WiFi", "WiFi disabled in settings.");
        return;
    }
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
