
#pragma once

#include "frame.h"

// Pin definitions for ESP32 E22; NANO-VHF V1; Rentnergang board as Meshtastic
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

#define PIN_WIFI_LED 25      //LED WiFi status (on = AP mode, blinking = client mode, off = not connected)
//#define PIN_AP_MODE_SWITCH 0     //Button WiFi Client/AP switch
#define PIN_HW_DEBUG 34

#define LORA_DEFAULT_TX_POWER 22
#define LORA_MAX_TX_POWER     22  // SX1262 hardware limit
#define WIFI_MAX_TX_POWER_DBM 20



void setWiFiLED(bool value);
void initHal();
bool checkReceive(Frame &f);
void transmitFrame(Frame &f);
bool getKeyApMode();

extern bool txFlag;
extern bool rxFlag;
