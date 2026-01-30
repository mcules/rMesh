
#pragma once

#include "frame.h"

// Pin-Definitionen für HELTEC_WiFi_LoRa_32_V4
#define LORA_NSS    8
#define LORA_SCK    9
#define LORA_MOSI   10
#define LORA_MISO   11
#define LORA_DIO1   14
#define LORA_RST    12
#define LORA_BUSY   13

#define LORA_DEFAULT_TX_POWER 22

#define PIN_AP_MODE_SWITCH 0     //Taster Umschaltung WiFi CLient/AP
#define PIN_WIFI_LED 35      //LED WiFi-Status (ein = AP-Mode, blinken = Client-Mode, aus = nicht verbunden)


void setWiFiLED(bool value);
void initHal();
bool checkReceive(Frame &f);
void transmitFrame(Frame &f);
bool getKeyApMode();


extern bool txFlag;
extern bool rxFlag;

