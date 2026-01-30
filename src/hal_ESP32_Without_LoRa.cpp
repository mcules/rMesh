#ifdef ESP32_WITHOUT_LORA

#include "hal.h"
#include "RadioLib.h"
#include "settings.h"
#include "frame.h"
#include "main.h"
#include "helperFunctions.h"
#include "webFunctions.h"

bool txFlag = false;
bool rxFlag = false;

void setWiFiLED(bool value) {
    //digitalWrite(PIN_WIFI_LED, value);
}

bool getKeyApMode() {
    //digitalRead(PIN_AP_MODE_SWITCH);
    return false;
}


void initHal() {

}


bool checkReceive(Frame &f) {
    return false;
}

void transmitFrame(Frame &f) {
    txFlag = false;
    rxFlag = false;
}


#endif