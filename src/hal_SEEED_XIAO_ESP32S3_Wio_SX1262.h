
#pragma once

#include "frame.h"

// Pin definitions for Seeed XIAO ESP32-S3 + Wio-SX1262 (B2B connector)
// https://wiki.seeedstudio.com/wio_sx1262_with_xiao_esp32s3_for_lora/

#define LORA_NSS    41  // SPI chip select
#define LORA_DIO1   39  // IRQ / DIO1
#define LORA_RST    42  // Reset
#define LORA_BUSY   40  // BUSY
#define LORA_ANT_SW 38  // Antenna switch enable

#define LORA_SCK    7   // D8 – SPI clock
#define LORA_MISO   8   // D9 – SPI MISO
#define LORA_MOSI   9   // D10 – SPI MOSI

#define LORA_DEFAULT_TX_POWER 22
#define LORA_MAX_TX_POWER     22  // SX1262 hardware limit
#define WIFI_MAX_TX_POWER_DBM 20

// Built-in user LED on XIAO ESP32-S3 (HIGH = on)
#define PIN_WIFI_LED 21

// BOOT button (GPIO0) used as AP-mode switch (LOW = pressed)
#define PIN_AP_MODE_SWITCH 0


void setWiFiLED(bool value);
void initHal();
bool checkReceive(Frame &f);
void transmitFrame(Frame &f);
bool getKeyApMode();

extern bool txFlag;
extern bool rxFlag;
