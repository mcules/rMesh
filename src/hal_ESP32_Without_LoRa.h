
#pragma once

#include "frame.h"


#define PIN_WIFI_LED 2      //LED WiFi-Status (ein = AP-Mode, blinken = Client-Mode, aus = nicht verbunden)
#define PIN_AP_MODE_SWITCH 0     //Taster Umschaltung WiFi CLient/AP
#define LORA_DEFAULT_TX_POWER 0

void setWiFiLED(bool value);
void initHal();
bool checkReceive(Frame &f);
void transmitFrame(Frame &f);
bool getKeyApMode();

extern bool txFlag;
extern bool rxFlag;
