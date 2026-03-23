#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <Update.h>

#include "config.h"
#include "settings.h"
#include "main.h"
#include "wifiFunctions.h"
#include "hal.h"
#include "helperFunctions.h"
#include "peer.h"
#include "routing.h"
#include "auth.h"

AsyncWebServer webServer(80);
AsyncWebSocketMessageHandler wsHandler;
AsyncWebSocket ws("/socket", wsHandler.eventHandler());


// ── Gefilterte Broadcast-Funktion ─────────────────────────────────────────────
// Sendet nur an authentifizierte Clients (oder alle, wenn kein Passwort gesetzt)
void wsBroadcast(const char* buf, size_t len) {
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


void startWebServer() {

  //---------------------- WEBSOCKET -------------------------

  wsHandler.onConnect([](AsyncWebSocket *server, AsyncWebSocketClient *client) {
    ws.cleanupClients();
    setClientAuth(client->id(), false);  // Session anlegen, noch nicht auth.

    if (webPasswordHash.isEmpty()) {
        // Kein Passwort – sofort Daten senden
        sendSettings();
        sendPeerList();
        sendRoutingList();
    } else {
        // Passwort gesetzt – Challenge senden (mit Callsign + Chip-ID zur Anzeige)
        String nonce = generateNonce(client->id());
        uint64_t mac = ESP.getEfuseMac();
        char chipId[13];
        snprintf(chipId, sizeof(chipId), "%02X%02X%02X%02X%02X%02X",
            (uint8_t)(mac >> 40), (uint8_t)(mac >> 32), (uint8_t)(mac >> 24),
            (uint8_t)(mac >> 16), (uint8_t)(mac >> 8), (uint8_t)(mac));
        String challenge = "{\"auth\":{\"required\":true,\"nonce\":\"" + nonce
            + "\",\"mycall\":\"" + String(settings.mycall)
            + "\",\"chipId\":\"" + String(chipId) + "\"}}";
        client->text(challenge);
    }
  });

  wsHandler.onDisconnect([](AsyncWebSocket *server, uint32_t clientId) {
    ws.cleanupClients();
    removeClientAuth(clientId);
  });

  wsHandler.onError([](AsyncWebSocket *server, AsyncWebSocketClient *client, uint16_t errorCode, const char *reason, size_t len) {
    Serial.printf("Client %" PRIu32 " error: %" PRIu16 ": %s\n", client->id(), errorCode, reason);
    ws.cleanupClients();
  });

  //Empfangene Daten auswerten
  wsHandler.onMessage([](AsyncWebSocket *server, AsyncWebSocketClient *client, const uint8_t *data, size_t len) {
    JsonDocument json;
    DeserializationError error = deserializeJson(json, data, len);

    // ── Auth-Handshake ────────────────────────────────────────────────────────
    if (json["auth"].is<JsonVariant>()) {
        if (json["auth"]["response"].is<JsonVariant>()) {
            String response = json["auth"]["response"].as<String>();
            if (verifyAuthResponse(client->id(), response)) {
                setClientAuth(client->id(), true);
                client->text("{\"auth\":{\"ok\":true}}");
                // Init-Daten senden (broadcast zu allen auth. Clients)
                sendSettings();
                sendPeerList();
                sendRoutingList();
            } else {
                // Falsches Passwort – neue Nonce für nächsten Versuch
                String nonce = generateNonce(client->id());
                String msg = "{\"auth\":{\"error\":\"Falsches Passwort\",\"nonce\":\"" + nonce + "\"}}";
                client->text(msg);
            }
        }
        return;  // Auth-Nachrichten benötigen keine weitere Verarbeitung
    }

    // ── Ab hier: nur authentifizierte Clients ────────────────────────────────
    if (!isAuthenticated(client->id())) return;

    //PING (nur zum test)
    if (json["ping"].is<JsonVariant>()) {
      //Serial.println(json["ping"].as<String>());
    }

    // ── Passwort setzen / löschen ─────────────────────────────────────────────
    // {setPassword: "sha256hex"}  → Passwort setzen
    // {setPassword: ""}           → Passwort löschen
    if (json["setPassword"].is<JsonVariant>()) {
        String hash = json["setPassword"].as<String>();
        Serial.printf("[Auth] setPassword empfangen, hash='%s'\n", hash.c_str());
        savePasswordHash(hash);
        setClientAuth(client->id(), true);
        Serial.printf("[Auth] webPasswordHash jetzt: '%s'\n", webPasswordHash.c_str());
        // Direkte Bestätigung an diesen Client (umgeht wsBroadcast-Filterung)
        String resp = hash.isEmpty()
            ? "{\"passwordSaved\":false}"
            : "{\"passwordSaved\":true}";
        Serial.printf("[Auth] Sende an Client %u: %s\n", client->id(), resp.c_str());
        client->text(resp);
        return;
    }

    //Einstellungen speichern
    if (json["settings"].is<JsonVariant>()) {
      if (json["settings"]["mycall"].is<JsonVariant>()) {
        const char* mycallSrc = json["settings"]["mycall"] | "";
        safeUtf8Copy(settings.mycall, (const uint8_t*)mycallSrc, sizeof(settings.mycall));
        for (size_t i = 0; i < sizeof(settings.mycall); i++) {
            settings.mycall[i] = toupper(settings.mycall[i]);
        }
        settings.mycall[sizeof(settings.mycall) - 1] = '\0';
      }
      if (json["settings"]["position"].is<JsonVariant>()) {
        strlcpy(settings.position, json["settings"]["position"] | "", sizeof(settings.position));
      }
      if (json["settings"]["ntp"].is<JsonVariant>()) { strlcpy(settings.ntpServer, json["settings"]["ntp"] | "", sizeof(settings.ntpServer)); }
      if (json["settings"]["dhcpActive"].is<JsonVariant>()) { settings.dhcpActive = json["settings"]["dhcpActive"].as<bool>(); }
      if (json["settings"]["wifiSSID"].is<JsonVariant>()) { strlcpy(settings.wifiSSID, json["settings"]["wifiSSID"] | "", sizeof(settings.wifiSSID)); }
      if (json["settings"]["wifiPassword"].is<JsonVariant>()) { strlcpy(settings.wifiPassword, json["settings"]["wifiPassword"] | "", sizeof(settings.wifiPassword)); }
      if (json["settings"]["apMode"].is<JsonVariant>()) { settings.apMode = json["settings"]["apMode"].as<bool>(); }
      if (json["settings"]["wifiIP"].is<JsonVariant>()) {
        JsonArray ipArray = json["settings"]["wifiIP"];
        for (int i = 0; i < 4; i++) {settings.wifiIP[i] = ipArray[i] | 0; }
      }
      if (json["settings"]["wifiNetMask"].is<JsonVariant>()) {
        JsonArray ipArray = json["settings"]["wifiNetMask"];
        for (int i = 0; i < 4; i++) {settings.wifiNetMask[i] = ipArray[i] | 0; }
      }
      if (json["settings"]["wifiGateway"].is<JsonVariant>()) {
        JsonArray ipArray = json["settings"]["wifiGateway"];
        for (int i = 0; i < 4; i++) {settings.wifiGateway[i] = ipArray[i] | 0; }
      }
      if (json["settings"]["wifiDNS"].is<JsonVariant>()) {
        JsonArray ipArray = json["settings"]["wifiDNS"];
        for (int i = 0; i < 4; i++) {settings.wifiDNS[i] = ipArray[i] | 0; }
      }
      if (json["settings"]["wifiBrodcast"].is<JsonVariant>()) {
        JsonArray ipArray = json["settings"]["wifiBrodcast"];
        for (int i = 0; i < 4; i++) {settings.wifiBrodcast[i] = ipArray[i] | 0; }
      }
        //UDP Peers
        if (json["settings"]["udpPeers"].is<JsonArray>()) {
            JsonArray peers = json["settings"]["udpPeers"];
            udpPeers.clear();
            udpPeerLegacy.clear();
            udpPeerEnabled.clear();
            udpPeerCall.clear();
            for (JsonObject p : peers) {
                JsonVariant v = p["ip"];
                if (v.is<JsonArray>() && v.size() == 4) {
                    JsonArray ip = v.as<JsonArray>();
                    udpPeers.push_back(IPAddress(ip[0]|0, ip[1]|0, ip[2]|0, ip[3]|0));
                    udpPeerLegacy.push_back(p["legacy"] | false);
                    udpPeerEnabled.push_back(p["enabled"] | true);
                    udpPeerCall.push_back(p["call"] | "");
                }
            }
        }
      if (json["settings"]["loraFrequency"].is<JsonVariant>()) {
        settings.loraFrequency = json["settings"]["loraFrequency"].as<float>();
        // TX-Power auf regulatorisches Maximum begrenzen (Public-Band: 27 dBm)
        if (isPublicBand(settings.loraFrequency) && settings.loraOutputPower > PUBLIC_MAX_TX_POWER) {
            settings.loraOutputPower = PUBLIC_MAX_TX_POWER;
        }
      }
      if (json["settings"]["loraOutputPower"].is<JsonVariant>()) {
        settings.loraOutputPower = json["settings"]["loraOutputPower"].as<int8_t>();
        if (isPublicBand(settings.loraFrequency) && settings.loraOutputPower > PUBLIC_MAX_TX_POWER) {
            settings.loraOutputPower = PUBLIC_MAX_TX_POWER;
        }
      }
      if (json["settings"]["loraBandwidth"].is<JsonVariant>()) { settings.loraBandwidth = json["settings"]["loraBandwidth"].as<float>(); }
      if (json["settings"]["loraSyncWord"].is<JsonVariant>()) { settings.loraSyncWord = json["settings"]["loraSyncWord"].as<uint8_t>(); }
      if (json["settings"]["loraCodingRate"].is<JsonVariant>()) { settings.loraCodingRate = json["settings"]["loraCodingRate"].as<uint8_t>(); }
      if (json["settings"]["loraSpreadingFactor"].is<JsonVariant>()) { settings.loraSpreadingFactor = json["settings"]["loraSpreadingFactor"].as<uint8_t>(); }
      if (json["settings"]["loraPreambleLength"].is<JsonVariant>()) { settings.loraPreambleLength = json["settings"]["loraPreambleLength"].as<int16_t>(); }
      if (json["settings"]["loraRepeat"].is<JsonVariant>()) { settings.loraRepeat = json["settings"]["loraRepeat"].as<bool>(); }
      if (json["settings"]["maxHopMessage"].is<JsonVariant>()) { extSettings.maxHopMessage = json["settings"]["maxHopMessage"].as<uint8_t>(); }
      if (json["settings"]["maxHopPosition"].is<JsonVariant>()) { extSettings.maxHopPosition = json["settings"]["maxHopPosition"].as<uint8_t>(); }
      if (json["settings"]["maxHopTelemetry"].is<JsonVariant>()) { extSettings.maxHopTelemetry = json["settings"]["maxHopTelemetry"].as<uint8_t>(); }
      if (json["settings"]["updateChannel"].is<JsonVariant>()) { updateChannel = json["settings"]["updateChannel"].as<uint8_t>(); }
      if (json["settings"]["loraEnabled"].is<JsonVariant>()) { loraEnabled = json["settings"]["loraEnabled"].as<bool>(); }
      saveSettings();
    }

    //Frame senden RAW
    if (json["sendFrame"].is<JsonVariant>()) {
        Frame f;
        if (json["sendFrame"]["frameType"].is<JsonVariant>()) {f.frameType = json["sendFrame"]["frameType"].as<uint8_t>();}
        if (json["sendFrame"]["srcCall"].is<JsonVariant>()) { strlcpy(f.srcCall, json["sendFrame"]["srcCall"] | "", sizeof(f.srcCall)); }
        if (json["sendFrame"]["dstGroup"].is<JsonVariant>()) { strlcpy(f.dstGroup, json["sendFrame"]["dstCall"] | "", sizeof(f.dstGroup)); }
        if (json["sendFrame"]["dstCall"].is<JsonVariant>()) { strlcpy(f.dstCall, json["sendFrame"]["dstCall"] | "", sizeof(f.dstCall)); }
        if (json["sendFrame"]["messageType"].is<JsonVariant>()) {f.messageType = json["sendFrame"]["messageType"].as<uint8_t>();}
        if (json["sendFrame"]["messageLength"].is<JsonVariant>()) {f.messageLength = json["sendFrame"]["messageLength"].as<uint16_t>();}
        if (json["sendFrame"]["messageText"].is<JsonVariant>()) {
            const char* tempText = json["sendFrame"]["messageText"];
            memcpy((char*)f.message, tempText, sizeof(f.message));
            f.messageLength = strlen(tempText);
        }
        if (json["sendFrame"]["message"].is<JsonArray>()) {
            JsonArray jsonMsg = json["sendFrame"]["message"].as<JsonArray>();
            uint8_t i = 0;
            for (uint8_t v : jsonMsg) {
                if (i < len && i < sizeof(f.message)) {
                    f.message[i] = v;
                    i++;
                }
            }
        }
        sendFrame(f);
    }

    //Nachricht senden
    if (json["sendMessage"].is<JsonVariant>()) {
        sendMessage(json["sendMessage"]["dst"].as<const char*>(), json["sendMessage"]["text"].as<const char*>() );
    }

    //Gruppe senden
    if (json["sendGroup"].is<JsonVariant>()) {
        sendGroup(json["sendGroup"]["dst"].as<const char*>(), json["sendGroup"]["text"].as<const char*>() );
    }

    //Messages.json löschen
    if (json["deleteMessages"].is<JsonVariant>()) {
        LittleFS.remove("/messages.json");
        rebootTimer = millis() + 1000;
    }

    //Trace senden
    if (json["trace"].is<JsonVariant>()) {
        char text[128];
        getFormattedTime("%H:%M:%S", text, sizeof(text));
        sendMessage(json["trace"]["dstCall"].as<const char*>(), text, Frame::MessageTypes::TRACE_MESSAGE);
    }

    //Uhrzeit Sync
    if (json["time"].is<JsonVariant>()) {
      struct timeval tv;
      tv.tv_sec = json["time"].as<time_t>();
      tv.tv_usec = 0;
      settimeofday(&tv, NULL);
    }

    //WiFi Scannen
    if (json["scanWifi"].is<JsonVariant>()) {
      Serial.println("WiFi Scan...");
      WiFi.scanNetworks(true);
    }

    //Announce
    if (json["announce"].is<JsonVariant>()) {
      Serial.println("Send manual announce...");
      announceTimer = 0;
    }

    //Tune
    if (json["tune"].is<JsonVariant>()) {
      Serial.println("Send tune...");
      Frame f;
      f.frameType = Frame::FrameTypes::TUNE_FRAME;
      txBuffer.push_back(f);
    }

    //Reboot
    if (json["reboot"].is<JsonVariant>()) {
      Serial.println("Reboot");
      rebootTimer = 0;
    }

    //Shutdown
    if (json["shutdown"].is<JsonVariant>()) {
      Serial.println("Shutdown");
      pendingShutdown = true;
    }

    //OTA Update (deferred – Aufruf im Main-Loop, nicht im async_tcp-Task)
    if (json["update"].is<JsonVariant>()) {
      Serial.println("OTA Update angefordert...");
      pendingManualUpdate = true;
    }

    //OTA Force-Install (deferred – Aufruf im Main-Loop, nicht im async_tcp-Task)
    if (json["forceUpdate"].is<JsonVariant>()) {
      pendingForceChannel = json["forceUpdate"].as<uint8_t>(); // 0=release, 1=dev
      pendingForceUpdate = true;
      Serial.printf("Force-Install angefordert (Kanal: %s)...\n", pendingForceChannel == 1 ? "dev" : "release");
    }

  });


  //Websocket -> Webserver
  webServer.addHandler(&ws);

  //---------------------- WEBSERVER -------------------------
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

    webServer.onNotFound([](AsyncWebServerRequest *request) {
        String path = request->url();
        if (path == "/") path = "/index.html";

        if (xSemaphoreTake(fsMutex, pdMS_TO_TICKS(10000))) {
            bool gzipped = !LittleFS.exists(path) && LittleFS.exists(path + ".gz");
            String servePath = gzipped ? path + ".gz" : path;

            if (LittleFS.exists(servePath)) {
                // Content-Type anhand des Original-Pfades bestimmen (ohne .gz)
                String ct = "";
                if      (path.endsWith(".html")) ct = "text/html";
                else if (path.endsWith(".js"))   ct = "application/javascript";
                else if (path.endsWith(".css"))  ct = "text/css";
                else if (path.endsWith(".json")) ct = "application/json";
                else if (path.endsWith(".txt"))  ct = "text/plain";

                AsyncWebServerResponse *response = request->beginResponse(LittleFS, servePath, ct);
                if (gzipped) response->addHeader("Content-Encoding", "gzip");

                // Kein Caching für alle Dateien (verhindert veraltete JS/HTML im Browser)
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

  // ── OTA Upload ──────────────────────────────────────────────────────────────
  webServer.on("/ota", HTTP_POST,
    // Antwort nach abgeschlossenem Upload
    [](AsyncWebServerRequest *request) {
        bool ok = !Update.hasError();
        String msg = ok ? "OK" : String(Update.errorString());
        AsyncWebServerResponse *resp = request->beginResponse(200, "text/plain", msg);
        resp->addHeader("Connection", "close");
        request->send(resp);
        bool noreboot = request->hasParam("noreboot") && request->getParam("noreboot")->value() == "1";
        if (ok) {
            if (noreboot) {
                char buf[] = "{\"updateStatus\":\"Teil-Upload OK, weiter...\"}";
                wsBroadcast(buf, strlen(buf));
            } else {
                char buf[] = "{\"updateStatus\":\"Upload erfolgreich – Neustart...\"}";
                wsBroadcast(buf, strlen(buf));
                rebootTimer = millis() + 2000;
            }
        } else {
            char buf[128];
            snprintf(buf, sizeof(buf), "{\"updateStatus\":\"Upload fehlgeschlagen: %s\"}", Update.errorString());
            wsBroadcast(buf, strlen(buf));
        }
    },
    // Upload-Handler (chunk-weise)
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (!index) {
            int updateType = U_FLASH;
            if (request->hasParam("type")) {
                if (request->getParam("type")->value() == "spiffs") updateType = U_SPIFFS;
            }
            Serial.printf("[OTA-Upload] Start: %s, Typ: %s\n", filename.c_str(), updateType == U_SPIFFS ? "SPIFFS" : "Flash");
            if (!Update.begin(UPDATE_SIZE_UNKNOWN, updateType)) {
                Serial.printf("[OTA-Upload] begin() Fehler: %s\n", Update.errorString());
            }
            char buf[] = "{\"updateStatus\":\"Upload läuft...\"}";
            wsBroadcast(buf, strlen(buf));
        }
        if (len && Update.write(data, len) != len) {
            Serial.printf("[OTA-Upload] write() Fehler: %s\n", Update.errorString());
        }
        if (final) {
            if (Update.end(true)) {
                Serial.printf("[OTA-Upload] Fertig, %u Bytes\n", index + len);
            } else {
                Serial.printf("[OTA-Upload] end() Fehler: %s\n", Update.errorString());
            }
        }
    }
  );

  webServer.begin();
}
