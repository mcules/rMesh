#include <ArduinoJson.h>

#include "frame.h"
#include "helperFunctions.h"
#include "hal_LILYGO_T3_LoRa32_V1_6_1.h"

size_t Frame::exportBinary(uint8_t* data, size_t length) {
    //Binär-Daten erzeugen
    size_t position = 0;
    //Frame-Typ text
    data[position] = frameType & 0x0F;
    data[position] = data[position] | (hopCount & 0x0F) << 4;
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
    for (size_t i = 0; i < messageLength; i++) {
        doc["monitor"]["message"][i] = message[i];
    }    
    char text[messageLength + 1] = {0};
    strncpy(text, (char*)message, messageLength);
    doc["monitor"]["text"] = text;
    doc["monitor"]["messageType"] = messageType;
    doc["monitor"]["messageLength"] = messageLength;
    doc["monitor"]["tx"] = tx;
    doc["monitor"]["rssi"] = rssi;
    doc["monitor"]["snr"] = snr;
    doc["monitor"]["frequencyError"] = frqError;
    doc["monitor"]["time"] = timestamp;
    doc["monitor"]["srcCall"] = srcCall;
    doc["monitor"]["dstCall"] = dstCall;
    doc["monitor"]["viaCall"] = viaCall;
    doc["monitor"]["nodeCall"] = nodeCall;
    doc["monitor"]["frameType"] = frameType;
    doc["monitor"]["id"] = id;
    doc["monitor"]["hopCount"] = hopCount;
    doc["monitor"]["initRetry"] = initRetry;
    doc["monitor"]["retry"] = retry;
    size_t len = serializeJson(doc, buffer, length);
    return len;
}

size_t Frame::messageJSON(char* buffer, size_t length) {
    //Schreibt Message-Daten in JSON-Buffer
    JsonDocument doc;
    for (size_t i = 0; i < messageLength; i++) {
        doc["message"]["message"][i] = message[i];
    }    
    char text[messageLength + 1] = {0};
    strncpy(text, (char*)message, messageLength);
    doc["message"]["text"] = text;
    doc["message"]["messageType"] = messageType;
    doc["message"]["srcCall"] = srcCall;
    doc["message"]["dstCall"] = dstCall;
    doc["message"]["id"] = id;
    doc["message"]["tx"] = tx;
    doc["message"]["time"] = timestamp;
    size_t len = serializeJson(doc, buffer, length);
    return len;
}


void Frame::importBinary(uint8_t* data, size_t length) {

    if (length <= 1) { return; }

    Serial.println("--------------");
    printHexArray(data, length);

    //Frame-TYP
    frameType = data[0] & 0x0F;
    hopCount = (data[0] & 0xF0) >> 4;

    //Frame druchlaufen und nach Headern suchen
    uint8_t header = 0;
    uint8_t payloadLength = 0;
    size_t i = 1;
    while (i < length) {
        //Header prüfen
        header = data[i] >> 4;
        payloadLength = (data[i] & 0x0F);
        i = i + 1;
        if (i >= length) {break;}
        switch (header) {
            case Frame::HeaderTypes::DST_CALL_HEADER:
                memcpy(dstCall, data + i, sizeof(dstCall)); 
                if (payloadLength >= sizeof(dstCall)) {payloadLength = sizeof(dstCall) - 1;}
                dstCall[payloadLength] = '\0';
                if ((i + payloadLength) < length) {i += payloadLength;}
                break;
            case Frame::HeaderTypes::VIA_CALL_HEADER:
                memcpy(viaCall, data + i, sizeof(viaCall)); 
                if (payloadLength >= sizeof(viaCall)) {payloadLength = sizeof(viaCall) - 1;}
                viaCall[payloadLength] = '\0';
                if ((i + payloadLength) < length) {i += payloadLength;}
                break;
            case Frame::HeaderTypes::NODE_CALL_HEADER:
                memcpy(nodeCall, data + i, sizeof(nodeCall)); 
                if (payloadLength >= sizeof(nodeCall)) {payloadLength = sizeof(nodeCall) - 1;}
                nodeCall[payloadLength] = '\0';
                if ((i + payloadLength) < length) {i += payloadLength;}
                break;
            case Frame::HeaderTypes::SRC_CALL_HEADER:
                memcpy(srcCall, data + i, sizeof(srcCall)); 
                if (payloadLength >= sizeof(srcCall)) {payloadLength = sizeof(srcCall) - 1;}
                srcCall[payloadLength] = '\0';
                if ((i + payloadLength) < length) {i += payloadLength;}
                break;
            case Frame::HeaderTypes::MESSAGE_HEADER:
                //Message Type Setzen
                messageType = (data[i - 1] & 0x0F);
                //ID ausschneiden
                if (length >= (i + sizeof(id))) {
                    id = (data[i + 4] << 24) + (data[i + 3] << 16) + (data[i + 2] << 8) + data[i + 1];  
                    i += sizeof(id) + 1;
                }
                //Message Länge
                messageLength = length - i;
                //Message
                memcpy(message, data + i, messageLength);
                //Suche beenden
                i = length;
                break;
            default:
                i = length;
                break;
        }      
    }

    Serial.printf("messageType: *%s*\n", messageType);
    Serial.printf("messageLength: *%s*\n", messageLength);
    Serial.printf("id: *%s*\n", id);
    Serial.printf("dstCall: *%s*\n", dstCall);
    Serial.printf("srcCall: *%s*\n", srcCall);
    Serial.printf("nodeCall: *%s*\n", nodeCall);
    Serial.printf("viaCall: *%s*\n", viaCall);

}