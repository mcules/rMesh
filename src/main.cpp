#include <Arduino.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
// #include <time.h>
#include <vector>
// #include <HTTPClient.h>
// #include <HTTPUpdate.h>
// #include <WiFi.h>

#include "config.h"
#include "hal_LILYGO_T3_LoRa32_V1_6_1.h"
#include "frame.h"
#include "settings.h"
#include "main.h"
#include "wifiFunctions.h"
#include "webFunctions.h"
#include "serial.h"
#include "webFunctions.h"
#include "helperFunctions.h"


//Uhrzeitformat
const char* TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3";

//Peer-Liste
std::vector<Peer> peerList;
portMUX_TYPE peerListMux = portMUX_INITIALIZER_UNLOCKED;

//Sendepuffer
std::vector<Frame> txBuffer;
portMUX_TYPE txBufferMux = portMUX_INITIALIZER_UNLOCKED;

//Timing
uint32_t announceTimer = 5000;      //Erstes Announce nach 5 Sekunden
uint32_t statusTimer = 0;
uint32_t rebootTimer = 0xFFFFFFFF;

//Anderes Zeug -> muss weg
uint16_t irqFlags = 0;



void setup() {
    //UART
    Serial.begin(115200);
    Serial.setDebugOutput(true);  
    while (!Serial) {}

    //CPU Frqg fest (soll wegen SPI sinnvoll sein)
    setCpuFrequencyMhz(240);

    //Puffer
    peerList.reserve(PEER_LIST_SIZE);
    txBuffer.reserve(TX_BUFFER_SIZE);     

    //Einstellungen laden
    loadSettings();

    //Initialize LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("An error has occurred while mounting LittleFS");
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
		txBuffer.push_back(f);
	}  

    //Prüfen, ob was gesendet werden muss
	if ((txFlag == false) && (rxFlag == false)) {
	    //Frames mit retry > 1 werden synchron gesendet !!!
        //Prüfen, ob es Frames gibt, die noch nicht synchron gesendet wurden
        bool sendNewSyncFrame = true;
        for (int i = 0; i < txBuffer.size(); i++) {
            if (txBuffer[i].syncFlag == true) {sendNewSyncFrame = false;}
        }

        //Im Puffer nach synchronen Frames duchen und den 1. gefundenen zum Senden freigeben
        if (sendNewSyncFrame == true) {
            for (int i = 0; i < txBuffer.size(); i++) {
                if(txBuffer[i].retry > 1) {
                    txBuffer[i].syncFlag = true; 
                    txBuffer[i].transmitMillis = millis() + TX_RETRY_TIME + getTOA(30); //Time On Air für Antwort
                    break;   
                }
            }
        }
        
        //Sendepuffer duchlaufen und ggg. Frames senden
    	for (int i = 0; i < txBuffer.size(); i++) {
    		//Prüfen, ob Frame gesendet werden muss
    		if ((millis() > txBuffer[i].transmitMillis) && ((txBuffer[i].retry <= 1) || (txBuffer[i].syncFlag == true))) {
    			//Frame senden
                transmitFrame(txBuffer[i]);
                //Retrys runterzählen
                if (txBuffer[i].retry > 0) {txBuffer[i].retry --;}
                //Nächsten Sendezeitpunkt festlegen (nur relevant, wenn retry > 1)
                txBuffer[i].transmitMillis = millis() + TX_RETRY_TIME + getTOA(30); //Time On Air für Antwort
                //Wenn kein Retry mehr übrig, dann löschen
                if (txBuffer[i].retry == 0) {  
                    //Aus Peer-Liste löschen
                    if (txBuffer[i].initRetry > 1) {availablePeerList(txBuffer[i].viaCall, false);}
                    //Frame löschen
                    txBuffer.erase(txBuffer.begin() + i);
                }
                break;
    		}    
    	}
    }

    //Prüfen, ob was empfangen wurde
    Frame f;
    if (checkReceive(f)) {


        

        

    }


    //Status über Websocket senden
    if (millis() > statusTimer) {
        statusTimer = millis() + 1000;
        //Status über Websocket senden
        JsonDocument doc;
        doc["status"]["time"] = time(NULL);
        doc["status"]["tx"] = txFlag;
        doc["status"]["rx"] = rxFlag;
        doc["status"]["txBufferCount"] = txBuffer.size();
        char jsonBuffer[1024];  
        size_t len = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
        ws.textAll(jsonBuffer, len);  // sendet direkt den Puffer
    	//Peer-Liste checken
    	//checkPeerList();
    }

    //Reboot
    if (millis() > rebootTimer) {ESP.restart();}
}

