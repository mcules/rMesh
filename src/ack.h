#pragma once

#include <Arduino.h>
#include "config.h"


struct ACK {
    char srcCall[MAX_CALLSIGN_LENGTH + 1] = {0};
    char nodeCall[MAX_CALLSIGN_LENGTH + 1] = {0};
    uint32_t id = 0;
};

bool checkACK(const char* srcCall, const char* nodeCall, const uint32_t id);
void addACK(const char* srcCall, const char* nodeCall, const uint32_t id);


