#pragma once

#include <Arduino.h>

#include "frame.h"



void printHexArray(uint8_t* data, size_t length);
void addJSONtoFile(char* buffer, size_t length, const char* file, const uint16_t lines);
uint32_t getTOA(uint8_t payloadBytes);
void sendMessage(const char* dst, const char* text, uint8_t messageType = Frame::MessageTypes::TEXT_MESSAGE); 
void sendGroup(const char* dst, const char* text, uint8_t messageType = Frame::MessageTypes::TEXT_MESSAGE); 
void safeUtf8Copy(char* dest, const uint8_t* src, size_t maxLength);
void getFormattedTime(const char* format, char* outBuffer, size_t outSize);
void sendFrame(Frame &f);

