#include <Arduino.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include <vector>
#include <nvs_flash.h>
#include <freertos/semphr.h>

#include "config.h"
#include "hal.h"
#include "frame.h"
#include "settings.h"
#include "main.h"
#include "wifiFunctions.h"
#include "webFunctions.h"
#include "serial.h"
#include "webFunctions.h"
#include "helperFunctions.h"
#include "peer.h"
#include "ack.h"
#include "udp.h"
#include "routing.h"
#include "reporting.h"
#include "time.h"

#ifdef LILYGO_T_LORA_PAGER
#include "display_LILYGO_T-LoraPager.h"
#include "hal_LILYGO_T-LoraPager.h"
#endif

#ifdef SEEED_SENSECAP_INDICATOR
#include "display_SEEED_SenseCAP_Indicator.h"
#include "hal_SEEED_SenseCAP_Indicator.h"
#endif


//Uhrzeitformat
const char* TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3";

//Sendepuffer
std::vector<Frame> txBuffer;

//Speicher für die letzten Message IDs
MSG messages[MAX_STORED_MESSAGES_RAM];
uint16_t messagesHead = 0;

//Mutex Dateisystem
SemaphoreHandle_t fsMutex = NULL;

//Timing
uint32_t announceTimer = 5000;      //Erstes Announce nach 5 Sekunden
uint32_t statusTimer = 0;
uint32_t rebootTimer = 0xFFFFFFFF;
uint8_t currentRetry = 0;
bool pendingManualUpdate = false;
bool pendingForceUpdate = false;
uint8_t pendingForceChannel = 0;
uint32_t updateCheckTimer = 60 * 60 * 1000;  //Erster Check nach 1 Stunde
uint32_t messagesDeleteTimer = 30 * 60 * 1000;  //Erster Check nach 30 Min
uint32_t reportingTimer = 5 * 60 * 1000;         //Erster Report nach 5 Minuten

void processRxFrame(Frame &f) {
    //Abbruch, wenn kein nodeCall
    if (strlen(f.nodeCall) == 0) {return;}

    //Monitor
    f.monitorJSON();

    //Peer List
    addPeerList(f);

    //Auswerten
    Frame tf;                   //ggf. Antwort-Frame
    bool found = false;         //z.b.V.
    File file;                  //z.b.V
    switch (f.frameType) {
        //Antwort auf announce
        case Frame::FrameTypes::ANNOUNCE_FRAME:
            if (strlen(f.nodeCall) > 0 ){
                // WiFi bevorzugen: LoRa-ACK unterdrücken wenn Peer bereits als WiFi-Peer bekannt
                if (f.port == 0) {
                    bool peerOnWifi = false;
                    for (size_t pi = 0; pi < peerList.size(); pi++) {
                        if (strcmp(f.nodeCall, peerList[pi].nodeCall) == 0 && peerList[pi].port == 1) {
                            peerOnWifi = true; break;
                        }
                    }
                    if (peerOnWifi) break;
                }
                tf.frameType = Frame::FrameTypes::ANNOUNCE_ACK_FRAME;
                tf.port = f.port;
                switch (tf.port){
                    case 0: tf.transmitMillis = millis() + calculateAckTime(); break;  //Time On Air für Antwort
                    case 1: tf.transmitMillis = millis(); break; //Bei UDP schneller
                }
                memcpy(tf.viaCall, f.nodeCall, sizeof(tf.viaCall));
                txBuffer.push_back(tf);
            }
            break;
        //In Peer Liste eintragen
        case Frame::FrameTypes::ANNOUNCE_ACK_FRAME:
            if (strcmp(f.viaCall, settings.mycall) == 0) {
                availablePeerList(f.nodeCall, true, f.port);   
                addRoutingList(f.nodeCall, f.nodeCall, f.hopCount); 
            }
            break;

        //Senden abbrechen
        case Frame::FrameTypes::MESSAGE_ACK_FRAME:
            //In Peer Liste eintragen
            if (strcmp(f.viaCall, settings.mycall) == 0) {
                availablePeerList(f.nodeCall, true, f.port);
                addRoutingList(f.nodeCall, f.nodeCall, f.hopCount); 
                //Wenn ich ein ACK direkt bekommen habe, dann extra Eintrag
                addACK(f.srcCall, settings.mycall, f.id);    
            }

            //Im TX-Puffer nach MSG-ID und NODE-Call suchen und löschen
            txBuffer.erase(
                std::remove_if(txBuffer.begin(), txBuffer.end(),
                    [&](const Frame& txB) {
                        return (strcmp(txB.viaCall, f.nodeCall) == 0) && (txB.id == f.id);
                    }),
                txBuffer.end()
            );

            //ACKs in Datei speichern (für REPEAT und ACK für fremde Frames senden)
            addACK(f.srcCall, f.nodeCall, f.id);
            break;

        //Nachricht empfangen
        case Frame::FrameTypes::MESSAGE_FRAME:  
            //In Peer Liste eintragen 
            if (strcmp(f.viaCall, settings.mycall) == 0) {
                availablePeerList(f.nodeCall, true, f.port);    
            }

            //Wenn die Nachricht ein anderes Node gesendet hat und wir die Nachricht auch senden wollen: Im TX-Puffer nach MSG-ID und VIA-Call suchen und löschen
            txBuffer.erase(
                std::remove_if(txBuffer.begin(), txBuffer.end(),
                    [&](const Frame& txB) {
                        return (strcmp(txB.srcCall, f.srcCall) == 0) && (strcmp(txB.viaCall, f.viaCall) == 0) && (txB.id == f.id) && (txB.initRetry == TX_RETRY);
                    }),
                txBuffer.end()
            );   

            //Aus dem TX-Puffer löschen, wenn man merkt, dass ein anderes Node den Frame schon wiederholt.
            txBuffer.erase(
                std::remove_if(txBuffer.begin(), txBuffer.end(),
                    [&](const Frame& txB) {
                        return (strcmp(txB.srcCall, f.srcCall) == 0) && (strcmp(txB.viaCall, f.nodeCall) == 0) && (txB.id == f.id) && (txB.initRetry == TX_RETRY);
                    }),
                txBuffer.end()
            ); 

            //Alle "alten" ACKs im TX-Puffer löschen
            txBuffer.erase(
                std::remove_if(txBuffer.begin(), txBuffer.end(),
                    [&](const Frame& txB) {
                        return (strcmp(txB.srcCall, f.srcCall) == 0) && (txB.id == f.id) && (txB.frameType == Frame::FrameTypes::MESSAGE_ACK_FRAME);
                    }),
                txBuffer.end()
            );

            //ACK-Senden
            bool sendACK = false;
            //ACK Senden, wenn ich direkt angesprochen wurde
            if (strcmp(f.viaCall, settings.mycall) == 0) {sendACK = true;}
            //ACK Senden, wenn ich nicht direkt angesprochen wurde, aber nur 1x
            if ((strlen(f.viaCall) > 0) && (checkACK(f.srcCall, f.nodeCall, f.id) == false) && (checkACK(f.srcCall, settings.mycall, f.id) == false)) {sendACK = true;}

            if (sendACK) {
                addACK(f.srcCall, f.nodeCall, f.id);
                tf.frameType = Frame::FrameTypes::MESSAGE_ACK_FRAME;
                memcpy(tf.viaCall, f.nodeCall, sizeof(tf.viaCall));
                memcpy(tf.srcCall, f.nodeCall, sizeof(tf.srcCall));
                tf.id = f.id;
                // ACK auf dem Weg senden, auf dem der Peer erreichbar ist
                bool nodeOnWifi = false;
                for (size_t pi = 0; pi < peerList.size(); pi++) {
                    if (strcmp(f.nodeCall, peerList[pi].nodeCall) == 0 && peerList[pi].port == 1) {
                        nodeOnWifi = true;
                        break;
                    }
                }
                if (nodeOnWifi) {
                    tf.port = 1;
                    tf.transmitMillis = 0;
                    txBuffer.push_back(tf);
                } else {
                    tf.port = 0;
                    tf.transmitMillis = millis() + calculateAckTime();
                    txBuffer.push_back(tf);
                }
            }

            //Message ID und SRC-Call in Messages Ringpuffer suchen
            for (int i = 0; i < MAX_STORED_MESSAGES_RAM; i++) {
                if (messages[i].id == f.id) {
                    if (strcmp(messages[i].srcCall, f.srcCall) == 0) {
                        found = true;
                        break;
                    }
                }
            }

            //Routing
            addRoutingList(f.srcCall, f.nodeCall, f.hopCount); 

            if ((found == false) && (f.messageLength > 0)) {
                //Neue Nachricht empfangen 
                
                //Message in Ringpuffer speichern
                strncpy(messages[messagesHead].srcCall, f.srcCall, MAX_CALLSIGN_LENGTH + 1);
                messages[messagesHead].id = f.id;
                messagesHead++;
                if (messagesHead >= MAX_STORED_MESSAGES_RAM) { messagesHead = 0; }                        

                //Message an Websocket senden & speichern
                char* jsonBuffer = (char*)malloc(4096);
                size_t len = f.messageJSON(jsonBuffer, 4096);
                ws.textAll(jsonBuffer, len);
                addJSONtoFile(jsonBuffer, len, "/messages.json", MAX_STORED_MESSAGES);
                #ifdef LILYGO_T_LORA_PAGER
                // Archive to SD card without size limit when a card is inserted
                pagerAddMessageToSD(jsonBuffer, len);
                #endif
                free(jsonBuffer);
                jsonBuffer = nullptr;

                // Display on T-LoraPager screen
                #ifdef LILYGO_T_LORA_PAGER
                if (f.messageType == Frame::MessageTypes::TEXT_MESSAGE) {
                    char textBuf[261] = {0};
                    memcpy(textBuf, f.message, f.messageLength);
                    displayOnNewMessage(f.srcCall, textBuf, f.dstGroup, f.dstCall);
                }
                displayMonitorFrame(f);
                #endif

                // Display on SenseCAP Indicator screen
                #ifdef SEEED_SENSECAP_INDICATOR
                if (f.messageType == Frame::MessageTypes::TEXT_MESSAGE) {
                    char textBuf[261] = {0};
                    memcpy(textBuf, f.message, f.messageLength);
                    displayOnNewMessage(f.srcCall, textBuf, f.dstGroup, f.dstCall);
                }
                displayMonitorFrame(f);
                #endif

                //ECHO für Tracking-Message
                if ((strcmp(f.dstCall, settings.mycall) == 0) && (f.messageType == Frame::MessageTypes::TRACE_MESSAGE) && (strstr((char*)f.message, "ECHO") == NULL)) {
                        char message[512];
                        size_t messageLength = f.messageLength;
                        memcpy(message, f.message, f.messageLength);
                        memcpy(&message[messageLength], " -> ECHO ", 9);
                        messageLength += 9;
                        memcpy(&message[messageLength], settings.mycall, strlen(settings.mycall));
                        messageLength += strlen(settings.mycall);
                        memcpy(&message[messageLength], " ", 1);
                        messageLength += 1;
                        char text[128];
                        getFormattedTime("%H:%M:%S", text, sizeof(text));                            
                        memcpy(&message[messageLength], text, strlen(text));
                        messageLength += strlen(text);
                        if (messageLength > 255) {messageLength = 255;}
                        sendMessage(f.srcCall, message, Frame::MessageTypes::TRACE_MESSAGE);
                }

                //Fernsteuerung
                if ((strcmp(f.dstCall, settings.mycall) == 0) && (f.messageType == Frame::MessageTypes::COMMAND_MESSAGE) ) {
                    switch (f.message[0]) {
                        case 0xff: //Firmware
                            sendMessage(f.srcCall, NAME " " VERSION " " PIO_ENV_NAME);
                            break;
                        case 0xfe: //Reboot
                            rebootTimer = millis() + 2500;
                            break;
                    }
                }

                //Messages wiederholen  (Hopcount OK und Ziel <> Ich)
                if ((settings.loraRepeat == true) && (f.hopCount < extSettings.maxHopMessage) && (strcmp(f.dstCall, settings.mycall) != 0)) {
                    //Frame vorbereiten
                    tf.frameType = f.frameType;
                    memcpy(tf.srcCall, f.srcCall, sizeof(tf.srcCall));
                    memcpy(tf.dstGroup, f.dstGroup, sizeof(tf.dstGroup));
                    memcpy(tf.dstCall, f.dstCall, sizeof(tf.dstCall));
                    tf.hopCount = f.hopCount;
                    if (tf.hopCount < 15) {tf.hopCount ++;}
                    tf.messageType = f.messageType;                        
                    memcpy(tf.message, f.message, sizeof(tf.message));
                    tf.messageLength = f.messageLength;
                    tf.id = f.id;
                    tf.timestamp = time(NULL);
                    tf.syncFlag = false;

                    //Nach Route suchen
                    bool routing = false;
                    char viaCall[MAX_CALLSIGN_LENGTH + 1];
                    getRoute(f.dstCall, viaCall, MAX_CALLSIGN_LENGTH + 1);
                    if (strlen(viaCall) > 0) { routing = true; }

                    // WiFi bevorzugen: LoRa überspringen wenn Routing-Ziel per WiFi erreichbar
                    bool routeViaWifi = false;
                    if (routing) {
                        for (size_t pi = 0; pi < peerList.size(); pi++) {
                            if (strcmp(peerList[pi].nodeCall, viaCall) == 0 &&
                                peerList[pi].port == 1 && peerList[pi].available) {
                                routeViaWifi = true; break;
                            }
                        }
                    }

                    //Ports durchlaufen – WiFi zuerst, dann LoRa
                    for (tf.port = 1; tf.port >= 0; tf.port--) {
                        if (tf.port == 0 && routeViaWifi) continue;

                        switch (tf.port){
                            case 0: tf.transmitMillis = millis() + calculateRetryTime(); break;  //Time On Air für Antwort
                            case 1: tf.transmitMillis = millis() + UDP_TX_RETRY_TIME; break; //Bei UDP schneller
                        }

                        //Prüfen, ob Tracking ein
                        if (f.messageType == Frame::MessageTypes::TRACE_MESSAGE) {
                            //EIN -> Rufzeichen und Uhrzeit dazu
                            memcpy(&tf.message[tf.messageLength], " -> ", 4);
                            tf.messageLength += 4;
                            memcpy(&tf.message[tf.messageLength], settings.mycall, strlen(settings.mycall));
                            tf.messageLength += strlen(settings.mycall);
                            memcpy(&tf.message[tf.messageLength], " ", 1);
                            tf.messageLength += 1;
                            char text[128];
                            getFormattedTime("%H:%M:%S", text, sizeof(text));                            
                            memcpy(&tf.message[tf.messageLength], text, strlen(text));
                            tf.messageLength += strlen(text);
                            if (tf.messageLength > 255) {tf.messageLength = 255;}
                        } 

                        //Prüfen, an wen man das Frame so senden könnte
                        for (int i = 0; i < peerList.size(); i++) {
                            //Prüfen, ob das Peer das Frame schon mal wiederholt hat (in ACK-Liste)
                            found = checkACK(f.srcCall, peerList[i].nodeCall, f.id);

                            //In TX-Puffer eintragen: NICHT an nodeCall und nicht an srcCall
                            if ((found == false) && (peerList[i].available == true) && (peerList[i].port == tf.port) && (strcmp(peerList[i].nodeCall, f.nodeCall) != 0) && (strcmp(peerList[i].nodeCall, f.srcCall) != 0)) {
                                if ((routing == false) || ( strcmp(peerList[i].nodeCall, viaCall) == 0)) {
                                //if ((strlen(f.dstCall) == 0) || (checkRoute(f.dstCall, peerList[i].nodeCall))) {
                                    //Frame in TX-Puffer
                                    memcpy(tf.viaCall, peerList[i].nodeCall, sizeof(tf.viaCall));
                                    tf.retry = TX_RETRY;
                                    tf.initRetry = TX_RETRY;
                                    txBuffer.push_back(tf);
                                    //In ACK-Liste eintagen, damit später kein ACK gesendet wird, wenn das Peer die MSG wiederholt
                                    addACK(tf.srcCall, tf.viaCall, tf.id);                                
                                }
                            }
                        } 
                    }
                }

            }

            break;
    }
 }


void setup() {
    //UART
    Serial.begin(115200);
    Serial.setDebugOutput(true);

    #if defined(LILYGO_T_LORA_PAGER)
    // USB-CDC needs ~1s to enumerate; early output would be lost
    delay(2000);
    Serial.println("=== rMesh T-LoraPager boot ===");
    Serial.printf("PSRAM: %s (%u bytes)\n", psramFound() ? "OK" : "NOT FOUND", ESP.getPsramSize());
    Serial.printf("Free heap: %u\n", ESP.getFreeHeap());
    Serial.flush();
    #elif defined(SEEED_SENSECAP_INDICATOR)
    // UART0 via CH340 bridge — ready immediately, no wait needed
    delay(100);
    #else
    while (!Serial) {}
    #endif

    //CPU Frqg fest (soll wegen SPI sinnvoll sein)
    setCpuFrequencyMhz(240);
    esp_log_level_set("NetworkUdp", ESP_LOG_NONE);
    esp_log_level_set("NetworkUdp", ESP_LOG_ERROR);
    esp_log_level_set("vfs", ESP_LOG_WARN);
    esp_log_level_set("vfs", ESP_LOG_NONE);

    //Puffer
    peerList.reserve(PEER_LIST_SIZE);
    txBuffer.reserve(TX_BUFFER_SIZE);     
    routingList.reserve(ROUTING_BUFFER_SIZE);     

    //Einstellungen laden
    loadSettings();

    //Initialize LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("An error has occurred while mounting LittleFS");
    } 
    fsMutex = xSemaphoreCreateMutex();
    
    //Messages JSON in messages Ringpuffer speichern
    File file = LittleFS.open("/messages.json", "r");
    if (file) {
        JsonDocument doc;
        while (file.available()) {
            DeserializationError error = deserializeJson(doc, file);
            if (error == DeserializationError::Ok) {
                const char* tempSrc = doc["message"]["srcCall"] | "";
                uint32_t tempId = doc["message"]["id"] | 0;
                //Serial.printf("id: %d, src: %s, head:%d\n", tempId, tempSrc, messagesHead);
                //In messages speichern
                strncpy(messages[messagesHead].srcCall, tempSrc, MAX_CALLSIGN_LENGTH);
                messages[messagesHead].id = doc["message"]["id"].as<uint32_t>();
                messagesHead++;
                if (messagesHead >= MAX_STORED_MESSAGES_RAM) { messagesHead = 0; }                        
            } else if (error != DeserializationError::EmptyInput) {
                file.readStringUntil('\n');
            }
        }
        file.close();                    
    }

    //Init Hardware
    initHal();

    //WiFI Init
    wifiInit();

    //Zeit setzzen
    struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    configTzTime(TZ_INFO, settings.ntpServer);

    //WEB-Server starten
    startWebServer();

    //Init OK
    Serial.printf("\n\n\n%s\n%s %s\nREADY.\n", PIO_ENV_NAME, NAME, VERSION);  
   
}


void loop() {
    //UART
    checkSerialRX();

    //Wifi
    showWiFiStatus();

    // Display polling
    #ifdef LILYGO_T_LORA_PAGER
    displayUpdateLoop();
    #endif
    #ifdef SEEED_SENSECAP_INDICATOR
    displayUpdateLoop();
    #endif

	//Announce Senden
	if (millis() > announceTimer) {
		announceTimer = millis() + ANNOUNCE_TIME;
		Frame f;
		f.frameType = Frame::FrameTypes::ANNOUNCE_FRAME;
		f.transmitMillis = 0;
		//Frame in SendeBuffer – WiFi zuerst, dann LoRa
        f.port = 1;
		txBuffer.push_back(f);
        f.port = 0;
		txBuffer.push_back(f);
        // WiFi-Peers auf "nicht verfügbar" setzen – ANNOUNCE_ACK setzt sie wieder auf true
        for (auto& peer : peerList) {
            if (peer.port == 1) peer.available = false;
        }
        sendPeerList();
	}  

    //Prüfen, ob was gesendet werden muss
	if ((txFlag == false) && (rxFlag == false)) {

	    //Frames mit retry > 1 werden synchron gesendet !!!
        bool sendNewSyncFrame = true;
        for (int i = 0; i < txBuffer.size(); i++) {
            //Prüfen, ob es Frames gibt, die noch nicht synchron gesendet wurden
            if (txBuffer[i].syncFlag == true) {sendNewSyncFrame = false;}
        }

        //Im Puffer nach synchronen Frames duchen und den 1. gefundenen (pro Port) zum Senden freigeben
        if (sendNewSyncFrame == true) {
            for (int port = 0; port <= 1; port++) {
                for (int i = 0; i < txBuffer.size(); i++) {
                    if ((txBuffer[i].retry > 1) && (txBuffer[i].port == port)) {
                        txBuffer[i].syncFlag = true; 
                        switch (txBuffer[i].port){
                            case 0: txBuffer[i].transmitMillis = millis() + calculateRetryTime(); break;  //Time On Air für Antwort
                            case 1: txBuffer[i].transmitMillis = millis() + UDP_TX_RETRY_TIME; break; //Bei UDP schneller
                        }
                        break;   
                    }
                }
            }
        }
        
        //Sendepuffer duchlaufen und ggg. Frames senden
        if (txBuffer.size() == 0) {currentRetry = 0;}
    	for (int i = 0; i < txBuffer.size(); i++) {
    		//Prüfen, ob Frame gesendet werden muss
    		if ((millis() > txBuffer[i].transmitMillis) && ((txBuffer[i].retry <= 1) || (txBuffer[i].syncFlag == true))) {
    			//Frame senden
                switch (txBuffer[i].port){
                    case 0: transmitFrame(txBuffer[i]); break;
                    case 1: sendUDP(txBuffer[i]); break;
                }
                //Retrys runterzählen
                if (txBuffer[i].retry > 0) {txBuffer[i].retry --;}
                currentRetry = txBuffer[i].initRetry - txBuffer[i].retry;
                //Nächsten Sendezeitpunkt festlegen (nur relevant, wenn retry > 1)
                switch (txBuffer[i].port){
                    case 0: txBuffer[i].transmitMillis = millis() + calculateRetryTime(); break;; //Time On Air für Antwort
                    case 1: txBuffer[i].transmitMillis = millis() + UDP_TX_RETRY_TIME; break; //Bei UDP schneller
                }
                //Wenn kein Retry mehr übrig, dann löschen
                if (txBuffer[i].retry == 0) {  
                    //Aus Peer-Liste löschen
                    if (txBuffer[i].initRetry > 1) {
                        availablePeerList(txBuffer[i].viaCall, false, txBuffer[i].port);
                    }
                    //Frame löschen
                    txBuffer.erase(txBuffer.begin() + i);
                }
                break;
    		}    
    	}
    }

    //Prüfen, ob was empfangen wurde
    Frame f;
    if (checkReceive(f)) { processRxFrame(f); }

    //Prüfen, ob was über UDP empfangen wurde
    if (checkUDP(f)) { processRxFrame(f); }

    //Status über Websocket senden
    if (millis() > statusTimer) {
        statusTimer = millis() + 1000;
        //Status über Websocket senden
        JsonDocument doc;
        doc["status"]["time"] = time(NULL);
        doc["status"]["tx"] = txFlag;
        doc["status"]["rx"] = rxFlag;
        doc["status"]["txBufferCount"] = txBuffer.size();
        doc["status"]["retry"] = currentRetry;
        doc["status"]["heap"] = ESP.getFreeHeap();
        char* jsonBuffer = (char*)malloc(1024);
        size_t len = serializeJson(doc, jsonBuffer, 1024);
        ws.textAll(jsonBuffer, len);  // sendet direkt den Puffer
        free(jsonBuffer);
        jsonBuffer = nullptr;
    	//Peer-Liste checken
    	checkPeerList();
    }

    //Reboot
    if (millis() > rebootTimer) {ESP.restart();}

    //Update Check
    if (millis() > updateCheckTimer) {
        updateCheckTimer = millis() + 24 * 60 * 60 * 1000; //24 Stunden
        checkForUpdates();
    }

    //Manueller Update-Trigger aus WebUI (deferred, damit kein async_tcp-Watchdog)
    if (pendingManualUpdate) {
        pendingManualUpdate = false;
        checkForUpdates();
    }
    if (pendingForceUpdate) {
        pendingForceUpdate = false;
        checkForUpdates(true, pendingForceChannel);
    }

    //messages.json verkleinern
    if (millis() > messagesDeleteTimer) {
        messagesDeleteTimer = millis() + 24 * 60 * 60 * 1000; //24 Stunden
        trimFile("/messages.json", MAX_STORED_MESSAGES);
    }

    //Topology-Reporting: einmal pro Stunde
    if (millis() > reportingTimer) {
        reportingTimer = millis() + 60 * 60 * 1000; //1 Stunde
        reportTopology();
    }
    //Topology-Reporting: bei Änderungen (30s Debounce)
    reportTopologyIfChanged();


    
}







