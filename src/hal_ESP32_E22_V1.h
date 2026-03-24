
#pragma once

#include "frame.h"

// Pin-Definitionen für EP32 E22; NANO-VHF V1; Board Rentnergang als Meshtastic
#define LORA_NSS    18
#define LORA_BUSY   32
#define LORA_RST    23
#define LORA_DIO1   33
#define LORA_TX_ENA 13
#define LORA_RX_ENA 14
#define SPI_SCK         5
#define SPI_MISO        19
#define SPI_MOSI        27
#define SPI_SS          18

#define PIN_WIFI_LED 25      //LED WiFi-Status (ein = AP-Mode, blinken = Client-Mode, aus = nicht verbunden)
//#define PIN_AP_MODE_SWITCH 0     //Taster Umschaltung WiFi CLient/AP
#define PIN_HW_DEBUG 34

#define LORA_DEFAULT_TX_POWER 22



void setWiFiLED(bool value);
void initHal();
bool checkReceive(Frame &f);
void transmitFrame(Frame &f);
bool getKeyApMode();

extern bool txFlag;
extern bool rxFlag;
