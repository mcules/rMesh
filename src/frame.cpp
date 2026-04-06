#include <Arduino.h>
#include <ArduinoJson.h>

#include "frame.h"
#include "helperFunctions.h"
#include "hal.h"
#include "webFunctions.h"

size_t Frame::exportBinary(uint8_t* data, size_t length) {
    //Generate binary data
    size_t position = 0;
    if (length < 1) return 0;
    //Frame type + hop count
    data[position] = (frameType & 0x0F) | ((hopCount & 0x0F) << 4);
    position ++;
    //Add sender
    size_t sLen = strlen(srcCall);
    if (sLen > 0) {
        if (position + 1 + sLen > length) return position;
        data[position] = Frame::HeaderTypes::SRC_CALL_HEADER << 4 | (0x0F & sLen);
        position ++;
        memcpy(&data[position], srcCall, sLen);
        position += sLen;
    }
    //Add node
    size_t nLen = strlen(nodeCall);
    if (nLen > 0) {
        if (position + 1 + nLen > length) return position;
        data[position] = Frame::HeaderTypes::NODE_CALL_HEADER << 4 | (0x0F & nLen);
        position ++;
        memcpy(&data[position], nodeCall, nLen);
        position += nLen;
    }
    //Add VIA call
    size_t vLen = strlen(viaCall);
    if (vLen > 0) {
        if (position + 1 + vLen > length) return position;
        data[position] = Frame::HeaderTypes::VIA_CALL_HEADER << 4 | (0x0F & vLen);
        position ++;
        memcpy(&data[position], viaCall, vLen);
        position += vLen;
    }
    //Add destination
    size_t gLen = strlen(dstGroup);
    if (gLen > 0) {
        if (position + 1 + gLen > length) return position;
        data[position] = Frame::HeaderTypes::DST_GROUP_HEADER << 4 | (0x0F & gLen);
        position ++;
        memcpy(&data[position], dstGroup, gLen);
        position += gLen;
    }
    //Add destination call
    size_t dLen = strlen(dstCall);
    if (dLen > 0) {
        if (position + 1 + dLen > length) return position;
        data[position] = Frame::HeaderTypes::DST_CALL_HEADER << 4 | (0x0F & dLen);
        position ++;
        memcpy(&data[position], dstCall, dLen);
        position += dLen;
    }
    //Add message (must always be at the end since there is no length field)
    switch (frameType) {
        case Frame::FrameTypes::MESSAGE_FRAME:
        case Frame::FrameTypes::MESSAGE_ACK_FRAME:
            //Normal message frames
            //Type
            data[position] = Frame::HeaderTypes::MESSAGE_HEADER << 4;     
            data[position] = data[position] | messageType;               
            position ++;
            //ID
            memcpy(&data[position], &id, sizeof(id)); //Payload
            position += sizeof(id);
            //Copy message
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
    #ifdef HAS_WIFI
    if (ESP.getFreeHeap() < 40000) return;

    // Rate-limit: max 2 monitor messages per second to reduce heap pressure
    // from WebSocket shared_ptr allocations
    static uint32_t lastMonitorMs = 0;
    static uint8_t  monitorCount  = 0;
    uint32_t now = millis();
    if ((int32_t)(now - lastMonitorMs) >= 500) {
        lastMonitorMs = now;
        monitorCount = 0;
    }
    if (++monitorCount > 2) return;

    // Build monitor JSON directly into static buffer (no heap JsonDocument).
    // Only called from the main loop, so a static buffer is safe.
    static char buf[512];
    int pos = 0;
    const int cap = (int)sizeof(buf);

    pos += snprintf(buf + pos, cap - pos, "{\"monitor\":{");

    if (messageLength > 0 && messageLength <= sizeof(message) &&
        (messageType == Frame::MessageTypes::TEXT_MESSAGE || messageType == Frame::MessageTypes::TRACE_MESSAGE)) {
        char text[261];
        size_t safeLen = messageLength < sizeof(text) ? messageLength : sizeof(text) - 1;
        safeUtf8Copy(text, (uint8_t*)message, safeLen, sizeof(text));
        pos += snprintf(buf + pos, cap - pos, "\"text\":\"");
        // JSON-escape the text inline
        for (const char *p = text; *p && pos + 7 < cap; p++) {
            switch (*p) {
                case '"':  buf[pos++] = '\\'; buf[pos++] = '"';  break;
                case '\\': buf[pos++] = '\\'; buf[pos++] = '\\'; break;
                case '\n': buf[pos++] = '\\'; buf[pos++] = 'n';  break;
                case '\r': buf[pos++] = '\\'; buf[pos++] = 'r';  break;
                case '\t': buf[pos++] = '\\'; buf[pos++] = 't';  break;
                default:
                    if ((uint8_t)*p < 0x20)
                        pos += snprintf(buf + pos, cap - pos, "\\u%04x", (uint8_t)*p);
                    else
                        buf[pos++] = *p;
            }
        }
        pos += snprintf(buf + pos, cap - pos, "\",");
    }

    pos += snprintf(buf + pos, cap - pos,
        "\"messageType\":%u,\"messageLength\":%u,\"tx\":%s",
        (unsigned)messageType, (unsigned)messageLength, tx ? "true" : "false");

    if (!tx) {
        pos += snprintf(buf + pos, cap - pos,
            ",\"rssi\":%.1f,\"snr\":%.1f,\"frqError\":%.1f", rssi, snr, frqError);
    }

    pos += snprintf(buf + pos, cap - pos, ",\"timestamp\":%ld", (long)timestamp);

    if (srcCall[0])  pos += snprintf(buf + pos, cap - pos, ",\"srcCall\":\"%s\"", srcCall);
    if (dstGroup[0]) pos += snprintf(buf + pos, cap - pos, ",\"dstGroup\":\"%s\"", dstGroup);
    if (dstCall[0])  pos += snprintf(buf + pos, cap - pos, ",\"dstCall\":\"%s\"", dstCall);
    if (viaCall[0])  pos += snprintf(buf + pos, cap - pos, ",\"viaCall\":\"%s\"", viaCall);
    if (nodeCall[0]) pos += snprintf(buf + pos, cap - pos, ",\"nodeCall\":\"%s\"", nodeCall);

    pos += snprintf(buf + pos, cap - pos,
        ",\"frameType\":%u,\"id\":%lu,\"hopCount\":%u,\"initRetry\":%u,\"retry\":%u,\"port\":%u}}",
        (unsigned)frameType, (unsigned long)id, (unsigned)hopCount,
        (unsigned)initRetry, (unsigned)retry, (unsigned)port);

    if (pos > 0 && pos < cap) {
        wsBroadcast(buf, pos);
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
        safeUtf8Copy(text, (uint8_t*)message, messageLength, messageLength + 1);
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
    //Abort if frame too short
    if (length <= 1) { return; }

    //Frame type
    frameType = data[0] & 0x0F;
    hopCount = (data[0] & 0xF0) >> 4;

    //Iterate frame and search for headers
    uint8_t header = 0;
    uint8_t payloadLength = 0;
    size_t i = 1;       //Position of 1st header
    while (i < length) {
        //Check header
        header = data[i] >> 4;
        payloadLength = data[i] & 0x0F;
        switch (header) {
            case Frame::HeaderTypes::NODE_CALL_HEADER:
                if (i + payloadLength < length) {
                    if (payloadLength > MAX_CALLSIGN_LENGTH) {payloadLength = MAX_CALLSIGN_LENGTH;}
                    safeUtf8Copy(nodeCall, (const uint8_t*)(data + i + 1), payloadLength, sizeof(nodeCall));
                    i += payloadLength + 1;
                } else {
                    i = length; //Abort
                }
                break;
            case Frame::HeaderTypes::VIA_CALL_HEADER:
                if (i + payloadLength < length) {
                    if (payloadLength > MAX_CALLSIGN_LENGTH) {payloadLength = MAX_CALLSIGN_LENGTH;}
                    safeUtf8Copy(viaCall, (const uint8_t*)(data + i + 1), payloadLength, sizeof(viaCall));
                    i += payloadLength + 1;
                } else {
                    i = length; //Abort
                }
                break;
            case Frame::HeaderTypes::SRC_CALL_HEADER:
                if (i + payloadLength < length) {
                    if (payloadLength > MAX_CALLSIGN_LENGTH) {payloadLength = MAX_CALLSIGN_LENGTH;}
                    safeUtf8Copy(srcCall, (const uint8_t*)(data + i + 1), payloadLength, sizeof(srcCall));
                    i += payloadLength + 1;
                } else {
                    i = length; //Abort
                }
                break;
            case Frame::HeaderTypes::DST_GROUP_HEADER:
                if (i + payloadLength < length) {
                    if (payloadLength > MAX_CALLSIGN_LENGTH) {payloadLength = MAX_CALLSIGN_LENGTH;}
                    safeUtf8Copy(dstGroup, (const uint8_t*)(data + i + 1), payloadLength, sizeof(dstGroup));
                    i += payloadLength + 1;
                } else {
                    i = length; //Abort
                }
                break;
            case Frame::HeaderTypes::DST_CALL_HEADER:
                if (i + payloadLength < length) {
                    if (payloadLength > MAX_CALLSIGN_LENGTH) {payloadLength = MAX_CALLSIGN_LENGTH;}
                    safeUtf8Copy(dstCall, (const uint8_t*)(data + i + 1), payloadLength, sizeof(dstCall));
                    i += payloadLength + 1;
                } else {
                    i = length; //Abort
                }
                break;
            case Frame::HeaderTypes::MESSAGE_HEADER:
                if (i + sizeof(id) < length) {
                    //Message Type 
                    messageType = payloadLength; //Used for dual purpose
                    i++;
                    //Message ID
                    memcpy(&id, &data[i], sizeof(id));  
                    i = i + sizeof(id);
                    //Message length
                    messageLength = length - i;
                    if (messageLength > sizeof(message)) messageLength = sizeof(message);
                    //Message
                    memcpy(message, data + i, messageLength);
                    //End of frame
                    i = length;
                } else {
                    i = length; //Abort
                }
                break;
            default: //Invalid header
                i = length;
                break;
        }      
    }

}