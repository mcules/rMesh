#include <Arduino.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include <vector>
#include <nvs_flash.h>

#include "config.h"
#include "Hal.h"
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


//Uhrzeitformat
const char* TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3";

//Sendepuffer
std::vector<Frame> txBuffer;
//portMUX_TYPE txBufferMux = portMUX_INITIALIZER_UNLOCKED;

//Speicher für die letzten Message IDs
MSG messages[MAX_STORED_MESSAGES_RAM];
uint16_t messagesHead = 0;


//Timing
uint32_t announceTimer = 5000;      //Erstes Announce nach 5 Sekunden
uint32_t statusTimer = 0;
uint32_t rebootTimer = 0xFFFFFFFF;
uint8_t currentRetry = 0;
uint32_t updateCheckTimer = 60 * 60 * 1000;  //Erster Check nach 1 Stunde




void processRxFrame(Frame &f) {
    //Abbruch, wenn kein nodeCall
    if (strlen(f.nodeCall) == 0) {return;}
    uint32_t pft = millis();

    //Monitor
    char* jsonBuffer = (char*)malloc(4096);
    size_t len = f.monitorJSON(jsonBuffer, 4096);
    ws.textAll(jsonBuffer, len); 
    free(jsonBuffer);
    jsonBuffer = nullptr;

    //Peer List
    addPeerList(f);

    //Auswerten
    Frame tf;                   //ggf. Antwort-Frame
    bool found = false;         //z.b.V.
    File file;                  //z.b.V

    pft = millis() - pft; Serial.printf("addPeer Time: %d\n", pft); pft = millis();
    switch (f.frameType) {

        //Antwort auf announce
        case Frame::FrameTypes::ANNOUNCE_FRAME:
            if (strlen(f.nodeCall) > 0 ){
                tf.frameType = Frame::FrameTypes::ANNOUNCE_ACK_FRAME;
                tf.port = f.port;
                switch (tf.port){
                    case 0: tf.transmitMillis = millis() + calculateAckTime(); break;  //Time On Air für Antwort
                    case 1: tf.transmitMillis = millis(); break; //Bei UDP schneller
                }
                memcpy(tf.viaCall, f.nodeCall, sizeof(tf.viaCall));
                txBuffer.push_back(tf);
                pft = millis() - pft; Serial.printf("ANNOUNCE_FRAME Time: %d\n", pft); pft = millis();

            }
            break;
        //In Peer Liste eintragen
        case Frame::FrameTypes::ANNOUNCE_ACK_FRAME:
            if (strcmp(f.viaCall, settings.mycall) == 0) {
                availablePeerList(f.nodeCall, true, f.port);    
            }
            pft = millis() - pft; Serial.printf("ANNOUNCE_ACK_FRAME Time: %d\n", pft); pft = millis();

            break;

        //Senden abbrechen
        case Frame::FrameTypes::MESSAGE_ACK_FRAME:
            //In Peer Liste eintragen
            if (strcmp(f.viaCall, settings.mycall) == 0) {
                availablePeerList(f.nodeCall, true, f.port);
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

            pft = millis() - pft; Serial.printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! MESSAGE_ACK_FRAME Time: %d\n", pft); pft = millis();
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
                        return (strcmp(txB.srcCall, f.srcCall) == 0) && (strcmp(txB.viaCall, f.viaCall) == 0) && (txB.id == f.id);
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

            pft = millis() - pft; Serial.printf("Alle alten ACKs im TX-Puffer löschen Time: %d\n", pft); pft = millis();


            //ACK-Senden bei mir immer, bei anderen nur 1x
            if ((strcmp(f.viaCall, settings.mycall) == 0) || ((strlen(f.viaCall) > 0) && (checkACK(f.srcCall, f.nodeCall, f.id) == false) && (checkACK(f.srcCall, settings.mycall, f.id) == false))) {
                addACK(f.srcCall, f.nodeCall, f.id);
                tf.frameType = Frame::FrameTypes::MESSAGE_ACK_FRAME;
                memcpy(tf.viaCall, f.nodeCall, sizeof(tf.viaCall));
                memcpy(tf.srcCall, f.nodeCall, sizeof(tf.srcCall));
                tf.id = f.id;
                tf.port = f.port;
                tf.transmitMillis = millis() + calculateAckTime();
                if (tf.port == 1) {tf.transmitMillis = 0;} //Bei UDP sofort ACK
                txBuffer.push_back(tf);
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
            
            // file = LittleFS.open("/messages.json", "r");
            // found = false;  
            // if (file) {
            //     JsonDocument doc;
            //     while (file.available()) {
            //         DeserializationError error = deserializeJson(doc, file);
            //         if (error == DeserializationError::Ok) {
            //             if ((doc["message"]["id"].as<uint32_t>() == f.id) && (strcmp(doc["message"]["srcCall"], f.srcCall) == 0)) {
            //                 found = true;
            //                 break; 
            //             }
            //         } else if (error != DeserializationError::EmptyInput) {
            //             file.readStringUntil('\n');
            //         }
            //     }
            //     file.close();                    
            // }

            pft = millis() - pft; Serial.printf("Message ID und SRC-Call in Datei suchen Time: %d\n", pft); pft = millis();


            if ((found == false) && (f.messageLength > 0)) {
                //Neue Nachricht empfangen
                
                //Message in Ringpuffer speichern
                strncpy(messages[messagesHead].srcCall, f.srcCall, MAX_CALLSIGN_LENGTH);
                messages[messagesHead].id = f.id;
                messagesHead++;
                if (messagesHead >= MAX_STORED_MESSAGES_RAM) { messagesHead = 0; }                        
                pft = millis() - pft; Serial.printf("Message in Ringpuffer speichern Time: %d\n", pft); pft = millis();

                //Message an Websocket senden & speichern
                char* jsonBuffer = (char*)malloc(2048);
                size_t len = f.messageJSON(jsonBuffer, 2048);
                ws.textAll(jsonBuffer, len);
                pft = millis() - pft; Serial.printf("Message an Websocket senden  Time: %d\n", pft); pft = millis();

                addJSONtoFile(jsonBuffer, len, "/messages.json", MAX_STORED_MESSAGES);
                pft = millis() - pft; Serial.printf("Message an Websocket JSON speichern Time: %d\n", pft); pft = millis();

                free(jsonBuffer);
                jsonBuffer = nullptr;




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


                //Messages wiederholen
                if (settings.loraRepeat == true) {
                    //Frame vorbereiten
                    tf.frameType = f.frameType;
                    memcpy(tf.srcCall, f.srcCall, sizeof(tf.srcCall));
                    memcpy(tf.dstGroup, f.dstGroup, sizeof(tf.srcCall));
                    memcpy(tf.dstCall, f.dstCall, sizeof(tf.dstCall));
                    tf.hopCount = f.hopCount;
                    if (tf.hopCount < 15) {tf.hopCount ++;}
                    tf.messageType = f.messageType;                        
                    memcpy(tf.message, f.message, sizeof(tf.message));
                    tf.messageLength = f.messageLength;
                    tf.id = f.id;
                    tf.timestamp = time(NULL);
                    tf.syncFlag = false;

                    //Ports duchlaufen
                    for (tf.port = 0; tf.port <= 1; tf.port++) {

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
            pft = millis() - pft; Serial.printf("Fertig Time: %d\n", pft); pft = millis();

            break;
    }

    pft = millis() - pft;
    Serial.printf("processRxFrame Time: %d\n", pft);


    Serial.printf("Heap: %u / %u (Max Block: %u)\n", ESP.getFreeHeap(), ESP.getHeapSize(), ESP.getMaxAllocHeap());
 }


void setup() {
    //UART
    Serial.begin(115200);
    Serial.setDebugOutput(true);  
    while (!Serial) {}

    //CPU Frqg fest (soll wegen SPI sinnvoll sein)
    setCpuFrequencyMhz(240);
    esp_log_level_set("NetworkUdp", ESP_LOG_NONE);
    esp_log_level_set("NetworkUdp", ESP_LOG_ERROR);
    esp_log_level_set("vfs", ESP_LOG_WARN);
    esp_log_level_set("vfs", ESP_LOG_NONE);

    //Puffer
    peerList.reserve(PEER_LIST_SIZE);
    txBuffer.reserve(TX_BUFFER_SIZE);     

    //Einstellungen laden
    loadSettings();

    //Initialize LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("An error has occurred while mounting LittleFS");
    } 

    //Messages JSON in messages Ringpuffer speichern
    File file = LittleFS.open("/messages.json", "r");
    if (file) {
        JsonDocument doc;
        while (file.available()) {
            DeserializationError error = deserializeJson(doc, file);
            if (error == DeserializationError::Ok) {
                Serial.printf("id: %d, src: %s, head:%d\n", doc["message"]["id"].as<uint32_t>(), doc["message"]["srcCall"].as<String>(), messagesHead);
                //In messages speichern
                strncpy(messages[messagesHead].srcCall, doc["message"]["srcCall"], MAX_CALLSIGN_LENGTH);
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

	//Announce Senden
	if (millis() > announceTimer) {
		announceTimer = millis() + ANNOUNCE_TIME;
		Frame f;
		f.frameType = Frame::FrameTypes::ANNOUNCE_FRAME;
		f.transmitMillis = 0;
		//Frame in SendeBuffer
        f.port = 0;
		txBuffer.push_back(f);
        f.port = 1;
		txBuffer.push_back(f);
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
}







