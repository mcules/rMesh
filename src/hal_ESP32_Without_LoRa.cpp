#ifdef ESP32_WITHOUT_LORA

#include "hal.h"
#include "RadioLib.h"
#include "settings.h"
#include "frame.h"
#include "main.h"
#include "helperFunctions.h"
#include "webFunctions.h"
#include "logging.h"

bool txFlag = false;
bool rxFlag = false;

void setWiFiLED(bool value) {
    digitalWrite(PIN_WIFI_LED, value);
}

bool getKeyApMode() {
    return !digitalRead(PIN_AP_MODE_SWITCH);
}

void initHal() {
   //Outputs
    pinMode(PIN_WIFI_LED, OUTPUT);
    digitalWrite(PIN_WIFI_LED, 0);

    //Inputs
    pinMode(PIN_AP_MODE_SWITCH, INPUT);

    loraReady = false;  // No RF module present
}


bool checkReceive(Frame &f) {
    return false;
}

void transmitFrame(Frame &f) {
    txFlag = false;
    rxFlag = false;
}


#endif