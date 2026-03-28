
#pragma once

#include "frame.h"

// Pin-Definitionen für HELTEC HT-Tracker V1.2 (Wireless Tracker)
// ESP32-S3 + SX1262 + ST7735 TFT (80x160) + UC6580 GNSS

// ── LoRa SPI (SX1262) ──────────────────────────────────────────────────────
#define LORA_NSS    8
#define LORA_SCK    9
#define LORA_MOSI   10
#define LORA_MISO   11
#define LORA_DIO1   14
#define LORA_RST    12
#define LORA_BUSY   13

#define LORA_DEFAULT_TX_POWER 22
#define WIFI_MAX_TX_POWER_DBM 20

// ── TFT Display (ST7735S, 80x160) ──────────────────────────────────────────
#define TFT_SPI_MOSI 42
#define TFT_SPI_SCK  41
#define TFT_DC       40
#define TFT_RST      39
#define TFT_CS       38
#define TFT_BL       21

// ── Vext Power (controls TFT + GNSS, HIGH = on) ────────────────────────────
#define PIN_VEXT_CTRL 3

// ── User button & LED ───────────────────────────────────────────────────────
#define PIN_AP_MODE_SWITCH 0     //Taster Umschaltung WiFi Client/AP
#define PIN_WIFI_LED 18          //LED WiFi-Status

// ── Battery ADC ─────────────────────────────────────────────────────────────
#define PIN_VBAT_CTRL 37     //Akku-ADC aktivieren (HIGH = Messung aktiv)
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
