#pragma once

#include "frame.h"


struct Route {
    char viaCall[MAX_CALLSIGN_LENGTH + 1] = {0};
    char srcCall[MAX_CALLSIGN_LENGTH + 1] = {0};
    time_t timestamp = 0;
    float snr = 0;
    uint8_t hopCount = 0;
};



extern std::vector<Route> routingList;

void sendRoutingList();
void addRoutingList(const char* srcCall, const char* viaCall, uint8_t hopCount);
bool checkRoute(char* srcCall, char* viaCall);

