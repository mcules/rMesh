
#pragma once

#include "frame.h"

// Pin-Definitionen für HELTEC_Wireless_Stick_Lite_V3
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

#define PIN_VBAT_CTRL 37     //Akku-ADC aktivieren (LOW = Messung aktiv)
#define PIN_VBAT_ADC  1      //Akku-Spannungsmessung (ADC)
#define HAS_BATTERY_ADC


void setWiFiLED(bool value);
void initHal();
bool checkReceive(Frame &f);
void transmitFrame(Frame &f);
bool getKeyApMode();
float getBatteryVoltage();


extern bool txFlag;
extern bool rxFlag;

