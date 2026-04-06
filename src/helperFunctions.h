#pragma once

#include <Arduino.h>

#include "frame.h"

// struct FileWriteParams {
//     char* content;
//     size_t length;
//     String fileName;
//     uint16_t maxLines;
// };

struct FileWriteParams {
    char* content;
    size_t length;
    char fileName[32]; // Fixed reserved space for the name
    uint16_t maxLines;
};

void printHexArray(uint8_t* data, size_t length);
void addJSONtoFile(char* buffer, size_t length, const char* file, const uint16_t lines);
void initFileWriteWorker();
uint32_t getTOA(uint16_t payloadBytes);
void sendMessage(const char* dst, const char* text, uint8_t messageType = Frame::MessageTypes::TEXT_MESSAGE);
void sendGroup(const char* dst, const char* text, uint8_t messageType = Frame::MessageTypes::TEXT_MESSAGE);
void safeUtf8Copy(char* dest, const uint8_t* src, size_t srcLen, size_t dstSize);
void getFormattedTime(const char* format, char* outBuffer, size_t outSize);
void sendFrame(Frame &f);
void initPendingSendQueue();
void processPendingSends();
uint32_t calculateAckTime();
uint32_t calculateRetryTime();
void trimFile(const char* fileName, size_t maxLines);
