#ifdef HAS_WIFI
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <Update.h>

#include "config.h"
#include "hal/settings.h"
#include "hal/hal.h"
#include "network/wifiFunctions.h"
#include "main.h"
#include "util/helperFunctions.h"
#include "mesh/peer.h"
#include "mesh/routing.h"
#include "network/auth.h"
#include "util/serial.h"
#include "util/logging.h"
#include "util/heapdbg.h"
#include "network/api.h"
#include "network/apiAuth.h"
#include "display/statusDisplay.h"

AsyncWebServer webServer(80);
AsyncWebSocketMessageHandler wsHandler;
AsyncWebSocket ws("/socket", wsHandler.eventHandler());

/**
 * Broadcasts a WebSocket message.
 *
 * Sends to all clients if no web password is set.
 * Otherwise, sends only to authenticated clients.
 */
void wsBroadcast(const char *buf, size_t len) {
    // ws.textAll/text internally allocates shared_ptr<vector> on the heap.
    // Under low heap conditions, operator new would trigger std::terminate().
    // Small messages (notifications < 30 bytes) are allowed at lower heap;
    // larger messages (status, monitor) need more headroom.
    uint32_t minHeap = (len < 30) ? 15000 : 40000;
    if (ESP.getFreeHeap() < minHeap || ESP.getMaxAllocHeap() < 8000) return;

    if (webPasswordHash.isEmpty()) {
        ws.textAll(buf, len);
        return;
    }
    for (int i = 0; i < MAX_AUTH_SESSIONS; i++) {
        if (authSessions[i].clientId != 0 && authSessions[i].authenticated) {
            ws.text(authSessions[i].clientId, buf, len);
        }
    }
}

// ── Lightweight WebSocket notifications ─────────────────────────────────────
// Instead of serializing full JSON on the heap, send a tiny "data changed"
// event.  The WebUI fetches the updated data via the REST API.

// Lightweight "something changed" notifications: the payload is a
// constant 20-byte string, so they do not grow the WS send buffer or the
// heap beyond what the WS stack already reserves per client. The WebUI
// re-fetches /api/peers or /api/routes via REST on receipt.
void notifyPeerListChanged() {
    wsBroadcast("{\"notify\":\"peers\"}", 18);
}

void notifyRoutingChanged() {
    wsBroadcast("{\"notify\":\"routes\"}", 19);
}

void notifySettingsChanged() {
    // Settings changes are rare; still broadcast so open WebUIs refresh.
    wsBroadcast("{\"notify\":\"settings\"}", 21);
}

void notifyNewMessage() {
    wsBroadcast("{\"notify\":\"messages\"}", 21);
}

/**
 * Send initial data to a single WebSocket client on connect/auth.
 * Uses notification messages that tell the client to fetch via REST API.
 * The REST API does not require auth when no password is set; when a password
 * IS set, we send the full settings inline (since the client can't fetch
 * the API without HTTP auth headers), but peers and routes use notifications
 * (the JS fetches /api/peers and /api/routes without auth — we add a
 * session-cookie bypass for these endpoints).
 *
 * Simpler approach: just send the same notifications. The client fetches
 * the API. For the no-password case this works. For the password case,
 * the API endpoints skip auth-check for requests originating from the
 * same IP as an authenticated WebSocket client.
 *
 * SIMPLEST approach (chosen): Send data directly via WebSocket to the
 * connecting client, using small JSON snippets.
 */
static void sendInitialDataToClient(AsyncWebSocketClient *client) {
    if (ESP.getFreeHeap() < 15000 || ESP.getMaxAllocHeap() < 8000) return;

    // Settings must be sent inline (WebUI needs mycall/version before anything works).
    // Use a minimal JSON with just the essential fields — avoids the old 17KB JsonDocument.
    {
        char json[512];
        int n = snprintf(json, sizeof(json),
            "{\"settings\":{\"mycall\":\"%s\",\"version\":\"%s\","
            "\"name\":\"%s\",\"hardware\":\"%s\","
            "\"webPasswordSet\":%s,"
            "\"loraMaxMessageLength\":%u}}",
            settings.mycall, VERSION, NAME, PIO_ENV_NAME,
            !webPasswordHash.isEmpty() ? "true" : "false",
            (unsigned)settings.loraMaxMessageLength);
        if (n > 0 && n < (int)sizeof(json)) {
            client->text(json, n);
        }
    }

    // After minimal settings, tell client to fetch full data via API
    // (API auth is bypassed for IPs with authenticated WS sessions)
    client->text("{\"notify\":\"settings\"}");
    client->text("{\"notify\":\"peers\"}");
    client->text("{\"notify\":\"routes\"}");
}

/**
 * Initializes and starts the web server and WebSocket handlers.
 */
void startWebServer() {
    //---------------------- WEBSOCKET -------------------------

    wsHandler.onConnect([](AsyncWebSocket *server, AsyncWebSocketClient *client) {
        ws.cleanupClients();
        setClientAuth(client->id(), false, (uint32_t)client->remoteIP());

        if (webPasswordHash.isEmpty()) {
            sendInitialDataToClient(client);
        } else {
            char nonce[33];
            generateNonce(client->id(), nonce);
            uint64_t mac = ESP.getEfuseMac();
            char chipId[13];
            snprintf(chipId, sizeof(chipId), "%02X%02X%02X%02X%02X%02X",
                     (uint8_t) (mac >> 40), (uint8_t) (mac >> 32), (uint8_t) (mac >> 24),
                     (uint8_t) (mac >> 16), (uint8_t) (mac >> 8), (uint8_t) (mac));
            char challenge[256];
            snprintf(challenge, sizeof(challenge),
                     "{\"auth\":{\"required\":true,\"nonce\":\"%s\",\"mycall\":\"%s\",\"chipId\":\"%s\"}}",
                     nonce, settings.mycall, chipId);
            client->text(challenge);
        }
    });

    wsHandler.onDisconnect([](AsyncWebSocket *server, uint32_t clientId) {
        ws.cleanupClients();
        removeClientAuth(clientId);
    });

    wsHandler.onError([](AsyncWebSocket *server, AsyncWebSocketClient *client, uint16_t errorCode, const char *reason,
                         size_t len) {
        logPrintf(LOG_ERROR, "Web", "Client %" PRIu32 " error: %" PRIu16 ": %s", client->id(), errorCode, reason);
        ws.cleanupClients();
    });

    // Handle incoming WebSocket messages
    wsHandler.onMessage([](AsyncWebSocket *server, AsyncWebSocketClient *client, const uint8_t *data, size_t len) {
        JsonDocument json;
        DeserializationError error = deserializeJson(json, data, len);

        // Authentication handshake
        if (json["auth"].is<JsonVariant>()) {
            if (json["auth"]["response"].is<JsonVariant>()) {
                const char* response = json["auth"]["response"] | "";
                if (verifyAuthResponse(client->id(), response)) {
                    setClientAuth(client->id(), true, (uint32_t)client->remoteIP());
                    client->text("{\"auth\":{\"ok\":true}}");
                    sendInitialDataToClient(client);
                } else {
                    char nonce[33];
                    generateNonce(client->id(), nonce);
                    char msg[192];
                    snprintf(msg, sizeof(msg),
                             "{\"auth\":{\"error\":\"Wrong password\",\"nonce\":\"%s\"}}",
                             nonce);
                    client->text(msg);
                }
            }
            return;
        }

        // Ignore all non-auth messages from unauthenticated clients
        if (!isAuthenticated(client->id())) return;

        if (json["ping"].is<JsonVariant>()) {
            // Ping message for testing
        }

        // Set or clear the web password hash
        if (json["setPassword"].is<JsonVariant>()) {
            const char* hash = json["setPassword"] | "";
            logPrintf(LOG_INFO, "Auth", "setPassword received, hash='%s'", hash);
            savePasswordHash(hash);
            setClientAuth(client->id(), true);
            logPrintf(LOG_INFO, "Auth", "webPasswordHash now: '%s'", webPasswordHash.c_str());

            const char* resp = (hash[0] == '\0')
                              ? "{\"passwordSaved\":false}"
                              : "{\"passwordSaved\":true}";
            logPrintf(LOG_INFO, "Auth", "Sending to client %u: %s", client->id(), resp);
            client->text(resp);
            return;
        }

        // Save settings from the web UI
        if (json["settings"].is<JsonVariant>()) {
            if (json["settings"]["mycall"].is<JsonVariant>()) {
                const char *mycallSrc = json["settings"]["mycall"] | "";
                safeUtf8Copy(settings.mycall, (const uint8_t *) mycallSrc, sizeof(settings.mycall), sizeof(settings.mycall));
                for (size_t i = 0; i < sizeof(settings.mycall); i++) {
                    settings.mycall[i] = toupper(settings.mycall[i]);
                }
                settings.mycall[sizeof(settings.mycall) - 1] = '\0';
            }
            if (json["settings"]["position"].is<JsonVariant>()) {
                strlcpy(settings.position, json["settings"]["position"] | "", sizeof(settings.position));
            }
            if (json["settings"]["ntp"].is<JsonVariant>()) {
                strlcpy(settings.ntpServer, json["settings"]["ntp"] | "", sizeof(settings.ntpServer));
            }
            if (json["settings"]["dhcpActive"].is<JsonVariant>()) {
                settings.dhcpActive = json["settings"]["dhcpActive"].as<bool>();
            }
            if (json["settings"]["wifiSSID"].is<JsonVariant>()) {
                strlcpy(settings.wifiSSID, json["settings"]["wifiSSID"] | "", sizeof(settings.wifiSSID));
            }
            if (json["settings"]["wifiPassword"].is<JsonVariant>()) {
                const char* pw = json["settings"]["wifiPassword"] | "";
                if (strcmp(pw, "***") != 0) {
                    strlcpy(settings.wifiPassword, pw, sizeof(settings.wifiPassword));
                }
            }
            if (json["settings"]["apMode"].is<JsonVariant>()) {
                settings.apMode = json["settings"]["apMode"].as<bool>();
            }
            if (json["settings"]["apName"].is<JsonVariant>()) {
                apName = json["settings"]["apName"] | "rMesh";
            }
            if (json["settings"]["apPassword"].is<JsonVariant>()) {
                apPassword = json["settings"]["apPassword"] | "";
            }
            // Update WiFi network list
            if (json["settings"]["wifiNetworks"].is<JsonArray>()) {
                JsonArray nets = json["settings"]["wifiNetworks"];
                std::vector<WifiNetwork> oldNetworks = wifiNetworks;
                wifiNetworks.clear();
                for (JsonObject n : nets) {
                    WifiNetwork net;
                    memset(&net, 0, sizeof(net));
                    strlcpy(net.ssid,     n["ssid"] | "",     sizeof(net.ssid));
                    const char* pw = n["password"] | "";
                    if (strcmp(pw, "***") == 0) {
                        for (auto& old : oldNetworks) {
                            if (strcmp(old.ssid, net.ssid) == 0) {
                                strlcpy(net.password, old.password, sizeof(net.password));
                                break;
                            }
                        }
                    } else {
                        strlcpy(net.password, pw, sizeof(net.password));
                    }
                    net.favorite = n["favorite"] | false;
                    if (net.ssid[0] != '\0') {
                        wifiNetworks.push_back(net);
                    }
                }
                // Sync legacy wifiSSID/wifiPassword to new favorite
                for (auto& net : wifiNetworks) {
                    if (net.favorite) {
                        strlcpy(settings.wifiSSID,     net.ssid,     sizeof(settings.wifiSSID));
                        strlcpy(settings.wifiPassword, net.password, sizeof(settings.wifiPassword));
                        break;
                    }
                }
            }
            if (json["settings"]["wifiIP"].is<JsonVariant>()) {
                JsonArray ipArray = json["settings"]["wifiIP"];
                for (int i = 0; i < 4; i++) { settings.wifiIP[i] = ipArray[i] | 0; }
            }
            if (json["settings"]["wifiNetMask"].is<JsonVariant>()) {
                JsonArray ipArray = json["settings"]["wifiNetMask"];
                for (int i = 0; i < 4; i++) { settings.wifiNetMask[i] = ipArray[i] | 0; }
            }
            if (json["settings"]["wifiGateway"].is<JsonVariant>()) {
                JsonArray ipArray = json["settings"]["wifiGateway"];
                for (int i = 0; i < 4; i++) { settings.wifiGateway[i] = ipArray[i] | 0; }
            }
            if (json["settings"]["wifiDNS"].is<JsonVariant>()) {
                JsonArray ipArray = json["settings"]["wifiDNS"];
                for (int i = 0; i < 4; i++) { settings.wifiDNS[i] = ipArray[i] | 0; }
            }
            if (json["settings"]["wifiBrodcast"].is<JsonVariant>()) {
                JsonArray ipArray = json["settings"]["wifiBrodcast"];
                for (int i = 0; i < 4; i++) { settings.wifiBrodcast[i] = ipArray[i] | 0; }
            }

            // Update UDP peer list
            if (json["settings"]["udpPeers"].is<JsonArray>()) {
                JsonArray peers = json["settings"]["udpPeers"];
                udpPeers.clear();
                udpPeerLegacy.clear();
                udpPeerEnabled.clear();
                udpPeerCall.clear();
                for (JsonObject p: peers) {
                    JsonVariant v = p["ip"];
                    if (v.is<JsonArray>() && v.size() == 4) {
                        JsonArray ip = v.as<JsonArray>();
                        udpPeers.push_back(IPAddress(ip[0] | 0, ip[1] | 0, ip[2] | 0, ip[3] | 0));
                        udpPeerLegacy.push_back(p["legacy"] | false);
                        udpPeerEnabled.push_back(p["enabled"] | true);
                        udpPeerCall.push_back(UdpPeerCallsign(p["call"] | ""));
                    }
                }
            }

            if (json["settings"]["loraFrequency"].is<JsonVariant>()) {
                settings.loraFrequency = json["settings"]["loraFrequency"].as<float>();
                if (isPublicBand(settings.loraFrequency) && settings.loraOutputPower > PUBLIC_MAX_TX_POWER) {
                    settings.loraOutputPower = PUBLIC_MAX_TX_POWER;
                }
            }
            if (json["settings"]["loraOutputPower"].is<JsonVariant>()) {
                settings.loraOutputPower = json["settings"]["loraOutputPower"].as<int8_t>();

                if (settings.loraOutputPower > LORA_MAX_TX_POWER) {
                    logPrintf(LOG_WARN, "Config", "TX Power %d dBm exceeds board max (%d dBm).", settings.loraOutputPower, LORA_MAX_TX_POWER);
                }
                if (isPublicBand(settings.loraFrequency) && settings.loraOutputPower > PUBLIC_MAX_TX_POWER) {
                    settings.loraOutputPower = PUBLIC_MAX_TX_POWER;
                }
            }
            if (json["settings"]["loraBandwidth"].is<JsonVariant>()) {
                settings.loraBandwidth = json["settings"]["loraBandwidth"].as<float>();
            }
            if (json["settings"]["loraSyncWord"].is<JsonVariant>()) {
                settings.loraSyncWord = json["settings"]["loraSyncWord"].as<uint8_t>();
            }
            if (json["settings"]["loraCodingRate"].is<JsonVariant>()) {
                settings.loraCodingRate = json["settings"]["loraCodingRate"].as<uint8_t>();
            }
            if (json["settings"]["loraSpreadingFactor"].is<JsonVariant>()) {
                settings.loraSpreadingFactor = json["settings"]["loraSpreadingFactor"].as<uint8_t>();
            }
            if (json["settings"]["loraPreambleLength"].is<JsonVariant>()) {
                settings.loraPreambleLength = json["settings"]["loraPreambleLength"].as<int16_t>();
            }
            if (json["settings"]["loraRepeat"].is<JsonVariant>()) {
                settings.loraRepeat = json["settings"]["loraRepeat"].as<bool>();
            }
            if (json["settings"]["maxHopMessage"].is<JsonVariant>()) {
                extSettings.maxHopMessage = json["settings"]["maxHopMessage"].as<uint8_t>();
            }
            if (json["settings"]["maxHopPosition"].is<JsonVariant>()) {
                extSettings.maxHopPosition = json["settings"]["maxHopPosition"].as<uint8_t>();
            }
            if (json["settings"]["maxHopTelemetry"].is<JsonVariant>()) {
                extSettings.maxHopTelemetry = json["settings"]["maxHopTelemetry"].as<uint8_t>();
            }
            if (json["settings"]["minSnr"].is<JsonVariant>()) {
                extSettings.minSnr = json["settings"]["minSnr"].as<int8_t>();
            }
            if (json["settings"]["updateChannel"].is<JsonVariant>()) {
                updateChannel = json["settings"]["updateChannel"].as<uint8_t>();
            }
            if (json["settings"]["loraEnabled"].is<JsonVariant>()) {
                loraEnabled = json["settings"]["loraEnabled"].as<bool>();
            }
            if (json["settings"]["batteryEnabled"].is<JsonVariant>()) {
                batteryEnabled = json["settings"]["batteryEnabled"].as<bool>();
            }
            if (json["settings"]["batteryFullVoltage"].is<JsonVariant>()) {
                batteryFullVoltage = json["settings"]["batteryFullVoltage"].as<float>();
            }
            if (json["settings"]["wifiTxPower"].is<JsonVariant>()) {
                wifiTxPower = json["settings"]["wifiTxPower"].as<int8_t>();
                if (wifiTxPower < 2) wifiTxPower = 2;
                if (wifiTxPower > WIFI_MAX_TX_POWER_DBM) wifiTxPower = WIFI_MAX_TX_POWER_DBM;
            }
            if (json["settings"]["displayBrightness"].is<JsonVariant>()) {
                displayBrightness = json["settings"]["displayBrightness"].as<uint8_t>();
                if (displayBrightness < 5) displayBrightness = 5;
            }
            if (json["settings"]["cpuFrequency"].is<JsonVariant>()) {
                uint16_t freq = json["settings"]["cpuFrequency"].as<uint16_t>();
                if (freq == 80 || freq == 160 || freq == 240) {
                    cpuFrequency = freq;
                }
            }
            if (json["settings"]["oledEnabled"].is<JsonVariant>()) {
                oledEnabled = json["settings"]["oledEnabled"].as<bool>();
            }
            if (json["settings"]["serialDebug"].is<JsonVariant>()) {
                serialDebug = json["settings"]["serialDebug"].as<bool>();
                Serial.setDebugOutput(serialDebug);
            }
            if (json["settings"]["heapDebug"].is<JsonVariant>()) {
                heapDebugEnabled = json["settings"]["heapDebug"].as<bool>();
            }
            if (json["settings"]["oledDisplayGroup"].is<JsonVariant>()) {
                strlcpy(oledDisplayGroup, json["settings"]["oledDisplayGroup"] | "", sizeof(oledDisplayGroup));
            }
            if (json["settings"]["oledPageInterval"].is<JsonVariant>()) {
                uint32_t iv = json["settings"]["oledPageInterval"].as<uint32_t>();
                if (iv < 1000)  iv = 1000;
                if (iv > 60000) iv = 60000;
                oledPageInterval = (uint16_t)iv;
            }
            if (json["settings"]["oledPageMask"].is<JsonVariant>()) {
                uint8_t m = json["settings"]["oledPageMask"].as<uint8_t>();
                if (m == 0) m = 0xFF;
                oledPageMask = m;
            }
            if (json["settings"]["oledButtonPin"].is<JsonVariant>()) {
                int pin = json["settings"]["oledButtonPin"].as<int>();
                if (pin < -1 || pin > 48) pin = -1;
                oledButtonPin = (int8_t)pin;
            }
            if (json["settings"]["groupNames"].is<JsonObject>()) {
                JsonObject gn = json["settings"]["groupNames"];
                for (int i = 3; i <= MAX_CHANNELS; i++) {
                    String key = String(i);
                    if (gn[key].is<const char*>()) {
                        strlcpy(groupNames[i], gn[key] | "", MAX_GROUP_NAME_LEN);
                    }
                }
                saveGroupNames();
            }

            // ── WiFi & Ethernet & per-interface settings ────────────────────
            if (json["settings"]["wifiEnabled"].is<JsonVariant>())
                wifiEnabled = json["settings"]["wifiEnabled"].as<bool>();
            if (json["settings"]["ethEnabled"].is<JsonVariant>())
                ethEnabled = json["settings"]["ethEnabled"].as<bool>();
            if (json["settings"]["ethDhcp"].is<JsonVariant>())
                ethDhcp = json["settings"]["ethDhcp"].as<bool>();
            if (json["settings"]["ethIP"].is<JsonArray>()) {
                JsonArray a = json["settings"]["ethIP"];
                for (int i = 0; i < 4; i++) ethIP[i] = a[i] | 0;
            }
            if (json["settings"]["ethNetMask"].is<JsonArray>()) {
                JsonArray a = json["settings"]["ethNetMask"];
                for (int i = 0; i < 4; i++) ethNetMask[i] = a[i] | 0;
            }
            if (json["settings"]["ethGateway"].is<JsonArray>()) {
                JsonArray a = json["settings"]["ethGateway"];
                for (int i = 0; i < 4; i++) ethGateway[i] = a[i] | 0;
            }
            if (json["settings"]["ethDNS"].is<JsonArray>()) {
                JsonArray a = json["settings"]["ethDNS"];
                for (int i = 0; i < 4; i++) ethDNS[i] = a[i] | 0;
            }
            if (json["settings"]["wifiNodeComm"].is<JsonVariant>())
                wifiNodeComm = json["settings"]["wifiNodeComm"].as<bool>();
            if (json["settings"]["wifiWebUI"].is<JsonVariant>())
                wifiWebUI = json["settings"]["wifiWebUI"].as<bool>();
            if (json["settings"]["ethNodeComm"].is<JsonVariant>())
                ethNodeComm = json["settings"]["ethNodeComm"].as<bool>();
            if (json["settings"]["ethWebUI"].is<JsonVariant>())
                ethWebUI = json["settings"]["ethWebUI"].as<bool>();
            if (json["settings"]["primaryInterface"].is<JsonVariant>()) {
                uint8_t v = json["settings"]["primaryInterface"].as<uint8_t>();
                if (v <= 2) primaryInterface = v;
            }

            pendingSettingsSave = true;
            if (hasStatusDisplay()) {
                if (oledEnabled) updateStatusDisplay();
                else disableStatusDisplay();
            }
        }

        // Send raw frame
        if (json["sendFrame"].is<JsonVariant>()) {
            Frame f;
            if (json["sendFrame"]["frameType"].is<JsonVariant>()) {
                f.frameType = json["sendFrame"]["frameType"].as<uint8_t>();
            }
            if (json["sendFrame"]["srcCall"].is<JsonVariant>()) {
                strlcpy(f.srcCall, json["sendFrame"]["srcCall"] | "", sizeof(f.srcCall));
            }
            if (json["sendFrame"]["dstGroup"].is<JsonVariant>()) {
                strlcpy(f.dstGroup, json["sendFrame"]["dstGroup"] | "", sizeof(f.dstGroup));
            }
            if (json["sendFrame"]["dstCall"].is<JsonVariant>()) {
                strlcpy(f.dstCall, json["sendFrame"]["dstCall"] | "", sizeof(f.dstCall));
            }
            if (json["sendFrame"]["messageType"].is<JsonVariant>()) {
                f.messageType = json["sendFrame"]["messageType"].as<uint8_t>();
            }
            if (json["sendFrame"]["messageLength"].is<JsonVariant>()) {
                f.messageLength = json["sendFrame"]["messageLength"].as<uint16_t>();
            }
            if (json["sendFrame"]["messageText"].is<JsonVariant>()) {
                const char *tempText = json["sendFrame"]["messageText"];
                size_t textLen = strlen(tempText);
                if (textLen > sizeof(f.message)) textLen = sizeof(f.message);
                memcpy((char *) f.message, tempText, textLen);
                f.messageLength = textLen;
            }
            if (json["sendFrame"]["message"].is<JsonArray>()) {
                JsonArray jsonMsg = json["sendFrame"]["message"].as<JsonArray>();
                uint8_t i = 0;
                for (uint8_t v: jsonMsg) {
                    if (i < sizeof(f.message)) {
                        f.message[i] = v;
                        i++;
                    }
                }
            }
            sendFrame(f);
        }

        if (json["sendMessage"].is<JsonVariant>()) {
            const char* dst = json["sendMessage"]["dst"] | "";
            const char* text = json["sendMessage"]["text"] | "";
            sendMessage(dst, text);
        }

        if (json["sendGroup"].is<JsonVariant>()) {
            const char* dst = json["sendGroup"]["dst"] | "";
            const char* text = json["sendGroup"]["text"] | "";
            sendGroup(dst, text);
        }

        if (json["deleteMessages"].is<JsonVariant>()) {
            LittleFS.remove("/messages.json");
            rebootTimer = millis() + 1000; rebootRequested = true;
        }

        if (json["trace"].is<JsonVariant>()) {
            char text[128];
            getFormattedTime("%H:%M:%S", text, sizeof(text));
            sendMessage(json["trace"]["dstCall"].as<const char *>(), text, Frame::MessageTypes::TRACE_MESSAGE);
        }

        if (json["time"].is<JsonVariant>()) {
            struct timeval tv;
            tv.tv_sec = json["time"].as<time_t>();
            tv.tv_usec = 0;
            settimeofday(&tv, NULL);
        }

        if (json["scanWifi"].is<JsonVariant>()) {
            logPrintf(LOG_INFO, "Web", "WiFi scan...");
            pendingReconnectScan = false;  // Don't treat this as a reconnect scan
            WiFi.scanNetworks(true);
        }

        if (json["announce"].is<JsonVariant>()) {
            logPrintf(LOG_INFO, "Web", "Send manual announce...");
            announceTimer = 0;
        }

        if (json["sendRoutes"].is<JsonVariant>()) {
            extern uint32_t routingInfoTimer;
            logPrintf(LOG_INFO, "Web", "Send manual ROUTING_INFO...");
            routingInfoTimer = 0;
        }

        if (json["tune"].is<JsonVariant>()) {
            logPrintf(LOG_INFO, "Web", "Send tune...");
            Frame f;
            f.frameType = Frame::FrameTypes::TUNE_FRAME;
            sendFrame(f);
        }

        if (json["reboot"].is<JsonVariant>()) {
            logPrintf(LOG_INFO, "Web", "Reboot");
            rebootTimer = millis(); rebootRequested = true;
        }

        if (json["shutdown"].is<JsonVariant>()) {
            logPrintf(LOG_INFO, "Web", "Shutdown");
            pendingShutdown = true;
        }

        // Deferred OTA update, handled in the main loop
        if (json["update"].is<JsonVariant>()) {
            logPrintf(LOG_INFO, "Web", "OTA update requested...");
            pendingManualUpdate = true;
        }

        // Deferred forced OTA install, handled in the main loop
        if (json["forceUpdate"].is<JsonVariant>()) {
            pendingForceChannel = json["forceUpdate"].as<uint8_t>(); // 0=release, 1=dev
            pendingForceUpdate = true;
            logPrintf(LOG_INFO, "Web", "Force install requested (channel: %s)...", pendingForceChannel == 1 ? "dev" : "release");
        }
    });

    webServer.addHandler(&ws);

    //---------------------- WEBSERVER -------------------------
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");

    // Serve files from LittleFS and fall back to 404
    webServer.onNotFound([](AsyncWebServerRequest *request) {
        if (!checkIfaceWebUI(request)) return;
        String path = request->url();
        if (path == "/") path = "/index.html";

        // Shorter timeout (2s) — prevents long UI hangs during trim operations
        if (xSemaphoreTake(fsMutex, pdMS_TO_TICKS(2000))) {
            bool gzipped = !LittleFS.exists(path) && LittleFS.exists(path + ".gz");
            String servePath = gzipped ? path + ".gz" : path;

            if (LittleFS.exists(servePath)) {
                String ct = "";
                if (path.endsWith(".html")) ct = "text/html";
                else if (path.endsWith(".js")) ct = "application/javascript";
                else if (path.endsWith(".css")) ct = "text/css";
                else if (path.endsWith(".json")) ct = "application/json";
                else if (path.endsWith(".txt")) ct = "text/plain";

                AsyncWebServerResponse *response = request->beginResponse(LittleFS, servePath, ct);
                if (gzipped) response->addHeader("Content-Encoding", "gzip");

                response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
                response->addHeader("Pragma", "no-cache");
                response->addHeader("Expires", "0");

                request->send(response);
            } else {
                request->send(404, "text/plain", "Not Found");
            }
            xSemaphoreGive(fsMutex);
        } else {
            request->send(503, "text/plain", "FS Busy");
        }
    });

    // OTA upload endpoint
    webServer.on("/ota", HTTP_POST,
                 [](AsyncWebServerRequest *request) {
                     if (!webPasswordHash.isEmpty()) {
                         if (!request->hasHeader("X-OTA-Token") || request->getHeader("X-OTA-Token")->value() != webPasswordHash) {
                             request->send(401, "text/plain", "Unauthorized");
                             return;
                         }
                     }
                     bool ok = !Update.hasError();
                     String msg = ok ? "OK" : String(Update.errorString());
                     AsyncWebServerResponse *resp = request->beginResponse(200, "text/plain", msg);
                     resp->addHeader("Connection", "close");
                     request->send(resp);

                     bool noreboot = request->hasParam("noreboot") && request->getParam("noreboot")->value() == "1";
                     if (ok) {
                         if (noreboot) {
                             char buf[] = "{\"updateStatus\":\"Partial upload OK, continuing...\"}";
                             wsBroadcast(buf, strlen(buf));
                         } else {
                             char buf[] = "{\"updateStatus\":\"Upload successful – rebooting...\"}";
                             wsBroadcast(buf, strlen(buf));
                             rebootTimer = millis() + 2000; rebootRequested = true;
                         }
                     } else {
                         char buf[128];
                         snprintf(buf, sizeof(buf), "{\"updateStatus\":\"Upload failed: %s\"}", Update.errorString());
                         wsBroadcast(buf, strlen(buf));
                     }
                 },
                 [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
                    bool final) {
                     if (!index) {
                         int updateType = U_FLASH;
                         if (request->hasParam("type")) {
                             if (request->getParam("type")->value() == "spiffs") updateType = U_SPIFFS;
                         }
                         logPrintf(LOG_INFO, "Web", "OTA-Upload Start: %s, type: %s", filename.c_str(),
                                       updateType == U_SPIFFS ? "SPIFFS" : "Flash");
                         showStatusDisplayFlashing(updateType == U_SPIFFS ? "Filesystem" : "Firmware");
                         if (!Update.begin(UPDATE_SIZE_UNKNOWN, updateType)) {
                             logPrintf(LOG_ERROR, "Web", "OTA-Upload begin() error: %s", Update.errorString());
                         }
                         char buf[] = "{\"updateStatus\":\"Upload in progress...\"}";
                         wsBroadcast(buf, strlen(buf));
                     }
                     if (len && Update.write(data, len) != len) {
                         logPrintf(LOG_ERROR, "Web", "OTA-Upload write() error: %s", Update.errorString());
                     }
                     if (final) {
                         if (Update.end(true)) {
                             logPrintf(LOG_INFO, "Web", "OTA-Upload Done, %u bytes", index + len);
                         } else {
                             logPrintf(LOG_ERROR, "Web", "OTA-Upload end() error: %s", Update.errorString());
                         }
                     }
                 }
    );

    // Register REST API endpoints for Bridge Server integration
    setupApiEndpoints(webServer);

    webServer.begin();
}
// ── BLE JSON command handler ────────────────────────────────────────────────
// Processes the same JSON commands as the WebSocket handler, but without
// authentication (BLE pairing serves as auth). Called from BLE RX callback.
void processBleJson(const char* data, size_t len) {
    JsonDocument json;
    DeserializationError error = deserializeJson(json, data, len);
    if (error) return;

    if (json["sendMessage"].is<JsonVariant>()) {
        const char* dst = json["sendMessage"]["dst"] | "";
        const char* text = json["sendMessage"]["text"] | "";
        sendMessage(dst, text);
    }

    if (json["sendGroup"].is<JsonVariant>()) {
        const char* dst = json["sendGroup"]["dst"] | "";
        const char* text = json["sendGroup"]["text"] | "";
        sendGroup(dst, text);
    }

    if (json["announce"].is<JsonVariant>()) {
        extern uint32_t announceTimer;
        announceTimer = 0;
    }

    if (json["sendFrame"].is<JsonVariant>()) {
        Frame f;
        if (json["sendFrame"]["frameType"].is<JsonVariant>())
            f.frameType = json["sendFrame"]["frameType"].as<uint8_t>();
        if (json["sendFrame"]["srcCall"].is<JsonVariant>())
            strlcpy(f.srcCall, json["sendFrame"]["srcCall"] | "", sizeof(f.srcCall));
        if (json["sendFrame"]["dstGroup"].is<JsonVariant>())
            strlcpy(f.dstGroup, json["sendFrame"]["dstGroup"] | "", sizeof(f.dstGroup));
        if (json["sendFrame"]["dstCall"].is<JsonVariant>())
            strlcpy(f.dstCall, json["sendFrame"]["dstCall"] | "", sizeof(f.dstCall));
        if (json["sendFrame"]["messageType"].is<JsonVariant>())
            f.messageType = json["sendFrame"]["messageType"].as<uint8_t>();
        if (json["sendFrame"]["messageText"].is<JsonVariant>()) {
            const char *tempText = json["sendFrame"]["messageText"];
            size_t textLen = strlen(tempText);
            if (textLen > sizeof(f.message)) textLen = sizeof(f.message);
            memcpy((char *) f.message, tempText, textLen);
            f.messageLength = textLen;
        }
        sendFrame(f);
    }

    if (json["time"].is<JsonVariant>()) {
        struct timeval tv;
        tv.tv_sec = json["time"].as<time_t>();
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);
    }
}

#endif // HAS_WIFI
