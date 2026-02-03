#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "ack.h"
#include "helperFunctions.h"

ACK acks[MAX_STORED_ACK];
uint16_t ackHead = 0;

void addACK(const char* srcCall, const char* nodeCall, const uint32_t id) {
    //Prüfen, ob ACK Frame bereits vorhanden
    bool found =  checkACK(srcCall, nodeCall, id);

    Serial.printf("ACK HEAD: %d\n", ackHead);

    //In ACK Liste speichern
    if (found == false) {
        strncpy(acks[ackHead].srcCall, srcCall, MAX_CALLSIGN_LENGTH);
        acks[ackHead].srcCall[MAX_CALLSIGN_LENGTH] = '\0'; 

        strncpy(acks[ackHead].nodeCall, nodeCall, MAX_CALLSIGN_LENGTH);
        acks[ackHead].nodeCall[MAX_CALLSIGN_LENGTH] = '\0';

        acks[ackHead].id = id;

        ackHead++;
        if (ackHead >= MAX_STORED_ACK) {
            ackHead = 0; 
        }
    }


    // if (found == false) {
    //     JsonDocument doc;
    //     doc["srcCall"] = srcCall;
    //     doc["nodeCall"] = nodeCall;
    //     doc["id"] = id;
    //     char* jsonBuffer = (char*)malloc(1024);
    //     size_t len = serializeJson(doc, jsonBuffer, 1024);
    //     addJSONtoFile(jsonBuffer, len, "/ack.json", MAX_STORED_ACK);
    //     free(jsonBuffer);
    //     jsonBuffer = nullptr;
    // }
}


bool checkACK(const char* srcCall, const char* nodeCall, const uint32_t id) {
    //Prüfen, ob ACK Frame bereits vorhanden
    for (int i = 0; i < MAX_STORED_ACK; i++) {
        // Wir prüfen, ob die ID matcht (Zahlenvergleiche sind am schnellsten)
        if (acks[i].id == id) {
            // Wenn ID passt, prüfen wir die Strings
            if (strcmp(acks[i].srcCall, srcCall) == 0 &&  strcmp(acks[i].nodeCall, nodeCall) == 0) {
                return true; 
            }
        }
    }

    return false;

    // File file = LittleFS.open("/ack.json", "r");
    // bool found = false;
    // if (file) {
    //     JsonDocument doc;
    //     while (file.available()) {
    //         DeserializationError error = deserializeJson(doc, file);
    //         if (error == DeserializationError::Ok) {
    //             if ((doc["id"].as<uint32_t>() == id) && (strcmp(doc["srcCall"], srcCall) == 0) && (strcmp(doc["nodeCall"], nodeCall) == 0)) {
    //                 found = true;
    //                 break; 
    //             }
    //         } else if (error != DeserializationError::EmptyInput) {
    //             file.readStringUntil('\n');
    //         }
    //     }
    //     file.close();                    
    // }
    // return found;
}

