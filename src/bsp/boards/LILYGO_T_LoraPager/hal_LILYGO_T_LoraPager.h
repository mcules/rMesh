
#pragma once

#include "mesh/frame.h"

// Pin definitions for LILYGO T-LoraPager (ESP32-S3 + SX1262)
// Source: https://github.com/Xinyuan-LilyGO/LilyGoLib/blob/master/docs/hardware/lilygo-t-lora-pager.md

// SPI bus (shared with display, SD, NFC)
#define LORA_SCK     35
#define LORA_MISO    33
#define LORA_MOSI    34

// SX1262 specific pins
#define LORA_NSS     36   // CS
#define LORA_RST     47
#define LORA_BUSY    48
#define LORA_DIO1    14   // IRQ

#define LORA_DEFAULT_TX_POWER 22
#define LORA_MAX_TX_POWER     22  // SX1262 hardware limit
#define WIFI_MAX_TX_POWER_DBM 20

// No dedicated WiFi LED on this board
// Boot button is GPIO0
#define PIN_AP_MODE_SWITCH 0


void setWiFiLED(bool value);
void initHal();
bool checkReceive(Frame &f);
void transmitFrame(Frame &f);
bool getKeyApMode();

// SD card helpers (no-ops when no card is inserted)
bool pagerSdAvailable();
void pagerAddMessageToSD(const char* json, size_t len);

extern bool txFlag;
extern bool rxFlag;
