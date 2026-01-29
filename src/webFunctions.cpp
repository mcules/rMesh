#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <LittleFS.h>

#include "config.h"
#include "settings.h"
#include "main.h"
#include "wifiFunctions.h"
#include "hal.h"
#include "helperFunctions.h"
#include "peer.h"

AsyncWebServer webServer(80);
AsyncWebSocketMessageHandler wsHandler;
AsyncWebSocket ws("/socket", wsHandler.eventHandler());



void startWebServer() {

  //---------------------- WEBSOCKET -------------------------

  wsHandler.onConnect([](AsyncWebSocket *server, AsyncWebSocketClient *client) {
    //Serial.printf("Client %" PRIu32 " connected\n", client->id());
    ws.cleanupClients();
    sendSettings();
    sendPeerList();
  });

  wsHandler.onDisconnect([](AsyncWebSocket *server, uint32_t clientId) {
    //Serial.printf("Client %" PRIu32 " disconnected\n", clientId);
    ws.cleanupClients();
  });

  wsHandler.onError([](AsyncWebSocket *server, AsyncWebSocketClient *client, uint16_t errorCode, const char *reason, size_t len) {
    Serial.printf("Client %" PRIu32 " error: %" PRIu16 ": %s\n", client->id(), errorCode, reason);
    ws.cleanupClients();
  });

  //Empfangene Daten auswerten
  wsHandler.onMessage([](AsyncWebSocket *server, AsyncWebSocketClient *client, const uint8_t *data, size_t len) {
    //JSON 
    JsonDocument json;
    DeserializationError error = deserializeJson(json, data, len);

    //PING (nur zum test)
    if (json["ping"].is<JsonVariant>()) {
      //Serial.println(json["ping"].as<String>());
    }

    //Einstellungen speichern
    if (json["settings"].is<JsonVariant>()) {
      if (json["settings"]["mycall"].is<JsonVariant>()) { 
        strlcpy(settings.mycall, json["settings"]["mycall"] | "", sizeof(settings.mycall));
        for (size_t i = 0; i < sizeof(settings.mycall); i++) {
            settings.mycall[i] = toupper(settings.mycall[i]);
        }
        settings.mycall[MAX_CALLSIGN_LENGTH] = '\0';
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
      if (json["settings"]["loraFrequency"].is<JsonVariant>()) { settings.loraFrequency = json["settings"]["loraFrequency"].as<float>(); }
      if (json["settings"]["loraOutputPower"].is<JsonVariant>()) { settings.loraOutputPower = json["settings"]["loraOutputPower"].as<int8_t>(); }
      if (json["settings"]["loraBandwidth"].is<JsonVariant>()) { settings.loraBandwidth = json["settings"]["loraBandwidth"].as<float>(); }
      if (json["settings"]["loraSyncWord"].is<JsonVariant>()) { settings.loraSyncWord = json["settings"]["loraSyncWord"].as<uint8_t>(); }
      if (json["settings"]["loraCodingRate"].is<JsonVariant>()) { settings.loraCodingRate = json["settings"]["loraCodingRate"].as<uint8_t>(); }
      if (json["settings"]["loraSpreadingFactor"].is<JsonVariant>()) { settings.loraSpreadingFactor = json["settings"]["loraSpreadingFactor"].as<uint8_t>(); }
      if (json["settings"]["loraPreambleLength"].is<JsonVariant>()) { settings.loraPreambleLength = json["settings"]["loraPreambleLength"].as<int16_t>(); }
      if (json["settings"]["loraRepeat"].is<JsonVariant>()) { settings.loraRepeat = json["settings"]["loraRepeat"].as<bool>(); }
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
                // Sicherstellen, dass wir niemals über das Ende von f.message (256 Bytes) schreiben
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
      //Frame in SendeBuffer
      txBuffer.push_back(f);
    }     

    //Reboot
    if (json["reboot"].is<JsonVariant>()) {
      Serial.println("Reboot");
      rebootTimer = 0;
    }    


  });
  

  //Websocket -> Webserver
  webServer.addHandler(&ws);

  //---------------------- WEBSERVER -------------------------
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

  //Redirect für Index-Seite
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("/index.html");
  }); 

  //Statische Webseite aus Filesystem  + Starten
  webServer.serveStatic("/", LittleFS, "/");
  webServer.begin(); 
}


