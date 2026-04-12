
#pragma once

#include "mesh/frame.h"

// Pin definitions for T-Beam V1.1+
#define LORA_NSS    18
#define LORA_DIO0   26
#define LORA_RST    23
#define LORA_DIO1   33
#define SPI_SCK         5
#define SPI_MISO        19
#define SPI_MOSI        27
#define SPI_SS          18

#define LORA_DEFAULT_TX_POWER 20
#define LORA_MAX_TX_POWER     20  // SX1278 hardware limit
#define WIFI_MAX_TX_POWER_DBM 20

//#define PIN_WIFI_LED 25      //LED WiFi status (on = AP mode, blinking = client mode, off = not connected)
#define PIN_AP_MODE_SWITCH 38    //User button (GPIO38) for WiFi mode switch (long press) and display toggle (short press)


void setWiFiLED(bool value);
void initHal();
bool checkReceive(Frame &f);
void transmitFrame(Frame &f);
bool getKeyApMode();

extern bool txFlag;
extern bool rxFlag;
