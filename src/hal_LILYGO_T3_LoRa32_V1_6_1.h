#pragma once

#include "frame.h"

//#define HW_TYPE "LILYGO_T3_LoRa32_V1_6_1"


// Pin-Definitionen für T3 V1.6.1
#define LORA_NSS    18
#define LORA_DIO0   26
#define LORA_RST    23
#define LORA_DIO1   33
#define SPI_SCK         5
#define SPI_MISO        19
#define SPI_MOSI        27
#define SPI_SS          18

#define PIN_WIFI_LED 25      //LED WiFi-Status (ein = AP-Mode, blinken = Client-Mode, aus = nicht verbunden)
//#define PIN_AP_MODE_SWITCH 0     //Taster Umschaltung WiFi CLient/AP


void setWiFiLED(bool value);
void initHal();
bool checkReceive(Frame &f);
void transmitFrame(Frame &f);




extern bool txFlag;
extern bool rxFlag;


/*
#ifndef RF_H
#define RF_H
#include <RadioLib.h>
#include "main.h"



extern bool transmittingFlag;
extern bool receivingFlag;
extern SX1278 radio;


void initRadio();
bool transmitFrame(Frame &f);


//void monitorFrame(Frame &f);




#endif
*/