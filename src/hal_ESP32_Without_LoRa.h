
#pragma once

#include "frame.h"


#define PIN_WIFI_LED 2      //LED WiFi status (on = AP mode, blinking = client mode, off = not connected)
#define PIN_AP_MODE_SWITCH 0     //Button WiFi Client/AP switch
#define LORA_DEFAULT_TX_POWER 0
#define WIFI_MAX_TX_POWER_DBM 20

void setWiFiLED(bool value);
void initHal();
bool checkReceive(Frame &f);
void transmitFrame(Frame &f);
bool getKeyApMode();

extern bool txFlag;
extern bool rxFlag;
