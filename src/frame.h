#pragma once

#include <Arduino.h>
#include <time.h>

#include "config.h"


struct Frame {
    char srcCall[MAX_CALLSIGN_LENGTH + 1] = {0};
    char dstGroup[MAX_CALLSIGN_LENGTH + 1] = {0};
    char dstCall[MAX_CALLSIGN_LENGTH + 1] = {0};
    char nodeCall[MAX_CALLSIGN_LENGTH + 1] = {0};
    char viaCall[MAX_CALLSIGN_LENGTH + 1] = {0};
    uint8_t message[260] = {0};
    size_t messageLength = 0;
    uint8_t messageType = TEXT_MESSAGE;
    uint32_t id = millis();
    time_t timestamp = time(NULL);
    uint8_t hopCount = 0;
    uint8_t retry = 1;
    uint8_t initRetry = 1;
    uint32_t transmitMillis = 0;
    uint8_t frameType = MESSAGE_FRAME;
    float rssi = 0;
    float snr = 0;
    float frqError = 0;
    bool tx = false;
    bool syncFlag = false;
    uint8_t port = 0;

    size_t exportBinary(uint8_t* data, size_t length);
    void importBinary(uint8_t* data, size_t length);
    void monitorJSON();
    size_t messageJSON(char* buffer, size_t length);

    enum FrameTypes {
        ANNOUNCE_FRAME,  
        ANNOUNCE_ACK_FRAME,
        TUNE_FRAME,
        MESSAGE_FRAME,
        MESSAGE_ACK_FRAME
    };

    enum MessageTypes {
        //Lower 4 bits of header byte -> 0x00 to 0x0F; Only for MESSAGE-HEADER (otherwise it's payload length)
        TEXT_MESSAGE = 0,
        TRACE_MESSAGE = 1,
        COMMAND_MESSAGE = 15   //Remote control commands for node: 0xFF:Version, 0xFE:Reboot
    };

    enum HeaderTypes {
        //Upper 4 bits of header byte -> 0x00 to 0x0F
        SRC_CALL_HEADER,  
        DST_GROUP_HEADER,
        MESSAGE_HEADER,
        NODE_CALL_HEADER,
        VIA_CALL_HEADER,
        DST_CALL_HEADER
    };

};

