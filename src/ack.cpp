#include <Arduino.h>
#ifdef NRF52_PLATFORM
#include "platform_nrf52.h"
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#else
#include <LittleFS.h>
#endif
#include <ArduinoJson.h>

#include "ack.h"
#include "helperFunctions.h"

// Exposed via ack.h for debug query access
ACK acks[MAX_STORED_ACK];
uint16_t ackHead = 0;

void addACK(const char* srcCall, const char* nodeCall, const uint32_t id) {
    //Check if ACK frame already exists
    bool found =  checkACK(srcCall, nodeCall, id);
    //Store in ACK list
    if (found == false) {
        memcpy(acks[ackHead].srcCall, srcCall, MAX_CALLSIGN_LENGTH + 1);
        memcpy(acks[ackHead].nodeCall, nodeCall, MAX_CALLSIGN_LENGTH + 1);
        acks[ackHead].id = id;

        ackHead++;
        if (ackHead >= MAX_STORED_ACK) {
            ackHead = 0; 
        }
    }
}


bool checkACK(const char* srcCall, const char* nodeCall, const uint32_t id) {
    //Check if ACK frame already exists
    for (int i = 0; i < MAX_STORED_ACK; i++) {
        // Check if the ID matches (numeric comparisons are fastest)
        if (acks[i].id == id) {
            // If ID matches, check the strings
            if (strcmp(acks[i].srcCall, srcCall) == 0 &&  strcmp(acks[i].nodeCall, nodeCall) == 0) {
                return true; 
            }
        }
    }
    return false;
}

