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
    //In ACK Liste speichern
    if (found == false) {
        memcpy(acks[ackHead].srcCall, srcCall, MAX_CALLSIGN_LENGTH);
        memcpy(acks[ackHead].nodeCall, nodeCall, MAX_CALLSIGN_LENGTH);
        acks[ackHead].id = id;

        ackHead++;
        if (ackHead >= MAX_STORED_ACK) {
            ackHead = 0; 
        }
    }
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
}

