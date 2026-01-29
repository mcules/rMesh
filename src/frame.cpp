#include <ArduinoJson.h>

#include "frame.h"
#include "helperFunctions.h"
#include "hal.h"

size_t Frame::exportBinary(uint8_t* data, size_t length) {
    //Binär-Daten erzeugen
    size_t position = 0;
    //Frame-Typ + Hop-Count
    data[position] = (frameType & 0x0F) | ((hopCount & 0x0F) << 4);
    position ++;
    //Absender hinzufügen
    if (strlen(srcCall) > 0) {
        data[position] = Frame::HeaderTypes::SRC_CALL_HEADER << 4 | (0x0F & strlen(srcCall));  
        position ++;
        memcpy(&data[position], srcCall, strlen(srcCall)); //Payload
        position += strlen(srcCall);
    }
    //Node hinzufügen
    if (strlen(nodeCall) > 0) {
        data[position] = Frame::HeaderTypes::NODE_CALL_HEADER << 4 | (0x0F & strlen(nodeCall));  
        position ++;
        memcpy(&data[position], nodeCall, strlen(nodeCall)); //Payload
        position += strlen(nodeCall);
    }
    //VIA-Call hinzufügen
    if (strlen(viaCall) > 0) {
        data[position] = Frame::HeaderTypes::VIA_CALL_HEADER << 4 | (0x0F & strlen(viaCall));  
        position ++;
        memcpy(&data[position], viaCall, strlen(viaCall)); //Payload
        position += strlen(viaCall);
    }
    //Destination hinzufügen
    if (strlen(dstGroup) > 0) {
        data[position] = Frame::HeaderTypes::DST_GROUP_HEADER << 4 | (0x0F & strlen(dstGroup));  
        position ++;
        memcpy(&data[position], dstGroup, strlen(dstGroup)); //Payload
        position += strlen(dstGroup);
    }
    //Destination Call hinzufügen
    if (strlen(dstCall) > 0) {
        data[position] = Frame::HeaderTypes::DST_CALL_HEADER << 4 | (0x0F & strlen(dstCall));  
        position ++;
        memcpy(&data[position], dstCall, strlen(dstCall)); //Payload
        position += strlen(dstCall);
    }
    //Message hinzu (muss immer ganz hinten sein, weil keine Längenangabe)
    switch (frameType) {
        case Frame::FrameTypes::MESSAGE_FRAME:
        case Frame::FrameTypes::MESSAGE_ACK_FRAME:
            //Normale Message Frames
            //TYP
            data[position] = Frame::HeaderTypes::MESSAGE_HEADER << 4;     
            data[position] = data[position] | messageType;               
            position ++;
            //ID
            memcpy(&data[position], &id, sizeof(id)); //Payload
            position += sizeof(id);
            //Message kopieren
            if (messageLength > (length - position)) { messageLength = length - position; }
            memcpy(&data[position], &message, messageLength); //Payload
            position += messageLength;    
            break;        
        case Frame::FrameTypes::TUNE_FRAME:
            while (position < 255) {
                data[position] = 0xFF;
                position ++;
            }
            break;
    }
    return position;
}

size_t Frame::monitorJSON(char* buffer, size_t length) {
    //Schreibt Monitor-Daten in JSON-Buffer
    JsonDocument doc;
    if (messageLength > 0) {
        for (size_t i = 0; i < messageLength; i++) {
            doc["monitor"]["message"][i] = message[i];
        }    
        char text[messageLength + 1];
        safeUtf8Copy(text, (uint8_t*)message, messageLength);
        doc["monitor"]["text"] = text;
    }
    doc["monitor"]["messageType"] = messageType;
    doc["monitor"]["messageLength"] = messageLength;
    doc["monitor"]["tx"] = tx;
    if (tx == false) {
        doc["monitor"]["rssi"] = rssi;
        doc["monitor"]["snr"] = snr;
        doc["monitor"]["frqError"] = frqError;
    }
    doc["monitor"]["timestamp"] = timestamp;
    if (strlen(srcCall) > 0) {doc["monitor"]["srcCall"] = srcCall;}
    if (strlen(dstGroup) > 0) {doc["monitor"]["dstGroup"] = dstGroup;}
    if (strlen(dstCall) > 0) {doc["monitor"]["dstCall"] = dstCall;}
    if (strlen(viaCall) > 0) {doc["monitor"]["viaCall"] = viaCall;}
    if (strlen(nodeCall) > 0) {doc["monitor"]["nodeCall"] = nodeCall;}
    doc["monitor"]["frameType"] = frameType;
    doc["monitor"]["id"] = id;
    doc["monitor"]["hopCount"] = hopCount;
    doc["monitor"]["initRetry"] = initRetry;
    doc["monitor"]["retry"] = retry;
    doc["monitor"]["port"] = port;
    size_t len = serializeJson(doc, buffer, length);
    return len;
}

size_t Frame::messageJSON(char* buffer, size_t length) {
    //Schreibt Message-Daten in JSON-Buffer
    JsonDocument doc;
    for (size_t i = 0; i < messageLength; i++) {
        doc["message"]["message"][i] = message[i];
    }    
    char text[messageLength + 1];
    safeUtf8Copy(text, (uint8_t*)message, messageLength);
    doc["message"]["text"] = text;    
    doc["message"]["messageType"] = messageType;
    doc["message"]["dstCall"] = dstCall;
    doc["message"]["dstGroup"] = dstGroup;
    doc["message"]["srcCall"] = srcCall;
    doc["message"]["id"] = id;
    doc["message"]["tx"] = tx;
    doc["message"]["timestamp"] = timestamp;
    size_t len = serializeJson(doc, buffer, length);
    return len;
}


void Frame::importBinary(uint8_t* data, size_t length) {
    //Abbruch, wenn Frame zu kurz
    if (length <= 1) { return; }

    //Frame-TYP
    frameType = data[0] & 0x0F;
    hopCount = (data[0] & 0xF0) >> 4;

    //Frame druchlaufen und nach Headern suchen
    uint8_t header = 0;
    uint8_t payloadLength = 0;
    size_t i = 1;       //Position 1. Header
    while (i < length) {
        //Header prüfen
        header = data[i] >> 4;
        payloadLength = data[i] & 0x0F;
        switch (header) {
            case Frame::HeaderTypes::NODE_CALL_HEADER:
                if (i + payloadLength < length) {
                    memcpy(nodeCall, data + i + 1, sizeof(nodeCall)); 
                    if (payloadLength >= sizeof(nodeCall)) {payloadLength = sizeof(nodeCall) - 1;}
                    nodeCall[payloadLength] = '\0';
                    i += payloadLength + 1;
                } else {
                    i = length; //Abbruch
                }
                break;
            case Frame::HeaderTypes::VIA_CALL_HEADER:
                if (i + payloadLength < length) {
                    memcpy(viaCall, data + i + 1, sizeof(viaCall)); 
                    if (payloadLength >= sizeof(viaCall)) {payloadLength = sizeof(viaCall) - 1;}
                    viaCall[payloadLength] = '\0';
                    i += payloadLength + 1;
                } else {
                    i = length; //Abbruch
                }
                break;
            case Frame::HeaderTypes::SRC_CALL_HEADER:
                if (i + payloadLength < length) {
                    memcpy(srcCall, data + i + 1, sizeof(srcCall)); 
                    if (payloadLength >= sizeof(srcCall)) {payloadLength = sizeof(srcCall) - 1;}
                    srcCall[payloadLength] = '\0';
                    i += payloadLength + 1;
                } else {
                    i = length; //Abbruch
                }
                break;
            case Frame::HeaderTypes::DST_GROUP_HEADER:
                if (i + payloadLength < length) {
                    memcpy(dstGroup, data + i + 1, sizeof(dstGroup)); 
                    if (payloadLength >= sizeof(dstGroup)) {payloadLength = sizeof(dstGroup) - 1;}
                    dstGroup[payloadLength] = '\0';
                    i += payloadLength + 1;
                } else {
                    i = length; //Abbruch
                }
                break;
            case Frame::HeaderTypes::DST_CALL_HEADER:
                if (i + payloadLength < length) {
                    memcpy(dstCall, data + i + 1, sizeof(dstCall)); 
                    if (payloadLength >= sizeof(dstCall)) {payloadLength = sizeof(dstCall) - 1;}
                    dstCall[payloadLength] = '\0';
                    i += payloadLength + 1;
                } else {
                    i = length; //Abbruch
                }
                break;
            case Frame::HeaderTypes::MESSAGE_HEADER:
                if (i + sizeof(id) < length) {
                    //Message Type 
                    messageType = payloadLength; //Wird dopplet verwendet 
                    i++;
                    //Message ID
                    memcpy(&id, &data[i], sizeof(id));  
                    i = i + sizeof(id);
                    //Message Länge
                    messageLength = length - i;
                    //Message
                    memcpy(message, data + i, messageLength);
                    //Frame Ende
                    i = length;
                } else {
                    i = length; //Abbruch
                }
                break;
            default: //Falscher Header
                i = length;
                break;
        }      
    }

    // Serial.printf("messageType: *%d*\n", messageType);
    // Serial.printf("messageLength: *%d*\n", messageLength);
    // Serial.printf("id: *%d*\n", id);
    // Serial.printf("dstCall: *%s*\n", dstCall);
    // Serial.printf("srcCall: *%s*\n", srcCall);
    // Serial.printf("nodeCall: *%s*\n", nodeCall);
    // Serial.printf("viaCall: *%s*\n", viaCall);

}