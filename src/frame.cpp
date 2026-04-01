#include <ArduinoJson.h>

#include "frame.h"
#include "helperFunctions.h"
#include "hal.h"
#include "webFunctions.h"

size_t Frame::exportBinary(uint8_t* data, size_t length) {
    //Binär-Daten erzeugen
    size_t position = 0;
    if (length < 1) return 0;
    //Frame-Typ + Hop-Count
    data[position] = (frameType & 0x0F) | ((hopCount & 0x0F) << 4);
    position ++;
    //Absender hinzufügen
    size_t sLen = strlen(srcCall);
    if (sLen > 0) {
        if (position + 1 + sLen > length) return position;
        data[position] = Frame::HeaderTypes::SRC_CALL_HEADER << 4 | (0x0F & sLen);
        position ++;
        memcpy(&data[position], srcCall, sLen);
        position += sLen;
    }
    //Node hinzufügen
    size_t nLen = strlen(nodeCall);
    if (nLen > 0) {
        if (position + 1 + nLen > length) return position;
        data[position] = Frame::HeaderTypes::NODE_CALL_HEADER << 4 | (0x0F & nLen);
        position ++;
        memcpy(&data[position], nodeCall, nLen);
        position += nLen;
    }
    //VIA-Call hinzufügen
    size_t vLen = strlen(viaCall);
    if (vLen > 0) {
        if (position + 1 + vLen > length) return position;
        data[position] = Frame::HeaderTypes::VIA_CALL_HEADER << 4 | (0x0F & vLen);
        position ++;
        memcpy(&data[position], viaCall, vLen);
        position += vLen;
    }
    //Destination hinzufügen
    size_t gLen = strlen(dstGroup);
    if (gLen > 0) {
        if (position + 1 + gLen > length) return position;
        data[position] = Frame::HeaderTypes::DST_GROUP_HEADER << 4 | (0x0F & gLen);
        position ++;
        memcpy(&data[position], dstGroup, gLen);
        position += gLen;
    }
    //Destination Call hinzufügen
    size_t dLen = strlen(dstCall);
    if (dLen > 0) {
        if (position + 1 + dLen > length) return position;
        data[position] = Frame::HeaderTypes::DST_CALL_HEADER << 4 | (0x0F & dLen);
        position ++;
        memcpy(&data[position], dstCall, dLen);
        position += dLen;
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

void Frame::monitorJSON() {
    //Schreibt Monitor-Daten in JSON-Buffer
    JsonDocument doc;
    // for (size_t i = 0; i < messageLength; i++) {
    //     doc["monitor"]["message"][i] = message[i];
    // }    
    if ((messageLength > 0) && ((messageType == Frame::MessageTypes::TEXT_MESSAGE) || (messageType == Frame::MessageTypes::TRACE_MESSAGE))) {
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

    size_t len = measureJson(doc);
    if (len == 0) return;
    #ifdef HAS_WIFI
    char* buf = (char*)malloc(len + 1);
    if (buf) {
        serializeJson(doc, buf, len + 1);
        wsBroadcast(buf, len);
        free(buf);
    }
    #endif


}

/**
 * @brief Escape a string for safe JSON embedding (handles \, ", and control chars).
 * @return Number of bytes written (excluding NUL).
 */
static size_t jsonEscape(char* dst, size_t dstLen, const char* src) {
    size_t d = 0;
    for (size_t i = 0; src[i] && d + 6 < dstLen; i++) {
        char c = src[i];
        if (c == '"')       { dst[d++] = '\\'; dst[d++] = '"'; }
        else if (c == '\\') { dst[d++] = '\\'; dst[d++] = '\\'; }
        else if (c == '\n') { dst[d++] = '\\'; dst[d++] = 'n'; }
        else if (c == '\r') { dst[d++] = '\\'; dst[d++] = 'r'; }
        else if (c == '\t') { dst[d++] = '\\'; dst[d++] = 't'; }
        else if ((uint8_t)c < 0x20) {
            d += snprintf(dst + d, dstLen - d, "\\u%04x", (uint8_t)c);
        }
        else { dst[d++] = c; }
    }
    dst[d] = '\0';
    return d;
}

size_t Frame::messageJSON(char* buffer, size_t length) {
    // Build message JSON without heap-allocating JsonDocument.
    char escapedText[512] = {0};
    char escapedDst[MAX_CALLSIGN_LENGTH * 2 + 1] = {0};
    char escapedDstGrp[MAX_CALLSIGN_LENGTH * 2 + 1] = {0};
    char escapedSrc[MAX_CALLSIGN_LENGTH * 2 + 1] = {0};

    jsonEscape(escapedDst, sizeof(escapedDst), dstCall);
    jsonEscape(escapedDstGrp, sizeof(escapedDstGrp), dstGroup);
    jsonEscape(escapedSrc, sizeof(escapedSrc), srcCall);

    size_t pos = 0;
    pos += snprintf(buffer + pos, length - pos, "{\"message\":{");

    if ((messageLength > 0) && (messageType == Frame::MessageTypes::TEXT_MESSAGE || messageType == Frame::MessageTypes::TRACE_MESSAGE)) {
        char text[messageLength + 1];
        safeUtf8Copy(text, (uint8_t*)message, messageLength);
        jsonEscape(escapedText, sizeof(escapedText), text);
        pos += snprintf(buffer + pos, length - pos, "\"text\":\"%s\",", escapedText);
    }

    pos += snprintf(buffer + pos, length - pos,
        "\"messageType\":%u,\"dstCall\":\"%s\",\"dstGroup\":\"%s\","
        "\"srcCall\":\"%s\",\"id\":%lu,\"tx\":%s,\"timestamp\":%ld,\"hopCount\":%u}}",
        messageType, escapedDst, escapedDstGrp, escapedSrc,
        (unsigned long)id, tx ? "true" : "false",
        (long)timestamp, hopCount);

    return pos;
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
                    if (payloadLength > MAX_CALLSIGN_LENGTH) {payloadLength = MAX_CALLSIGN_LENGTH;}
                    safeUtf8Copy(nodeCall, (const uint8_t*)(data + i + 1), payloadLength);
                    i += payloadLength + 1;
                } else {
                    i = length; //Abbruch
                }
                break;
            case Frame::HeaderTypes::VIA_CALL_HEADER:
                if (i + payloadLength < length) {
                    if (payloadLength > MAX_CALLSIGN_LENGTH) {payloadLength = MAX_CALLSIGN_LENGTH;}
                    safeUtf8Copy(viaCall, (const uint8_t*)(data + i + 1), payloadLength);
                    i += payloadLength + 1;
                } else {
                    i = length; //Abbruch
                }
                break;
            case Frame::HeaderTypes::SRC_CALL_HEADER:
                if (i + payloadLength < length) {
                    if (payloadLength > MAX_CALLSIGN_LENGTH) {payloadLength = MAX_CALLSIGN_LENGTH;}
                    safeUtf8Copy(srcCall, (const uint8_t*)(data + i + 1), payloadLength);
                    i += payloadLength + 1;
                } else {
                    i = length; //Abbruch
                }
                break;
            case Frame::HeaderTypes::DST_GROUP_HEADER:
                if (i + payloadLength < length) {
                    if (payloadLength > MAX_CALLSIGN_LENGTH) {payloadLength = MAX_CALLSIGN_LENGTH;}
                    safeUtf8Copy(dstGroup, (const uint8_t*)(data + i + 1), payloadLength);
                    i += payloadLength + 1;
                } else {
                    i = length; //Abbruch
                }
                break;
            case Frame::HeaderTypes::DST_CALL_HEADER:
                if (i + payloadLength < length) {
                    if (payloadLength > MAX_CALLSIGN_LENGTH) {payloadLength = MAX_CALLSIGN_LENGTH;}
                    safeUtf8Copy(dstCall, (const uint8_t*)(data + i + 1), payloadLength);
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

}