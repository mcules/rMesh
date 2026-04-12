#include "hal/hal.h"
#include "RadioLib.h"
#include "hal/settings.h"
#include "mesh/frame.h"
#include "main.h"
#include "util/helperFunctions.h"
#include "network/webFunctions.h"
#include "util/logging.h"

bool txFlag = false;
bool rxFlag = false;

void setWiFiLED(bool value) {
    if (!board->hasWiFiLED()) return;
    digitalWrite(board->pinWiFiLED(), board->wiFiLEDActiveLow() ? !value : value);
}

bool getKeyApMode() {
    if (!board->hasUserButton()) return false;
    return !digitalRead(board->pinUserButton());
}

void initHal() {
   //Outputs
    if (board->hasWiFiLED()) {
        pinMode(board->pinWiFiLED(), OUTPUT);
        digitalWrite(board->pinWiFiLED(), board->wiFiLEDActiveLow() ? HIGH : LOW);
    }

    //Inputs
    if (board->hasUserButton()) {
        pinMode(board->pinUserButton(), INPUT);
    }

    loraReady = false;  // No RF module present
}


bool checkReceive(Frame &f) {
    return false;
}

void transmitFrame(Frame &f) {
    txFlag = false;
    rxFlag = false;
}
