
#pragma once

#include "frame.h"

// Pin definitions for HELTEC WiFi LoRa 32 V3
#define LORA_NSS    8
#define LORA_SCK    9
#define LORA_MOSI   10
#define LORA_MISO   11
#define LORA_DIO1   14
#define LORA_RST    12
#define LORA_BUSY   13

#define LORA_DEFAULT_TX_POWER 22
#define WIFI_MAX_TX_POWER_DBM 20

#define PIN_AP_MODE_SWITCH 0     //Button WiFi Client/AP switch
#define PIN_WIFI_LED 35      //LED WiFi status (on = AP mode, blinking = client mode, off = not connected)

#define PIN_VBAT_CTRL 37     //Enable battery ADC (LOW = measurement active)
#define PIN_VBAT_ADC  1      //Battery voltage measurement (ADC)
#define HAS_BATTERY_ADC


void setWiFiLED(bool value);
void initHal();
bool checkReceive(Frame &f);
void transmitFrame(Frame &f);
bool getKeyApMode();
float getBatteryVoltage();


extern bool txFlag;
extern bool rxFlag;

