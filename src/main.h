#pragma once

#include "config.h"
#include "frame.h"

struct MSG {
    char srcCall[MAX_CALLSIGN_LENGTH + 1] = {0};
    uint32_t id = 0;
};

extern uint32_t rebootTimer;
extern uint32_t statusTimer;
extern uint32_t announceTimer;
extern const char* TZ_INFO;
extern std::vector<Frame> txBuffer;
//extern portMUX_TYPE txBufferMux;

