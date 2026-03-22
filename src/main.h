#pragma once


#include "config.h"
#include "frame.h"

struct MSG {
    char srcCall[MAX_CALLSIGN_LENGTH + 1] = {0};
    uint32_t id = 0;
};

extern uint32_t rebootTimer;
extern bool pendingManualUpdate;
extern bool pendingForceUpdate;
extern uint8_t pendingForceChannel;
extern uint32_t statusTimer;
extern uint32_t announceTimer;
extern const char* TZ_INFO;
extern std::vector<Frame> txBuffer;
extern SemaphoreHandle_t fsMutex;
extern MSG messages[MAX_STORED_MESSAGES_RAM];
extern uint16_t messagesHead;
//extern portMUX_TYPE txBufferMux;

